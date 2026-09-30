#include "Tools/CompoundTool.h"
#include "Tools/HutongPresetDefaults.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongGateRow.h"
#include "Generation/HutongMeshUtils.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"
#include "ContextObjectStore.h"
#include "ToolContextInterfaces.h"
#include "SceneManagement.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/ScopeExit.h"

using UE::Geometry::FDynamicMesh3;

#define LOCTEXT_NAMESPACE "HutongLayout"

namespace
{
	// Progress-dialog label per piece.
	FText PieceLabel(EHutongCompoundPiece Piece)
	{
		switch (Piece)
		{
		case EHutongCompoundPiece::MainHall:   return LOCTEXT("PieceMainHall", "Main Hall (正房)");
		case EHutongCompoundPiece::EarRoom:    return LOCTEXT("PieceEarRoom", "Ear Room (耳房)");
		case EHutongCompoundPiece::EarPassage: return LOCTEXT("PieceEarPassage", "Ear Room With Passage (耳房過道)");
		case EHutongCompoundPiece::SideHouse:  return LOCTEXT("PieceSideHouse", "Side House (廂房)");
		case EHutongCompoundPiece::FrontRow:   return LOCTEXT("PieceFrontRow", "Front Row (倒座房)");
		case EHutongCompoundPiece::RearRow:    return LOCTEXT("PieceRearRow", "Rear Row (後罩房)");
		case EHutongCompoundPiece::Passage:    return LOCTEXT("PiecePassage", "Covered Passage (過道)");
		case EHutongCompoundPiece::GateLodge:  return LOCTEXT("PieceGateLodge", "Gate Lodge (門房)");
		case EHutongCompoundPiece::GateHouse:  return LOCTEXT("PieceGateHouse", "Main Gate (大門)");
		case EHutongCompoundPiece::InnerGate:  return LOCTEXT("PieceInnerGate", "Inner Gate (垂花門)");
		case EHutongCompoundPiece::ScreenWall: return LOCTEXT("PieceScreenWall", "Screen Wall (影壁)");
		case EHutongCompoundPiece::Corridor:   return LOCTEXT("PieceCorridor", "Covered Corridor (遊廊)");
		case EHutongCompoundPiece::Path:       return LOCTEXT("PiecePath", "Paved Path (甬路)");
		case EHutongCompoundPiece::FlowerBed:  return LOCTEXT("PieceFlowerBed", "Flower Bed (花池)");
		case EHutongCompoundPiece::WaterJar:   return LOCTEXT("PieceWaterJar", "Water Jar (魚缸)");
		default:                               return LOCTEXT("PieceWall", "Wall (牆)");
		}
	}

	// Corridor the ring is built from.
	FHutongCorridorParams HutongRingCorridor(const FHutongCorridorParams& Base, double WalkWidth)
	{
		FHutongCorridorParams C = Base;
		C.bClosedSide = true;
		C.bBuildBackWall = false;
		// Every end abuts something; an overhang would reach into it.
		C.GableOverhang = 0.0;
		C.Width = FMath::Clamp(WalkWidth,
			FMath::Max(C.WalkWidthMin, 10.0), FMath::Max(C.WalkWidthMax, C.WalkWidthMin + 1.0));
		return C;
	}

	// Ridge height above the plot: what reads along a street front.
}

// 耳房過道: this court's ear room with the way to the 後院 cut through, at the plot-boundary end.
FHutongEarPassageParams HutongCompound::CourtEarPassage(const FHutongSiheyuanParams& Room,
	const FHutongPassageParams& Roof, double PassageWidth, bool bAtFarEnd)
{
	FHutongEarPassageParams P;
	P.Room = Room;
	P.Passage = Roof;
	P.PassageWidth = FMath::Max(PassageWidth, 120.0);
	P.bPassageAtFarEnd = bAtFarEnd;
	return P;
}

FHutongSiheyuanParams HutongCompound::Subordinate(const FHutongSiheyuanParams& Base, double MaxEave)
{
	FHutongSiheyuanParams P = Base;
	if (MaxEave <= 0.0 || P.GetEaveHeight() <= MaxEave) return P;
	// Taken off the bay rule, not clamped in it: 額枋, 中檻, sill are fractions of the eave and must drop together.
	P.bDeriveEaveFromBays = false;
	P.EaveHeight = MaxEave;
	return P;
}

FHutongSiheyuanParams HutongCompound::HeldAt(const FHutongSiheyuanParams& Base, double Eave)
{
	FHutongSiheyuanParams P = Base;
	if (Eave <= 0.0) return P;
	P.bDeriveEaveFromBays = false;
	P.EaveHeight = Eave;
	return P;
}

// 廂房 as built: panel preset plus what the court's walk asks.
FHutongSiheyuanParams HutongCompound::CourtWing(const FHutongSiheyuanParams& Base, EHutongCourtWalk Walk)
{
	FHutongSiheyuanParams P = OnCourtWalk(Base, Walk);
	if ((Walk != EHutongCourtWalk::WingVerandas && Walk != EHutongCourtWalk::Linked) || P.bHasFrontVeranda)
	{
		return P;
	}

	// 圖5-3-3 (left): 五檁, four 步架 with the 廊 first, on a 鑽金柱 — the rooms' depth plus a fifth for the 廊,
	// shared over the four. Was 七檁 with the 廊 added outside.
	const double Rooms = P.GetSuggestedDepth();
	P.bHasFrontVeranda = true;
	P.Purlins = EHutongPurlins::Five;
	if (Rooms > 0.0 && P.SuggestedDepth <= 0.0)
	{
		P.StepRun = 1.2 * Rooms / 4.0;
	}
	return P;
}

FHutongSiheyuanParams HutongCompound::OnCourtWalk(const FHutongSiheyuanParams& Base, EHutongCourtWalk Walk)
{
	FHutongSiheyuanParams P = Base;
	P.bHasVerandaEndDoorways = (Walk == EHutongCourtWalk::Linked);
	return P;
}

// 後檐 = what stands behind; for the hall row the plan says.
FHutongSiheyuanParams HutongCompound::CourtRow(const FHutongSiheyuanParams& Base,
	EHutongCompoundPlan Plan, EHutongBaySide Facing)
{
	FHutongSiheyuanParams P = Base;
	const bool bOntoRearCourt = (Plan == EHutongCompoundPlan::ThreeCourtyards)
		&& Facing == EHutongBaySide::MinusY;
	P.RearEave = bOntoRearCourt ? EHutongRearEave::Courtyard : EHutongRearEave::Lane;
	return P;
}

UHutongCompoundToolProperties::UHutongCompoundToolProperties()
{
	// Seeded from the canon house table, like the built-in presets, so compound and hand-placed 正房 match.
	MainHall = HutongPresets::MakeHouse(HutongCanon::House::MainHall);
	SmallMainHall = HutongPresets::MakeHouse(HutongCanon::House::MainHallSmall);
	EarRoom = HutongPresets::MakeHouse(HutongCanon::House::EarRoom);
	SideHouse = HutongPresets::MakeHouse(HutongCanon::House::SideHouseSmall);
	FrontRow = HutongPresets::MakeHouse(HutongCanon::House::FrontRow);
	RearRow = HutongPresets::MakeHouse(HutongCanon::House::RearRow);

	// 後罩房 backs onto the plot's north boundary, not a court; the preset's symmetrical eave is for between courts.
	RearRow.RearEave = EHutongRearEave::Lane;

	GateHouse.Style = EHutongGateStyle::Ruyi;
	// A terrace gate has a doorstep, not a forecourt.
	GateHouse.PlatformOverhang = 18.0;
	GateHouse.StepCount = 2;

	// Wall height and thickness come from its role.
	ApplyCourtSize();
}

void UHutongCompoundToolProperties::ApplyCourtSize()
{
	if (PlotSize == EHutongCompoundSize::Custom) return;

	// 院落寬大, 房子也隨之高大: court size sets the three buildings. Both halls seeded; MainHallFor picks one.
	const HutongCanon::Courtyard::FSize& S = HutongPresets::CourtSize(PlotSize);
	MainHall = HutongPresets::MakeCourtHall(S, /*bRearVeranda*/ true);
	SmallMainHall = HutongPresets::MakeCourtHall(S, /*bRearVeranda*/ false);
	// A size with its own hall depth builds it 七檁: 前廊 in front 步架, rear 步架 inside the back wall.
	// Not the five-purlin 前廊後無廊: it was shallower than the grand court's 倒座房.
	if (S.HallDepthCm > 0.0)
	{
		MainHall.bHasRearVeranda = S.bHallRearVeranda;
		MainHall.Purlins = EHutongPurlins::Seven;
		MainHall.StepRun = S.HallDepthCm / 6.0;
		MainHall.SuggestedDepth = 0.0;
	}
	EarRoom = HutongPresets::MakeCourtEarRoom(S);
	if (S.EarRoomDepthCm > 0.0)
	{
		EarRoom.Purlins = EHutongPurlins::Five;
		EarRoom.StepRun = S.EarRoomDepthCm / 4.0;
		EarRoom.SuggestedDepth = 0.0;
	}
	SideHouse = HutongPresets::MakeCourtWing(S);
	// 廂房進深 5.5 m 含外廊 (大型), 3.5-4 m 無外廊 (小型): the walk belongs to the size.
	// With 廂房 前廊 the court is walked under cover (Fig 2-9.1); 抄手遊廊 joins them to 垂花門 and 正房.
	CourtWalk = S.bWingVeranda ? EHutongCourtWalk::Linked : EHutongCourtWalk::None;

	// Street row: table 倒座房 or the size's depth (七檁, keeps its 步架); the 大門 in it takes the row's 進深.
	FrontRow = HutongPresets::MakeHouse(HutongCanon::House::FrontRow);
	if (S.FrontRowDepthCm > 0.0)
	{
		const bool bSeven = S.FrontRowDepthCm >= 560.0;
		FrontRow.Purlins = bSeven ? EHutongPurlins::Seven : EHutongPurlins::Five;
		FrontRow.StepRun = S.FrontRowDepthCm / (bSeven ? 6.0 : 4.0);
	}
	GateHouse.Style = S.bGrandGate ? EHutongGateStyle::Guangliang : EHutongGateStyle::Ruyi;

	// Grand court: 一殿一卷 垂花門; the smaller two: single-post.
	InnerGate.Style = S.bGrandInnerGate ? EHutongInnerGateStyle::OneHallOneRoll : EHutongInnerGateStyle::SinglePost;
	// 攢邊門 on 抱鼓石 (Fig 5-2-2); small gate keeps blocks.
	InnerGate.DoorStones.Style = S.bGrandInnerGate ? EHutongDoorStone::Drum : EHutongDoorStone::Block;

	// Ordinary depths of this size's three courts.
	Courtyard.Y = S.CourtDepthCm;
	TypicalOuterCourtDepth = S.OuterCourtDepthCm;
	TypicalRearCourtDepth = S.RearCourtDepthCm;

	// 院當寬度: plot width minus both 廂房, walls and gaps. Stored, not derived at layout, so the panel shows it.
	const double Wing = HutongCompound::CourtWing(SideHouse, CourtWalk).GetSuggestedDepth();
	const double Court = FMath::Max(
		S.PlotWidthCm - 2.0 * (Wing + HutongCanon::Wall::PerimeterThicknessCm + Gap), 300.0);
	Courtyard.X = Court;
	MinCourtyard.X = 0.92 * Court;
}

void UHutongCompoundToolProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// Only the size reseeds the buildings; later edits to one must stick.
	const FName Changed = PropertyChangedEvent.GetPropertyName();
	if (Changed == GET_MEMBER_NAME_CHECKED(UHutongCompoundToolProperties, PlotSize))
	{
		ApplyCourtSize();
	}
}

UInteractiveTool* UHutongCompoundToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongCompoundTool>(SceneState.ToolManager);
}

void UHutongCompoundTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongCompoundToolProperties>(this);

	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("Compound"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongCompoundToolProperties, MainHall));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	// Presets after the parameters they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

void UHutongCompoundTool::PlotSizeForCursor(const FVector2D& LocalCursor, double& OutW, double& OutD) const
{
	double MinW, MinD, WantW, WantD;
	MakeInput(1.0, 1.0).GetMinimumPlot(MinW, MinD);
	GetSuggestedPlot(WantW, WantD);

	// Stamped size is not dragged: width is the court's, depth is the plan's.
	if (Settings && Settings->PlotSize != EHutongCompoundSize::Custom)
	{
		OutW = FMath::Max(HutongPresets::CourtSize(Settings->PlotSize).PlotWidthCm, MinW);
		OutD = FMath::Max(WantD, MinD);
		return;
	}

	// Anchor = southeast corner, cursor = northwest.
	constexpr double SnapBand = 50.0;
	auto Axis = [](double Cursor, double Least, double Want)
	{
		double Size = FMath::Max(Cursor, Least);
		if (FMath::Abs(Size - Want) < SnapBand) Size = FMath::Max(Want, Least);
		return Size;
	};
	OutW = Axis(LocalCursor.X, MinW, WantW);
	OutD = Axis(LocalCursor.Y, MinD, WantD);
}

void UHutongCompoundTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	double W, D;
	PlotSizeForCursor(WorldXYToLocalRect(CurrentWorld), W, D);
	OutMinX = 0.0;
	OutMinY = 0.0;
	OutMaxX = W;
	OutMaxY = D;
}

void UHutongCompoundTool::ApplyNorthOrientation()
{
	if (!Settings) return;

	// Plot frame: street at local -Y, 正房 toward +Y.
	PlacementYawDeg = Settings->NorthYawDeg - 90.0;
}

void UHutongCompoundTool::OnPlacementStarted(const FVector& HitWorld)
{
	ApplyNorthOrientation();
}

void UHutongCompoundTool::OnPlacementHover(const FVector& HitWorld)
{
	ApplyNorthOrientation();
}

FText UHutongCompoundTool::GetStagePromptText() const
{
	const bool bStamped = Settings && Settings->PlotSize != EHutongCompoundSize::Custom;
	if (!bIsDragging)
	{
		const FText Plan = GetPlanEditPromptText();
		if (!Plan.IsEmpty()) return Plan;
		return bStamped
			? LOCTEXT("PromptSECornerStamped",
				"Click the ground to set the southeast corner (巽位), where the gate goes. The plot is stamped at the chosen size; pick Custom to drag one.")
			: LOCTEXT("PromptSECorner",
				"Click the ground to set the southeast corner (巽位), where the gate goes. The plan shown is the ordinary plot.");
	}
	return bStamped
		? LOCTEXT("PromptStamped", "Click to lay the plot out at its stamped size. Hold R to turn it, Esc to cancel.")
		: LOCTEXT("PromptSize",
			"Move northwest to size the plot, then click to lay it out. It will not go below the smallest plot these settings can be spaced on. Esc to cancel.");
}

TArray<FText> UHutongCompoundTool::GetStageNames() const
{
	return { LOCTEXT("StageCorner", "SE corner"), LOCTEXT("StageSize", "Size") };
}

void UHutongCompoundTool::AlignBaseCourse(FHutongSiheyuanParams& P,
	EHutongBaySide Facing, double SizeX, double SizeY) const
{
	if (!Settings) return;

	// Footprint first.
	const bool bAlongX = HutongGen::BaySide::IsAlongX(Facing);
	P.Width = FMath::Max(bAlongX ? SizeX : SizeY, 1.0);
	P.Depth = FMath::Max(bAlongX ? SizeY : SizeX, 1.0);

	// Wall band starts at ground; a building's on top of its 臺基.
	P.BaseCourseHeight = FMath::Max(Settings->BaseCourseTop - P.GetFloorHeight(), 25.0);
}

HutongGen::FCompoundInput UHutongCompoundTool::MakeInput(double SizeX, double SizeY) const
{
	return MakeInputFrom(Settings, SizeX, SizeY);
}

void UHutongCompoundTool::GetSuggestedPlot(double& OutW, double& OutD) const
{
	OutW = OutD = 0.0;
	if (!Settings) return;
	// Stamped court suggests with its built hall; dragged with the larger.
	const FHutongSiheyuanParams& Hall = (Settings->PlotSize != EHutongCompoundSize::Custom)
		? MainHallFor(Settings, 1.0, 1.0) : Settings->MainHall;
	MakeInputWithHall(Settings, Hall, 1.0, 1.0).GetSuggestedPlot(OutW, OutD);
}

void UHutongCompoundTool::GetStampedPlot(const UHutongCompoundToolProperties* S, double& OutW, double& OutD)
{
	OutW = OutD = 0.0;
	if (!S) return;
	double MinW, MinD, WantW, WantD;
	MakeInputFrom(S, 1.0, 1.0).GetMinimumPlot(MinW, MinD);
	const FHutongSiheyuanParams& Hall = (S->PlotSize != EHutongCompoundSize::Custom) ? MainHallFor(S, 1.0, 1.0) : S->MainHall;
	MakeInputWithHall(S, Hall, 1.0, 1.0).GetSuggestedPlot(WantW, WantD);
	OutW = (S->PlotSize != EHutongCompoundSize::Custom)
		? FMath::Max(HutongPresets::CourtSize(S->PlotSize).PlotWidthCm, MinW) : FMath::Max(WantW, MinW);
	OutD = FMath::Max(WantD, MinD);
}

double UHutongCompoundTool::CourtWalkWidth(const UHutongCompoundToolProperties* S, double* OutShift)
{
	if (OutShift) *OutShift = 0.0;
	if (!S) return HutongCanon::Compound::CorridorWalkWidthCm;
	if (S->CourtWalk != EHutongCourtWalk::Linked) return S->CorridorWalkWidth;

	// 廂房 as the court builds it, at its suggested footprint.
	FHutongSiheyuanParams Wing = HutongCompound::CourtWing(S->SideHouse, S->CourtWalk);
	Wing.Depth = FMath::Max(Wing.GetSuggestedDepth(), 200.0);
	Wing.Width = FMath::Max(Wing.SuggestedFrontage, 300.0);
	double FY = 0.0, RY = 0.0;
	Wing.GetBuiltVerandaDepths(Wing.Depth, Wing.WallThickness, FY, RY);
	const double ColR = Wing.GetColumnRadius();
	if (FY <= 2.0 * ColR) return S->CorridorWalkWidth;

	// 檐柱 axis to 金柱 axis: the corridor's walk measure.
	const FHutongCorridorParams C = HutongRingCorridor(S->Corridor, FY - ColR);
	if (OutShift) *OutShift = FMath::Max(C.FloorOverhang, 0.0) + C.GetColumnRadius() - ColR;
	return C.Width;
}

const FHutongSiheyuanParams& UHutongCompoundTool::MainHallFor(const UHutongCompoundToolProperties* S, double SizeX, double SizeY)
{
	// Stamped: 七檁前後廊 (大型), 前廊後無廊 (smaller two) (四合院建築及其構造 p.85). Only a dragged plot is measured.
	if (S->PlotSize != EHutongCompoundSize::Custom)
	{
		// The size's own hall if named; else 前後廊 vs 前廊後無廊.
		const HutongCanon::Courtyard::FSize& Size = HutongPresets::CourtSize(S->PlotSize);
		return (Size.bHallRearVeranda || Size.HallDepthCm > 0.0) ? S->MainHall : S->SmallMainHall;
	}

	double MinW, MinD;
	MakeInputWithHall(S, S->MainHall, 1.0, 1.0).GetMinimumPlot(MinW, MinD);
	return (SizeX >= MinW - 1.0 && SizeY >= MinD - 1.0) ? S->MainHall : S->SmallMainHall;
}

HutongGen::FCompoundInput UHutongCompoundTool::MakeInputFrom(const UHutongCompoundToolProperties* Settings, double SizeX, double SizeY)
{
	if (!Settings)
	{
		HutongGen::FCompoundInput In;
		In.Width = SizeX;
		In.Depth = SizeY;
		return In;
	}
	return MakeInputWithHall(Settings, MainHallFor(Settings, SizeX, SizeY), SizeX, SizeY);
}

HutongGen::FCompoundInput UHutongCompoundTool::MakeInputWithHall(const UHutongCompoundToolProperties* Settings,
	const FHutongSiheyuanParams& Hall, double SizeX, double SizeY)
{
	HutongGen::FCompoundInput In;
	In.Width = SizeX;
	In.Depth = SizeY;
	if (!Settings) return In;

	In.Plan = Settings->Plan;
	In.Gap = Settings->Gap;
	In.bGateAtEastEnd = Settings->bGateAtEastEnd;
	In.CourtWalk = Settings->CourtWalk;
	In.bHasPath = Settings->bHasPath;
	In.bHasScreenWall = Settings->bHasScreenWall;
	In.bHasOuterYard = Settings->bHasOuterYard;
	In.OuterYardWidth = FMath::Max(Settings->OuterYardWidth, 250.0);

	// Depths from each building's suggested footprint.
	In.HallDepth = FMath::Max(Hall.GetSuggestedDepth(), 200.0);
	// Wing as actually built.
	In.WingDepth = FMath::Max(
		HutongCompound::CourtWing(Settings->SideHouse, Settings->CourtWalk).GetSuggestedDepth(), 200.0);
	// 正房三間兩耳: hall frontage in the middle, 耳房 at the ends, depth from their own 檁數.
	In.HallFrontage = FMath::Max(Hall.SuggestedFrontage, 400.0);
	In.EarRoomDepth = FMath::Max(Settings->EarRoom.GetSuggestedDepth(), 150.0);
	In.bHasEarRooms = Settings->bHasEarRooms;
	In.MinEarRoomFrontage = FMath::Max(1.05 * Settings->EarRoom.MinBayWidth, 150.0);
	In.WingFrontage = FMath::Max(Settings->SideHouse.SuggestedFrontage, 300.0);
	// A 廂房 is three bays, not the court's length; the remainder is the 小天井 at its north end (Fig 2-9.1).
	In.MaxWingFrontage = In.WingFrontage;
	// 廂耳房 = one room of the size's 耳房 bay, not the hall's pair.
	if (Settings->PlotSize != EHutongCompoundSize::Custom)
	{
		In.WingEarRoomFrontage = HutongPresets::CourtSize(Settings->PlotSize).EarRoomBayCm;
	}

	In.bHasWingEarRooms = Settings->bHasWingEarRooms;
	In.FrontRowDepth = FMath::Max(Settings->FrontRow.GetSuggestedDepth(), 180.0);
	In.RearRowDepth = FMath::Max(Settings->RearRow.GetSuggestedDepth(), 180.0);
	In.RearCourtDepth = FMath::Max(Settings->RearCourtDepth, 150.0);
	In.PassageWidth = FMath::Max(Settings->PassageWidth, 120.0);
	In.PassageBearing = FMath::Clamp(Settings->Passage.Bearing, 1.0, 30.0);

	const FHutongGateHouseParams::FSizeRange GR = Settings->GateHouse.GetSizeRange();
	In.GateFrontage = (GR.FrontageMax > 0.0) ? 0.5 * (GR.FrontageMin + GR.FrontageMax) : 360.0;
	// 進深 is the row's: a 大門 is one bay of the street face. The style's standalone depth left it 60 cm shallow in a row.
	In.GateDepth = In.FrontRowDepth;

	const FHutongInnerGateParams::FSizeRange IR = Settings->InnerGate.GetSizeRange();
	In.InnerGateFrontage = (IR.FrontageMax > 0.0) ? 0.5 * (IR.FrontageMin + IR.FrontageMax) : 330.0;
	In.InnerGateDepth = (IR.DepthMax > 0.0) ? 0.5 * (IR.DepthMin + IR.DepthMax) : 140.0;
	{
		// Gate's position in the cross wall and its rear flight's reach.
		FHutongInnerGateParams G = Settings->InnerGate;
		G.Depth = In.InnerGateDepth;
		In.InnerGateWallAt = G.GetWallLineY();
		// Walk floor resolves only at spawn; reserve a flight for the hall floor (under 80 cm).
		if (Settings->CourtWalk == EHutongCourtWalk::Linked || Settings->CourtWalk == EHutongCourtWalk::Corridor)
		{
			G.FloorHeight = FMath::Max(G.FloorHeight, 80.0);
		}
		In.InnerGateRearReach = FMath::Max(G.PlatformOverhang, G.GetColumnRadius())
			+ FMath::Max(G.GetStepCount(), 0) * FMath::Max(G.StepTread, 0.0);
	}

	// Minimum 廂房 length.
	In.MinWingFrontage = FMath::Max(2.0 * Settings->SideHouse.MinBayWidth,
		0.62 * Settings->SideHouse.SuggestedFrontage);
	In.MinCourtyardWidth = FMath::Max(Settings->MinCourtyard.X, 200.0);
	In.MinCourtyardDepth = FMath::Max(Settings->MinCourtyard.Y, 200.0);
	// Held at or above minimum.
	In.CourtyardWidth = FMath::Max(Settings->Courtyard.X, In.MinCourtyardWidth);
	In.CourtyardDepth = FMath::Max(Settings->Courtyard.Y, In.MinCourtyardDepth);
	In.TypicalOuterCourtDepth = FMath::Max(Settings->TypicalOuterCourtDepth, In.OuterCourtDepth);
	In.TypicalRearCourtDepth = FMath::Max(Settings->TypicalRearCourtDepth, In.RearCourtDepth);

	In.bHasGateLodge = Settings->bHasGateLodge;
	In.GateLodgeFrontage = FMath::Max(Settings->GateLodgeFrontage, 150.0);
	In.ScreenLength = FMath::Max(Settings->ScreenWallLength, 150.0);
	// Larger of the screen's two end overhangs.
	In.ScreenSideProjection = FMath::Max(Settings->ScreenWall.GableOverhang,
		Settings->ScreenWall.PlinthProjection);
	// Both through the role, from one params struct.
	{
		FHutongWallParams Perimeter = Settings->Wall;
		Perimeter.Role = EHutongWallRole::Perimeter;
		In.WallThickness = Perimeter.GetThickness();

		FHutongWallParams Courtyard = Settings->Wall;
		Courtyard.Role = EHutongWallRole::Courtyard;
		In.CourtyardWallThickness = Courtyard.GetThickness();
	}
	// Measured on the corridor actually built.
	In.CorridorWalkWidth = CourtWalkWidth(Settings, &In.LinkShift);
	In.CorridorDepth = HutongRingCorridor(Settings->Corridor, In.CorridorWalkWidth)
		.GetFootprintDepth();
	In.PathWidth = FMath::Max(Settings->Path.WidthMin, 60.0);
	In.bHasCourtyardFurnishing = Settings->bHasCourtyardFurnishing;
	In.WaterJarSpan = Settings->WaterJar.GetFootprint();
	In.FlowerBedSizeX = Settings->FlowerBedSizeX;
	In.FlowerBedSizeY = Settings->FlowerBedSizeY;
	In.ScreenDepth = Settings->ScreenWall.GetFootprintDepth();
	In.ScreenPlinthProjection = FMath::Max(Settings->ScreenWall.PlinthProjection, 0.0);
	return In;
}

// Name from the slot, not the build: plan-only placements never run the build lambda.
static FString SlotNameBase(EHutongCompoundPiece Piece)
{
	switch (Piece)
	{
	case EHutongCompoundPiece::MainHall: return TEXT("Hutong_Zhengfang");
	case EHutongCompoundPiece::EarRoom: return TEXT("Hutong_Erfang");
	case EHutongCompoundPiece::EarPassage: return TEXT("Hutong_Erfang_Guodao");
	case EHutongCompoundPiece::SideHouse: return TEXT("Hutong_Xiangfang");
	case EHutongCompoundPiece::FrontRow: return TEXT("Hutong_Daozuofang");
	case EHutongCompoundPiece::RearRow: return TEXT("Hutong_Houzhaofang");
	case EHutongCompoundPiece::Passage: return TEXT("Hutong_Guodao");
	case EHutongCompoundPiece::GateLodge: return TEXT("Hutong_Menfang");
	case EHutongCompoundPiece::GateHouse: return TEXT("Hutong_Gate");
	case EHutongCompoundPiece::InnerGate: return TEXT("Hutong_InnerGate");
	case EHutongCompoundPiece::ScreenWall: return TEXT("Hutong_Screen");
	case EHutongCompoundPiece::Corridor: return TEXT("Hutong_Corridor");
	case EHutongCompoundPiece::FlowerBed: return TEXT("Hutong_FlowerBed");
	case EHutongCompoundPiece::WaterJar: return TEXT("Hutong_WaterJar");
	case EHutongCompoundPiece::Path: return TEXT("Hutong_Path");
	case EHutongCompoundPiece::Wall:
	default: return TEXT("Hutong_Wall");
	}
}

HutongCompound::FSlotParams UHutongCompoundTool::MakeSlotParams(const FHutongCompoundSlot& Slot) const
{
	HutongCompound::FSlotParams P;
	if (!Settings) return P;

	const double SX = Slot.Size.X;
	const double SY = Slot.Size.Y;
	const double Run = Slot.bLengthAlongY ? SY : SX;

	const FHutongSiheyuanParams* Hall = PlacedMainHall ? PlacedMainHall : &Settings->MainHall;
	// 耳房 held under the hall's eave, however wide their slot.
	const double EarCap = (HallEaveZ > 0.0)
		? HallEaveZ - HutongCanon::Compound::EarRoomBelowHallCm : 0.0;

	// Perimeter courses aligned before building.
	auto Aligned = [&](const FHutongSiheyuanParams& From)
	{
		FHutongSiheyuanParams A = From;
		AlignBaseCourse(A, Slot.Facing, SX, SY);
		return A;
	};

	switch (Slot.Piece)
	{
	case EHutongCompoundPiece::MainHall:
		P.House = Aligned(HutongCompound::OnCourtWalk(
			HutongCompound::CourtRow(*Hall, Settings->Plan, Slot.Facing), Settings->CourtWalk));
		break;
	case EHutongCompoundPiece::EarRoom:
	{
		// Hall's ears face the court and share its eave; a 廂耳房 is only held under.
		const FHutongSiheyuanParams Ear = Aligned(HutongCompound::CourtRow(Settings->EarRoom, Settings->Plan, Slot.Facing));
		P.House = (Slot.Facing == EHutongBaySide::MinusY)
			? HutongCompound::HeldAt(Ear, EarCap) : HutongCompound::Subordinate(Ear, EarCap);
		break;
	}
	case EHutongCompoundPiece::SideHouse:
	{
		FHutongSiheyuanParams Wing = HutongCompound::CourtWing(Settings->SideHouse, Settings->CourtWalk);
		// Its 前廊 is part of the walk: stands at the walk's floor.
		if (WalkFloorZ > 0.0) Wing.FloorHeightHeldAt = WalkFloorZ;
		P.House = Aligned(Wing);
		break;
	}
	case EHutongCompoundPiece::RearRow:
		P.House = Aligned(Settings->RearRow);
		break;
	// 門房 = street row continuing past the gate.
	case EHutongCompoundPiece::FrontRow:
	case EHutongCompoundPiece::GateLodge:
		P.House = Aligned(Settings->FrontRow);
		break;
	default:
		break;
	}

	P.Gate = Settings->GateHouse;
	P.Gate.BaseCourseHeight = FMath::Max(Settings->BaseCourseTop - P.Gate.FloorHeight, 25.0);
	// 大門 rises above its row.
	HutongGen::GateRow::LiftGateAboveRidge(P.Gate,
		HutongGen::BaySide::IsAlongX(Slot.Facing) ? SY : SX,
		StreetRowRidgeZ, Settings->GateRidgeClearance);

	// Compound owns the role: only the layout knows perimeter vs crossing.
	P.Wall = Settings->Wall;
	P.Wall.Role = Slot.WallRole;
	// Compound owns doorways, as it owns roles.
	HutongGen::ApplySlotDoorway(P.Wall, Slot, Run);
	// Only the perimeter aligns: BaseCourseTop keeps the band unbroken round the plot's *outside*.
	P.Wall.BaseCourseHeight = (Slot.WallRole == EHutongWallRole::Perimeter)
		? FMath::Clamp(Settings->BaseCourseTop, 10.0, P.Wall.GetHeight() * 0.6)
		: FMath::Clamp(P.Wall.GetHeight() / 3.0, 10.0, P.Wall.GetHeight() * 0.6);

	// 過道 roof lands on the closing 隔牆.
	{
		FHutongWallParams CrossWall = Settings->Wall;
		CrossWall.Role = EHutongWallRole::Courtyard;
		P.PassageRoof = Settings->Passage;
		P.PassageRoof.EaveHeight = FMath::Max(CrossWall.GetHeight(), 120.0);
	}

	// 耳房過道: this flank's ear room with the way through, at the plot-boundary end.
	if (Slot.Piece == EHutongCompoundPiece::EarPassage)
	{
		const FHutongSiheyuanParams Room = HutongCompound::HeldAt(
			Aligned(HutongCompound::CourtRow(Settings->EarRoom, Settings->Plan, Slot.Facing)), EarCap);
		P.EarPassage = HutongCompound::CourtEarPassage(Room, P.PassageRoof, Settings->PassageWidth,
			/*bAtFarEnd*/ Slot.Min.X > 1.0);
	}

	// Ring's params and walk width, not the panel's.
	P.Corridor = HutongRingCorridor(Settings->Corridor, CourtWalkWidth(Settings));
	P.Corridor.BenchGapAt = Slot.CorridorBenchGapAt;
	P.Corridor.bOmitLowEndPost = Slot.bOmitLowEndPost;
	P.Corridor.bOmitHighEndPost = Slot.bOmitHighEndPost;
	P.Corridor.bNoBenchAtLowEnd = Slot.bNoBenchAtLowEnd;
	P.Corridor.bNoBenchAtHighEnd = Slot.bNoBenchAtHighEnd;
	// On the walk floor, lifted so clear height under the eave stays the corridor's.
	if (WalkFloorZ > 0.0)
	{
		const double Lift = WalkFloorZ - P.Corridor.FloorHeight;
		P.Corridor.FloorHeight = WalkFloorZ;
		P.Corridor.EaveHeight += FMath::Max(Lift, 0.0);
	}

	// Under a 遊廊 roof: up to its eave; the roof is its cap.
	if (Slot.bUnderEave)
	{
		P.Wall.PinHeight(P.Corridor.GetEaveHeight());
		P.Wall.CapSlabHeight = 0.0;
		P.Wall.CapRidgeHeight = 0.0;
	}

	// 垂花門 opens onto the walk at the same floor.
	P.InnerGate = Settings->InnerGate;
	if (WalkFloorZ > 0.0)
	{
		// Door head and eave are heights above ground: lift with the floor or the doorway shrinks.
		const double Lift = FMath::Max(WalkFloorZ - P.InnerGate.FloorHeight, 0.0);
		P.InnerGate.FloorHeight = WalkFloorZ;
		P.InnerGate.bFlushSides = true;
		P.InnerGate.DoorHeadHeight += Lift;
		P.InnerGate.EaveHeight += Lift;
	}

	// Sized to the reserved slot, so the drawn jar is the built jar.
	P.WaterJar = Settings->WaterJar;
	P.WaterJar.BellyDiameter = FMath::Min(SX, SY);
	return P;
}

void UHutongCompoundTool::BuildSlotMesh(const FHutongCompoundSlot& Slot, const HutongCompound::FSlotParams& P,
	FDynamicMesh3& Mesh, EHutongDetail Level) const
{
	if (!Settings) return;
	const double SX = Slot.Size.X;
	const double SY = Slot.Size.Y;
	// Slot decides a line-like piece's direction.
	const bool bAlongY = Slot.bLengthAlongY;
	const double Run = bAlongY ? SY : SX;     // along the piece's length
	const double Cross = bAlongY ? SX : SY;   // across it

	// Same static entry points as the individual tools.
	switch (Slot.Piece)
	{
	case EHutongCompoundPiece::MainHall:
	case EHutongCompoundPiece::EarRoom:
	case EHutongCompoundPiece::SideHouse:
	case EHutongCompoundPiece::FrontRow:
	case EHutongCompoundPiece::RearRow:
	case EHutongCompoundPiece::GateLodge:
		UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(P.House, Slot.Facing, 0, SX, SY, Mesh, Level);
		break;
	case EHutongCompoundPiece::EarPassage:
		UHutongEarPassageBuildingComponent::BuildEarPassageMesh(P.EarPassage, Slot.Facing, SX, SY, Mesh, Level);
		break;
	case EHutongCompoundPiece::Passage:
		UHutongPassageBuildingComponent::BuildPassageMesh(
			P.PassageRoof, Run, Cross - 2.0 * P.PassageRoof.Bearing, bAlongY, Mesh, Level);
		break;
	case EHutongCompoundPiece::GateHouse:
		UHutongGateHouseBuildingComponent::BuildGateHouseMesh(P.Gate, Slot.Facing, SX, SY, Mesh, Level);
		break;
	case EHutongCompoundPiece::InnerGate:
		UHutongInnerGateBuildingComponent::BuildInnerGateMesh(
			P.InnerGate, Slot.Facing, SX, SY, Mesh, Level);
		break;
	case EHutongCompoundPiece::ScreenWall:
		UHutongScreenWallBuildingComponent::BuildScreenWallMesh(
			Settings->ScreenWall, Run, bAlongY, Mesh, Level);
		break;
	case EHutongCompoundPiece::Corridor:
		UHutongCorridorBuildingComponent::BuildCorridorMesh(
			P.Corridor, Run, bAlongY, Slot.bFlipOpenSide, Mesh, Level);
		break;
	case EHutongCompoundPiece::FlowerBed:
		UHutongFlowerBedBuildingComponent::BuildFlowerBedMesh(Settings->FlowerBed, SX, SY, Mesh, Level);
		break;
	case EHutongCompoundPiece::WaterJar:
		UHutongWaterJarBuildingComponent::BuildWaterJarMesh(P.WaterJar, Mesh, Level);
		break;
	case EHutongCompoundPiece::Path:
		UHutongPathBuildingComponent::BuildPathMesh(
			Settings->Path, Run, Settings->Path.WidthFromFootprint(Cross), bAlongY, Mesh, Level);
		break;
	case EHutongCompoundPiece::Wall:
	default:
		UHutongWallBuildingComponent::BuildWallMesh(P.Wall, Run, Cross, bAlongY, Mesh, Level);
		break;
	}

	// Matches UHutongBuildingComponent::BuildLODs, so placement and rebuild agree.
	if (!Slot.Skew.IsZero()) HutongMeshUtils::WarpFootprint(Mesh, SX, SY, Slot.Skew);
}

void UHutongCompoundTool::BuildCompound(double SizeX, double SizeY, EHutongDetail Level,
	TArray<HutongCompound::FBuiltSlot>& Out) const
{
	Out.Reset();
	if (!Settings) return;
	const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(MakeInput(SizeX, SizeY));
	ResolveStreetRowRidge(Slots);
	ResolveHallEave(Slots, SizeX, SizeY);
	PlacedMainHall = &MainHallFor(Settings, SizeX, SizeY);
	ON_SCOPE_EXIT { PlacedMainHall = nullptr; };

	for (const FHutongCompoundSlot& Slot : Slots)
	{
		HutongCompound::FBuiltSlot& B = Out.AddDefaulted_GetRef();
		B.Slot = Slot;
		BuildSlotMesh(Slot, MakeSlotParams(Slot), B.Mesh, Level);
		// Spawn placement: slot corner, no turn in plot frame.
		HutongMeshUtils::TransformVerticesFrom(B.Mesh, 0, FTransform(FVector(Slot.Min.X, Slot.Min.Y, 0.0)));
	}
}

AStaticMeshActor* UHutongCompoundTool::SpawnSlot(UWorld* World, const FHutongCompoundSlot& Slot,
	double MinX, double MinY) const
{
	if (!Settings) return nullptr;

	const double SX = Slot.Size.X;
	const double SY = Slot.Size.Y;
	const bool bAlongY = Slot.bLengthAlongY;
	const double Run = bAlongY ? SY : SX;
	const double Cross = bAlongY ? SX : SY;

	const FString NameBase = SlotNameBase(Slot.Piece);
	const HutongCompound::FSlotParams P = MakeSlotParams(Slot);
	auto BuildAt = [&](FDynamicMesh3& Mesh, EHutongDetail Level) { BuildSlotMesh(Slot, P, Mesh, Level); };

	TArray<FDynamicMesh3> LODs;
	const int32 CollisionLOD = HutongGen::Detail::BuildPlacementLODs(IsPlanOnly(), SX, SY,
		GetDetailLevel(), ShouldBuildLODChain(), BuildAt, LODs);
	const bool bPlanOnly = IsPlanOnly();
	if (!bPlanOnly && (LODs.Num() == 0 || LODs[0].TriangleCount() == 0)) return nullptr;

	// Placed as a drag would place it.
	const FVector Loc = LocalRectToWorld(MinX + Slot.Min.X, MinY + Slot.Min.Y);
	const FTransform Xform(FRotator(0.0, PlacementYawDeg, 0.0),
		FVector(Loc.X, Loc.Y, StartWorld.Z));

	const FHutongPalette Palette = Appearance ? Appearance->Palette : FHutongPalette();
	AStaticMeshActor* Actor = bPlanOnly
		? HutongGen::SpawnEmptyActor(World, Xform, NameBase)
		: HutongGen::SpawnStaticMeshActor(World, LODs, Xform, NameBase, Palette, CollisionLOD);
	if (!Actor) return nullptr;

	// Each piece keeps its own building component.
	UHutongBuildingComponent* Building = nullptr;
	switch (Slot.Piece)
	{
	case EHutongCompoundPiece::MainHall:
	case EHutongCompoundPiece::EarRoom:
	case EHutongCompoundPiece::SideHouse:
	case EHutongCompoundPiece::FrontRow:
	case EHutongCompoundPiece::RearRow:
	case EHutongCompoundPiece::GateLodge:
	{
		UHutongSiheyuanBuildingComponent* B = NewObject<UHutongSiheyuanBuildingComponent>(Actor);
		B->Params = P.House;
		B->FootprintX = SX; B->FootprintY = SY; B->BaySide = Slot.Facing;
		Building = B;
		break;
	}
	case EHutongCompoundPiece::EarPassage:
	{
		UHutongEarPassageBuildingComponent* B = NewObject<UHutongEarPassageBuildingComponent>(Actor);
		B->Params = P.EarPassage;
		B->FootprintX = SX; B->FootprintY = SY; B->BaySide = Slot.Facing;
		Building = B;
		break;
	}
	case EHutongCompoundPiece::GateHouse:
	{
		UHutongGateHouseBuildingComponent* B = NewObject<UHutongGateHouseBuildingComponent>(Actor);
		B->Params = P.Gate;
		B->FootprintX = SX; B->FootprintY = SY; B->BaySide = Slot.Facing;
		Building = B;
		break;
	}
	case EHutongCompoundPiece::InnerGate:
	{
		UHutongInnerGateBuildingComponent* B = NewObject<UHutongInnerGateBuildingComponent>(Actor);
		B->Params = P.InnerGate;
		const bool bX = HutongGen::BaySide::IsAlongX(Slot.Facing);
		B->Width = bX ? SX : SY; B->Depth = bX ? SY : SX; B->BaySide = Slot.Facing;
		Building = B;
		break;
	}
	case EHutongCompoundPiece::ScreenWall:
	{
		UHutongScreenWallBuildingComponent* B = NewObject<UHutongScreenWallBuildingComponent>(Actor);
		B->Params = Settings->ScreenWall;
		B->Length = Run; B->bLengthAlongY = bAlongY;
		Building = B;
		break;
	}
	case EHutongCompoundPiece::Corridor:
	{
		UHutongCorridorBuildingComponent* B = NewObject<UHutongCorridorBuildingComponent>(Actor);
		B->Params = P.Corridor;
		B->bOmitLowEndPost = Slot.bOmitLowEndPost;
		B->bOmitHighEndPost = Slot.bOmitHighEndPost;
		B->bNoBenchAtLowEnd = Slot.bNoBenchAtLowEnd;
		B->bNoBenchAtHighEnd = Slot.bNoBenchAtHighEnd;
		// On the component, not Params.
		B->BenchGapAt = Slot.CorridorBenchGapAt;
		B->Length = Run;
		B->Width = B->Params.Width;
		B->bLengthAlongY = bAlongY;
		B->bFlipOpenSide = Slot.bFlipOpenSide;
		Building = B;
		break;
	}
	case EHutongCompoundPiece::FlowerBed:
	{
		UHutongFlowerBedBuildingComponent* B = NewObject<UHutongFlowerBedBuildingComponent>(Actor);
		B->Params = Settings->FlowerBed;
		B->FootprintX = SX; B->FootprintY = SY;
		Building = B;
		break;
	}
	case EHutongCompoundPiece::WaterJar:
	{
		UHutongWaterJarBuildingComponent* B = NewObject<UHutongWaterJarBuildingComponent>(Actor);
		B->Params = P.WaterJar;
		Building = B;
		break;
	}
	case EHutongCompoundPiece::Passage:
	{
		UHutongPassageBuildingComponent* B = NewObject<UHutongPassageBuildingComponent>(Actor);
		B->Params = P.PassageRoof;
		B->Length = Run;
		B->Width = Cross - 2.0 * B->Params.Bearing;
		B->bLengthAlongY = bAlongY;
		Building = B;
		break;
	}
	case EHutongCompoundPiece::Path:
	{
		UHutongPathBuildingComponent* B = NewObject<UHutongPathBuildingComponent>(Actor);
		B->Params = Settings->Path;
		B->Length = Run;
		B->Width = Settings->Path.WidthFromFootprint(Cross);
		B->bLengthAlongY = bAlongY;
		Building = B;
		break;
	}
	case EHutongCompoundPiece::Wall:
	default:
	{
		UHutongWallBuildingComponent* B = NewObject<UHutongWallBuildingComponent>(Actor);
		B->Params = P.Wall;
		B->Length = Run; B->bLengthAlongY = bAlongY;
		// Plotted width the run was laid on.
		B->FootprintThickness = Cross;
		Building = B;
		break;
	}
	}

	if (Building)
	{
		Building->FootprintSkew = Slot.Skew;
		if (Appearance) Building->Palette = Appearance->Palette;
		StampDetail(Building);
		Actor->AddInstanceComponent(Building);
		Building->RegisterComponent();
		// Houses' 窗紙 lights, as for a hand-placed one.
		Building->ApplyPlacementAttachments();
	}
	return Actor;
}

void UHutongCompoundTool::ResolveHallEave(const TArray<FHutongCompoundSlot>& Slots,
	double SizeX, double SizeY) const
{
	HallEaveZ = 0.0;
	WalkFloorZ = 0.0;
	if (!Settings) return;
	for (const FHutongCompoundSlot& S : Slots)
	{
		if (S.Piece != EHutongCompoundPiece::MainHall) continue;
		FHutongSiheyuanParams P = HutongCompound::CourtRow(
			MainHallFor(Settings, SizeX, SizeY), Settings->Plan, S.Facing);
		const bool bAlongX = HutongGen::BaySide::IsAlongX(S.Facing);
		P.Width = bAlongX ? S.Size.X : S.Size.Y;
		P.Depth = bAlongX ? S.Size.Y : S.Size.X;
		HallEaveZ = P.GetEaveHeight();
		const bool bWalk = Settings->CourtWalk == EHutongCourtWalk::Linked
			|| Settings->CourtWalk == EHutongCourtWalk::Corridor;
		WalkFloorZ = bWalk ? P.GetFloorHeight() : 0.0;
	}
}

void UHutongCompoundTool::ResolveStreetRowRidge(const TArray<FHutongCompoundSlot>& Slots) const
{
	StreetRowRidgeZ = 0.0;
	if (!Settings) return;

	// From the slot, not re-derived from plot width.
	for (const FHutongCompoundSlot& S : Slots)
	{
		if (S.Piece != EHutongCompoundPiece::FrontRow && S.Piece != EHutongCompoundPiece::GateLodge)
		{
			continue;
		}
		const bool bAlongX = HutongGen::BaySide::IsAlongX(S.Facing);
		const double Frontage = bAlongX ? S.Size.X : S.Size.Y;
		const double Depth    = bAlongX ? S.Size.Y : S.Size.X;
		StreetRowRidgeZ = FMath::Max(StreetRowRidgeZ,
			HutongGen::Ridge::House(Settings->FrontRow, Frontage, Depth));
	}
}

void UHutongCompoundTool::SpawnFinalActor()
{
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const double SizeX = MaxX - MinX;
	const double SizeY = MaxY - MinY;
	// GetEffectiveRectBounds already enforces the minimum; this catches a drag that never started.
	if (SizeX < 100.0 || SizeY < 100.0) return;

	const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(MakeInput(SizeX, SizeY));
	if (Slots.Num() == 0) return;

	// Before spawning: 大門 height depends on its neighbours'.
	ResolveStreetRowRidge(Slots);
	ResolveHallEave(Slots, SizeX, SizeY);
	PlacedMainHall = &MainHallFor(Settings, SizeX, SizeY);
	ON_SCOPE_EXIT { PlacedMainHall = nullptr; };

	UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();

	// One transaction: a compound is one Ctrl+Z.
	UInteractiveToolManager* ToolManager = GetToolManager();
	ToolManager->BeginUndoTransaction(LOCTEXT("PlaceCompound", "Place Hutong Compound"));

	// Each piece bakes a UStaticMesh; a full 二進 plan freezes the editor for seconds.
	FScopedSlowTask Task((float)Slots.Num(), LOCTEXT("BuildingCompound", "Laying out courtyard house (四合院)…"));
	Task.MakeDialog();

	for (const FHutongCompoundSlot& Slot : Slots)
	{
		Task.EnterProgressFrame(1.0f,
			FText::Format(LOCTEXT("BuildingPiece", "Building {0}…"), PieceLabel(Slot.Piece)));
		SpawnSlot(World, Slot, MinX, MinY);
	}
	ToolManager->EndUndoTransaction();
}

void UHutongCompoundTool::DrawPlan(FPrimitiveDrawInterface* PDI, const FVector& Origin,
	double SizeX, double SizeY) const
{
	if (SizeX < 100.0 || SizeY < 100.0) return;
	auto At = [&](double X, double Y) { return LocalRectToWorldFrom(Origin, X, Y); };

	// Plan preview.
	const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(MakeInput(SizeX, SizeY));
	const FLinearColor Plan(0.35f, 0.75f, 1.0f, 1.0f);
	for (const FHutongCompoundSlot& S : Slots)
	{
		const double X0 = S.Min.X, Y0 = S.Min.Y;
		const double X1 = X0 + S.Size.X, Y1 = Y0 + S.Size.Y;
		const FVector A = At(X0, Y0), B = At(X1, Y0), C = At(X1, Y1), D = At(X0, Y1);
		DrawPreviewLine(PDI, A, B, Plan, 3.0f);
		DrawPreviewLine(PDI, B, C, Plan, 3.0f);
		DrawPreviewLine(PDI, C, D, Plan, 3.0f);
		DrawPreviewLine(PDI, D, A, Plan, 3.0f);
	}

	// Plot outline, warm like every tool's footprint.
	const FLinearColor Plot(1.0f, 0.9f, 0.15f, 1.0f);
	DrawPreviewLine(PDI, At(0.0, 0.0), At(SizeX, 0.0), Plot, 5.0f);
	DrawPreviewLine(PDI, At(SizeX, 0.0), At(SizeX, SizeY), Plot, 5.0f);
	DrawPreviewLine(PDI, At(SizeX, SizeY), At(0.0, SizeY), Plot, 5.0f);
	DrawPreviewLine(PDI, At(0.0, SizeY), At(0.0, 0.0), Plot, 5.0f);

	// Ordinary plot ghosted from the same corner beyond the drag.
	double WantW, WantD;
	GetSuggestedPlot(WantW, WantD);
	if (FMath::Abs(WantW - SizeX) > 1.0 || FMath::Abs(WantD - SizeY) > 1.0)
	{
		const FLinearColor Ghost(0.45f, 0.8f, 1.0f, 1.0f);
		DrawDashedPreviewLine(PDI, At(0.0, 0.0), At(WantW, 0.0), Ghost, 2.0f);
		DrawDashedPreviewLine(PDI, At(WantW, 0.0), At(WantW, WantD), Ghost, 2.0f);
		DrawDashedPreviewLine(PDI, At(WantW, WantD), At(0.0, WantD), Ghost, 2.0f);
		DrawDashedPreviewLine(PDI, At(0.0, WantD), At(0.0, 0.0), Ghost, 2.0f);
	}

	// Street edge: Y = 0 in plot frame.
	const FLinearColor Street(1.0f, 0.55f, 0.15f, 1.0f);
	DrawPreviewLine(PDI, At(0.0, 0.0), At(SizeX, 0.0), Street, 7.0f);

	// North arrow up the middle.
	const double CX = 0.5 * SizeX;
	const double Head = SizeY + 0.12 * SizeY;
	const double Wing = 0.05 * SizeX;
	const FVector Tail = At(CX, -0.06 * SizeY);
	const FVector Tip = At(CX, Head);
	DrawPreviewLine(PDI, Tail, Tip, Plan, 4.0f);
	DrawPreviewLine(PDI, Tip, At(CX - Wing, Head - 1.6 * Wing), Plan, 4.0f);
	DrawPreviewLine(PDI, Tip, At(CX + Wing, Head - 1.6 * Wing), Plan, 4.0f);
}

void UHutongCompoundTool::RenderIdlePreview(FPrimitiveDrawInterface* PDI, const FVector& CursorGround)
{
	// Before any click the whole plan follows the cursor at ordinary size, southeast corner on the mouse.
	ApplyNorthOrientation();
	double W, D;
	PlotSizeForCursor(FVector2D::ZeroVector, W, D);
	double WantW, WantD;
	GetSuggestedPlot(WantW, WantD);
	DrawPlan(PDI, CursorGround, FMath::Max(W, WantW), FMath::Max(D, WantD));
}

void UHutongCompoundTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);
	if (!bIsDragging || RenderAPI == nullptr) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	DrawPlan(PDI, StartWorld, MaxX - MinX, MaxY - MinY);
}

FString UHutongCompoundTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const TArray<FHutongCompoundSlot> Slots =
		HutongGen::LayOutCompound(MakeInput(MaxX - MinX, MaxY - MinY));
	double MinW, MinD;
	MakeInput(1.0, 1.0).GetMinimumPlot(MinW, MinD);
	const bool bAtFloor = (MaxX - MinX <= MinW + 1.0) || (MaxY - MinY <= MinD + 1.0);

	const TCHAR* SizeName =
		(Settings->PlotSize == EHutongCompoundSize::Small)  ? TEXT("small (小型)") :
		(Settings->PlotSize == EHutongCompoundSize::Medium) ? TEXT("medium (中型)") :
		(Settings->PlotSize == EHutongCompoundSize::Large)  ? TEXT("large (大型)") :
		(Settings->PlotSize == EHutongCompoundSize::Standard) ? TEXT("standard (標准, Fig 2-9.1)") : TEXT("custom");

	const TCHAR* PlanName = TEXT("One Courtyard (一進)");
	if (Settings->Plan == EHutongCompoundPlan::TwoCourtyards) PlanName = TEXT("Two Courtyards (二進)");
	else if (Settings->Plan == EHutongCompoundPlan::ThreeCourtyards) PlanName = TEXT("Three Courtyards (三進)");
	return FString::Printf(TEXT("%s · %s · %d buildings%s"),
		SizeName,
		PlanName,
		Slots.Num(),
		bAtFloor ? *FString::Printf(TEXT(" · at minimum %.0f x %.0f"), MinW, MinD) : TEXT(""));
}

double UHutongCompoundTool::GetPreviewHeight() const
{
	// Plan drawn flat; corner posts over a compound would be a cage.
	return 0.0;
}

void UHutongCompoundTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	// Height keys move all three storey heights together, keeping their hierarchy.
	// Seed from the plot's slot, not the preset frontage: eave derives from bay width, else the first press jumps every roof.
	auto TakeManualControl = [this](FHutongSiheyuanParams& P, EHutongCompoundPiece Piece)
	{
		if (!P.bDeriveProportions || !P.bDeriveEaveFromBays) return;
		FHutongSiheyuanParams Probe = P;
		double Frontage = 0.0, Depth = 0.0;
		if (RowSlotSize(Piece, Frontage, Depth))
		{
			Probe.Width = Frontage;
			Probe.Depth = Depth;
		}
		else if (Probe.SuggestedFrontage > 0.0)
		{
			Probe.Width = Probe.SuggestedFrontage;
		}
		P.EaveHeight = Probe.GetEaveHeight();
		P.bDeriveEaveFromBays = false;
	};
	TakeManualControl(Settings->MainHall, EHutongCompoundPiece::MainHall);
	TakeManualControl(Settings->SmallMainHall, EHutongCompoundPiece::MainHall);
	TakeManualControl(Settings->SideHouse, EHutongCompoundPiece::SideHouse);
	TakeManualControl(Settings->FrontRow, EHutongCompoundPiece::FrontRow);

	auto Bump = [DeltaCm](double& V, double Lo, double Hi) { V = FMath::Clamp(V + DeltaCm, Lo, Hi); };
	Bump(Settings->MainHall.EaveHeight, Settings->MainHall.GetMinEaveHeight(), 520.0);
	Bump(Settings->SmallMainHall.EaveHeight, Settings->SmallMainHall.GetMinEaveHeight(), 520.0);
	Bump(Settings->SideHouse.EaveHeight, Settings->SideHouse.GetMinEaveHeight(),
		FMath::Max(Settings->MainHall.EaveHeight - 15.0, Settings->SideHouse.GetMinEaveHeight()));
	Bump(Settings->FrontRow.EaveHeight, Settings->FrontRow.GetMinEaveHeight(),
		FMath::Max(Settings->SideHouse.EaveHeight - 5.0, Settings->FrontRow.GetMinEaveHeight()));
}

bool UHutongCompoundTool::RowSlotSize(EHutongCompoundPiece Piece, double& OutFrontage, double& OutDepth) const
{
	// Plot under cursor while placing, else ordinary; same figure the preview draws.
	double W, D;
	PlotSizeForCursor(bIsDragging ? WorldXYToLocalRect(CurrentWorld) : FVector2D::ZeroVector, W, D);
	for (const FHutongCompoundSlot& S : HutongGen::LayOutCompound(MakeInput(W, D)))
	{
		if (S.Piece != Piece) continue;
		const bool bAlongX = HutongGen::BaySide::IsAlongX(S.Facing);
		OutFrontage = bAlongX ? S.Size.X : S.Size.Y;
		OutDepth = bAlongX ? S.Size.Y : S.Size.X;
		return true;
	}
	return false;
}

TArray<FText> UHutongCompoundTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongCompoundTool", "HelpDrag",
		"Click to set the plot's southeast corner, move northwest to size it, click to lay it out. The orange edge is the street.");
	Lines.Insert(NSLOCTEXT("HutongCompoundTool", "HelpNorth",
		"The plan faces south by the compass, so R does nothing; set North Direction if north is not -X."), 1);
	Lines.Insert(NSLOCTEXT("HutongCompoundTool", "HelpMin",
		"The plot stops at the smallest size the settings allow; corridors or a deeper main hall (正房) raise it."), 1);
	Lines.Insert(NSLOCTEXT("HutongCompoundTool", "HelpWhat",
		"Places a dozen separate, editable buildings on an ideal plan: for comparing types, not tracing a real plot."), 1);
	return Lines;
}

#undef LOCTEXT_NAMESPACE
