#include "Tools/CompoundTool.h"
#include "Tools/HutongPresetDefaults.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongGateRow.h"
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
	// What the progress dialog calls each piece.
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

	// The corridor the compound's ring is built from.
	FHutongCorridorParams HutongRingCorridor(const FHutongCorridorParams& Base, double WalkWidth)
	{
		FHutongCorridorParams C = Base;
		C.bClosedSide = true;
		C.bBuildBackWall = false;
		C.Width = FMath::Clamp(WalkWidth,
			FMath::Max(C.WalkWidthMin, 10.0), FMath::Max(C.WalkWidthMax, C.WalkWidthMin + 1.0));
		return C;
	}

	// Ridge height above the plot, which is what actually reads along a street front.
}

// 耳房過道: the ear room of this court with the way through to the 後院 cut through it, at whichever
// end of its frontage the plot boundary is.
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
	// Taken off the bay rule rather than clamped inside it: every figure above the eave — 額枋, 中檻,
	// sill — is a fraction of it, and they must all come down together.
	P.bDeriveEaveFromBays = false;
	P.EaveHeight = MaxEave;
	return P;
}

// The 廂房 the compound builds, which is the panel's preset plus whatever the court's walk asks of it.
FHutongSiheyuanParams HutongCompound::CourtWing(const FHutongSiheyuanParams& Base, EHutongCourtWalk Walk)
{
	FHutongSiheyuanParams P = OnCourtWalk(Base, Walk);
	if ((Walk != EHutongCourtWalk::WingVerandas && Walk != EHutongCourtWalk::Linked) || P.bHasFrontVeranda)
	{
		return P;
	}

	const double Rooms = P.GetSuggestedDepth();
	P.bHasFrontVeranda = true;
	P.Purlins = EHutongPurlins::Seven;
	if (Rooms > 0.0 && P.SuggestedDepth <= 0.0)
	{
		P.StepRun = Rooms / 5.0;
	}
	return P;
}

FHutongSiheyuanParams HutongCompound::OnCourtWalk(const FHutongSiheyuanParams& Base, EHutongCourtWalk Walk)
{
	FHutongSiheyuanParams P = Base;
	P.bHasVerandaEndDoorways = (Walk == EHutongCourtWalk::Linked);
	return P;
}

// 後檐 says what stands behind a building, and for the hall row that answer is the plan's.
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
	// Seeded from the canon's own house table, which is what the built-in presets are made from,
	// so a compound's 正房 and a hand-placed one are the same building.
	MainHall = HutongPresets::MakeHouse(HutongCanon::House::MainHall);
	SmallMainHall = HutongPresets::MakeHouse(HutongCanon::House::MainHallSmall);
	EarRoom = HutongPresets::MakeHouse(HutongCanon::House::EarRoom);
	SideHouse = HutongPresets::MakeHouse(HutongCanon::House::SideHouse);
	FrontRow = HutongPresets::MakeHouse(HutongCanon::House::FrontRow);
	RearRow = HutongPresets::MakeHouse(HutongCanon::House::RearRow);

	// The 後罩房's back is the plot's north boundary here, not another court: the preset ships the
	// symmetrical eave for a hand-placed one standing between two courts, which a compound's is not.
	RearRow.RearEave = EHutongRearEave::Lane;

	GateHouse.Style = EHutongGateStyle::Ruyi;
	// A gate in a terrace has a doorstep, not a forecourt.
	GateHouse.PlatformOverhang = 18.0;
	GateHouse.StepCount = 2;

	// The wall's height and thickness come from its role now.
	ApplyCourtSize();
}

void UHutongCompoundToolProperties::ApplyCourtSize()
{
	if (PlotSize == EHutongCompoundSize::Custom) return;

	// 院落寬大, 房子也隨之高大: the court's own figures for the three buildings its width decides.
	// Both halls are seeded — which of them is built is still the plot's answer (MainHallFor).
	const HutongCanon::Courtyard::FSize& S = HutongPresets::CourtSize(PlotSize);
	MainHall = HutongPresets::MakeCourtHall(S, /*bRearVeranda*/ true);
	SmallMainHall = HutongPresets::MakeCourtHall(S, /*bRearVeranda*/ false);
	EarRoom = HutongPresets::MakeCourtEarRoom(S);
	SideHouse = HutongPresets::MakeCourtWing(S);
	// 廂房進深 5.5 m 含外廊 in a 大型 court, 3.5-4 m 無外廊 in a 小型 one: the walk is part of the size.
	CourtWalk = S.bWingVeranda ? EHutongCourtWalk::WingVerandas : EHutongCourtWalk::None;

	// 院當寬度: what the plot's width leaves between the two 廂房 once the walls and gaps are off it.
	// Left as a figure rather than derived at layout time so the panel shows the court being built.
	const double Wing = HutongCompound::CourtWing(SideHouse, CourtWalk).GetSuggestedDepth();
	const double Court = FMath::Max(
		S.PlotWidthCm - 2.0 * (Wing + HutongCanon::Wall::PerimeterThicknessCm + Gap), 300.0);
	Courtyard.X = Court;
	MinCourtyard.X = 0.92 * Court;
}

void UHutongCompoundToolProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// Only the size itself reseeds the buildings; editing one of them afterwards must stick.
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

	// A stamped size is not dragged: the width is the court's own, the depth what this plan wants on it.
	if (Settings && Settings->PlotSize != EHutongCompoundSize::Custom)
	{
		OutW = FMath::Max(HutongPresets::CourtSize(Settings->PlotSize).PlotWidthCm, MinW);
		OutD = FMath::Max(WantD, MinD);
		return;
	}

	// The southeast corner is the anchor and the cursor is the northwest one.
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

	// The plot has the street at local -Y and the 正房 across local +Y.
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

	// The footprint has to go on first.
	const bool bAlongX = HutongGen::BaySide::IsAlongX(Facing);
	P.Width = FMath::Max(bAlongX ? SizeX : SizeY, 1.0);
	P.Depth = FMath::Max(bAlongX ? SizeY : SizeX, 1.0);

	// The wall's band starts at the ground; a building's starts on top of its 臺基.
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
	MakeInputWithHall(Settings, Settings->MainHall, 1.0, 1.0).GetSuggestedPlot(OutW, OutD);
}

const FHutongSiheyuanParams& UHutongCompoundTool::MainHallFor(const UHutongCompoundToolProperties* S, double SizeX, double SizeY)
{
	// A stamped court knows its own answer: 七檁前後廊 in a 大型 court, 前廊後無廊 in the smaller two
	// (四合院建築及其構造 p.85). Only a dragged plot has to be measured.
	if (S->PlotSize != EHutongCompoundSize::Custom)
	{
		return HutongPresets::CourtSize(S->PlotSize).bHallRearVeranda ? S->MainHall : S->SmallMainHall;
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

	// Depths come from the buildings' own suggested footprints.
	In.HallDepth = FMath::Max(Hall.GetSuggestedDepth(), 200.0);
	// The wing as it will actually be built.
	In.WingDepth = FMath::Max(
		HutongCompound::CourtWing(Settings->SideHouse, Settings->CourtWalk).GetSuggestedDepth(), 200.0);
	// 正房三間兩耳: the hall is its own frontage in the middle, the 耳房 fill the ends, and their depth comes from their own 檁數 like everyone else's.
	In.HallFrontage = FMath::Max(Hall.SuggestedFrontage, 400.0);
	In.EarRoomDepth = FMath::Max(Settings->EarRoom.GetSuggestedDepth(), 150.0);
	In.bHasEarRooms = Settings->bHasEarRooms;
	In.MinEarRoomFrontage = FMath::Max(1.05 * Settings->EarRoom.MinBayWidth, 150.0);
	In.WingFrontage = FMath::Max(Settings->SideHouse.SuggestedFrontage, 300.0);

	In.bHasWingEarRooms = Settings->bHasWingEarRooms;
	In.FrontRowDepth = FMath::Max(Settings->FrontRow.GetSuggestedDepth(), 180.0);
	In.RearRowDepth = FMath::Max(Settings->RearRow.GetSuggestedDepth(), 180.0);
	In.RearCourtDepth = FMath::Max(Settings->RearCourtDepth, 150.0);
	In.PassageWidth = FMath::Max(Settings->PassageWidth, 120.0);
	In.PassageBearing = FMath::Clamp(Settings->Passage.Bearing, 1.0, 30.0);

	const FHutongGateHouseParams::FSizeRange GR = Settings->GateHouse.GetSizeRange();
	In.GateFrontage = (GR.FrontageMax > 0.0) ? 0.5 * (GR.FrontageMin + GR.FrontageMax) : 360.0;
	// 進深 is the row's: a 大門 is one bay of the street face, not a porch in front of it. The style's
	// band sizes a gate standing alone, which inside a row left it 60 cm shallow, its roof a box out
	// over the lane.
	In.GateDepth = In.FrontRowDepth;

	const FHutongInnerGateParams::FSizeRange IR = Settings->InnerGate.GetSizeRange();
	In.InnerGateFrontage = (IR.FrontageMax > 0.0) ? 0.5 * (IR.FrontageMin + IR.FrontageMax) : 330.0;
	In.InnerGateDepth = (IR.DepthMax > 0.0) ? 0.5 * (IR.DepthMin + IR.DepthMax) : 140.0;

	// The 廂房 has to be long enough to be one.
	In.MinWingFrontage = FMath::Max(2.0 * Settings->SideHouse.MinBayWidth,
		0.62 * Settings->SideHouse.SuggestedFrontage);
	In.MinCourtyardWidth = FMath::Max(Settings->MinCourtyard.X, 200.0);
	In.MinCourtyardDepth = FMath::Max(Settings->MinCourtyard.Y, 200.0);
	// Held at or above the minimum.
	In.CourtyardWidth = FMath::Max(Settings->Courtyard.X, In.MinCourtyardWidth);
	In.CourtyardDepth = FMath::Max(Settings->Courtyard.Y, In.MinCourtyardDepth);
	In.TypicalOuterCourtDepth = FMath::Max(Settings->TypicalOuterCourtDepth, In.OuterCourtDepth);
	In.TypicalRearCourtDepth = FMath::Max(Settings->TypicalRearCourtDepth, In.RearCourtDepth);

	In.bHasGateLodge = Settings->bHasGateLodge;
	In.GateLodgeFrontage = FMath::Max(Settings->GateLodgeFrontage, 150.0);
	In.ScreenLength = FMath::Max(Settings->ScreenWallLength, 150.0);
	// Whichever of the screen's two overhangs reaches further past its footprint's ends.
	In.ScreenSideProjection = FMath::Max(Settings->ScreenWall.GableOverhang,
		Settings->ScreenWall.PlinthProjection);
	// Both through the role, off the one params struct.
	{
		FHutongWallParams Perimeter = Settings->Wall;
		Perimeter.Role = EHutongWallRole::Perimeter;
		In.WallThickness = Perimeter.GetThickness();

		FHutongWallParams Courtyard = Settings->Wall;
		Courtyard.Role = EHutongWallRole::Courtyard;
		In.CourtyardWallThickness = Courtyard.GetThickness();
	}
	// Measured on the corridor the compound actually builds.
	In.CorridorWalkWidth = Settings->CorridorWalkWidth;
	In.CorridorDepth = HutongRingCorridor(Settings->Corridor, Settings->CorridorWalkWidth)
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

// The name is the slot's, not the build's: plan-only placements never run the build lambda,
// and a piece spawned from one was labelled with nothing but its guid.
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

AStaticMeshActor* UHutongCompoundTool::SpawnSlot(UWorld* World, const FHutongCompoundSlot& Slot,
	double MinX, double MinY) const
{
	if (!Settings) return nullptr;

	const double SX = Slot.Size.X;
	const double SY = Slot.Size.Y;

	// Which way a line-like piece runs is the slot's to say.
	const bool bAlongY = Slot.bLengthAlongY;
	const double Run = bAlongY ? SY : SX;     // along the piece's length
	const double Cross = bAlongY ? SX : SY;   // across it

	const FString NameBase = SlotNameBase(Slot.Piece);
	const FHutongSiheyuanParams* Hall = PlacedMainHall ? PlacedMainHall : &Settings->MainHall;
	// 耳房 are the hall's ears: held under its eave however wide the slot the plot leaves them.
	const double EarCap = (HallEaveZ > 0.0)
		? HallEaveZ - HutongCanon::Compound::EarRoomBelowHallCm : 0.0;

	// Perimeter courses lined up before anything is built.
	auto Aligned = [&](const FHutongSiheyuanParams& From)
	{
		FHutongSiheyuanParams P = From;
		AlignBaseCourse(P, Slot.Facing, SX, SY);
		return P;
	};

	FHutongGateHouseParams Gate = Settings->GateHouse;
	Gate.BaseCourseHeight = FMath::Max(Settings->BaseCourseTop - Gate.FloorHeight, 25.0);

	// The 大門 rises above the row it interrupts.
	HutongGen::GateRow::LiftGateAboveRidge(Gate,
		HutongGen::BaySide::IsAlongX(Slot.Facing) ? SY : SX,
		StreetRowRidgeZ, Settings->GateRidgeClearance);

	// The compound owns the role: a run either bounds the plot or crosses it, and the layout is what knows which.
	FHutongWallParams WallP = Settings->Wall;
	WallP.Role = Slot.WallRole;

	// The 過道's roof lands on the 隔牆 that closes it.
	auto PassageRoof = [&]()
	{
		FHutongWallParams Cross = Settings->Wall;
		Cross.Role = EHutongWallRole::Courtyard;
		FHutongPassageParams Pass = Settings->Passage;
		Pass.EaveHeight = FMath::Max(Cross.GetHeight(), 120.0);
		return Pass;
	};

	// 耳房過道: the ear room of this flank with the way through cut through it, at the end of its
	// frontage the plot boundary is on.
	auto EarPassageFor = [&]() -> FHutongEarPassageParams
	{
		const FHutongSiheyuanParams Room = HutongCompound::Subordinate(
			Aligned(HutongCompound::CourtRow(Settings->EarRoom, Settings->Plan, Slot.Facing)), EarCap);
		return HutongCompound::CourtEarPassage(Room, PassageRoof(), Settings->PassageWidth,
			/*bAtFarEnd*/ Slot.Min.X > 1.0);
	};

	// The compound owns the doorways for the same reason it owns the roles.
	HutongGen::ApplySlotDoorway(WallP, Slot, Run);
	// Only the perimeter is aligned. BaseCourseTop exists so the band runs unbroken round the *outside* of the plot.
	WallP.BaseCourseHeight = (Slot.WallRole == EHutongWallRole::Perimeter)
		? FMath::Clamp(Settings->BaseCourseTop, 10.0, WallP.GetHeight() * 0.6)
		: FMath::Clamp(WallP.GetHeight() / 3.0, 10.0, WallP.GetHeight() * 0.6);

	// Built through the very same static entry points the individual tools use.
	auto BuildAt = [&](FDynamicMesh3& Mesh, EHutongDetail Level)
	{
		switch (Slot.Piece)
		{
		case EHutongCompoundPiece::MainHall:
			UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
				Aligned(HutongCompound::OnCourtWalk(
					HutongCompound::CourtRow(*Hall, Settings->Plan, Slot.Facing), Settings->CourtWalk)),
				Slot.Facing, 0, SX, SY, Mesh, Level);
			break;
		case EHutongCompoundPiece::EarRoom:
			UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
				HutongCompound::Subordinate(
					Aligned(HutongCompound::CourtRow(Settings->EarRoom, Settings->Plan, Slot.Facing)), EarCap),
				Slot.Facing, 0, SX, SY, Mesh, Level);
			break;
		case EHutongCompoundPiece::EarPassage:
			UHutongEarPassageBuildingComponent::BuildEarPassageMesh(
				EarPassageFor(), Slot.Facing, SX, SY, Mesh, Level);
			break;
		case EHutongCompoundPiece::SideHouse:
			UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
				Aligned(HutongCompound::CourtWing(Settings->SideHouse, Settings->CourtWalk)),
				Slot.Facing, 0, SX, SY, Mesh, Level);
			break;
		case EHutongCompoundPiece::FrontRow:
			UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
				Aligned(Settings->FrontRow), Slot.Facing, 0, SX, SY, Mesh, Level);
			break;
		case EHutongCompoundPiece::RearRow:
			UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
				Aligned(Settings->RearRow), Slot.Facing, 0, SX, SY, Mesh, Level);
			break;
		case EHutongCompoundPiece::Passage:
			UHutongPassageBuildingComponent::BuildPassageMesh(
				PassageRoof(), Run, Cross - 2.0 * PassageRoof().Bearing, bAlongY, Mesh, Level);
			break;
		case EHutongCompoundPiece::GateLodge:
			// The 門房 is the street row continuing past the gate.
			UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
				Aligned(Settings->FrontRow), Slot.Facing, 0, SX, SY, Mesh, Level);
			break;
		case EHutongCompoundPiece::GateHouse:
			UHutongGateHouseBuildingComponent::BuildGateHouseMesh(
				Gate, Slot.Facing, SX, SY, Mesh, Level);
			break;
		case EHutongCompoundPiece::InnerGate:
			UHutongInnerGateBuildingComponent::BuildInnerGateMesh(
				Settings->InnerGate, Slot.Facing, SX, SY, Mesh, Level);
			break;
		case EHutongCompoundPiece::ScreenWall:
			UHutongScreenWallBuildingComponent::BuildScreenWallMesh(
				Settings->ScreenWall, Run, bAlongY, Mesh, Level);
			break;
		case EHutongCompoundPiece::Corridor:
		{
			FHutongCorridorParams C = HutongRingCorridor(Settings->Corridor, Settings->CorridorWalkWidth);
			C.BenchGapAt = Slot.CorridorBenchGapAt;
			UHutongCorridorBuildingComponent::BuildCorridorMesh(
				C, Run, bAlongY, Slot.bFlipOpenSide, Mesh, Level);
			break;
		}
		case EHutongCompoundPiece::FlowerBed:
			UHutongFlowerBedBuildingComponent::BuildFlowerBedMesh(
				Settings->FlowerBed, SX, SY, Mesh, Level);
			break;
		case EHutongCompoundPiece::WaterJar:
		{
			// Sized to the slot the layout reserved, so the jar the plan drew is the jar that is built.
			FHutongWaterJarParams J = Settings->WaterJar;
			J.BellyDiameter = FMath::Min(SX, SY);
			UHutongWaterJarBuildingComponent::BuildWaterJarMesh(J, Mesh, Level);
			break;
		}
		case EHutongCompoundPiece::Path:
			UHutongPathBuildingComponent::BuildPathMesh(
				Settings->Path, Run, Settings->Path.WidthFromFootprint(Cross), bAlongY, Mesh,
				Level);
			break;
		case EHutongCompoundPiece::Wall:
		default:
			UHutongWallBuildingComponent::BuildWallMesh(WallP, Run, Cross, bAlongY, Mesh, Level);
			break;
		}
	};

	TArray<FDynamicMesh3> LODs;
	HutongGen::Detail::BuildPlacementLODs(IsPlanOnly(), SX, SY,
		GetDetailLevel(), ShouldBuildLODChain(), BuildAt, LODs);
	const bool bPlanOnly = IsPlanOnly();
	if (!bPlanOnly && (LODs.Num() == 0 || LODs[0].TriangleCount() == 0)) return nullptr;

	// Placed exactly as a drag would place it.
	const FVector Loc = LocalRectToWorld(MinX + Slot.Min.X, MinY + Slot.Min.Y);
	const FTransform Xform(FRotator(0.0, PlacementYawDeg, 0.0),
		FVector(Loc.X, Loc.Y, StartWorld.Z));

	const FHutongPalette Palette = Appearance ? Appearance->Palette : FHutongPalette();
	AStaticMeshActor* Actor = bPlanOnly
		? HutongGen::SpawnEmptyActor(World, Xform, NameBase)
		: HutongGen::SpawnStaticMeshActor(World, LODs, Xform, NameBase, Palette);
	if (!Actor) return nullptr;

	// Every piece keeps its own building component.
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
		// The 門房 falls through to the 倒座房's parameters.
		B->Params = Aligned(
			(Slot.Piece == EHutongCompoundPiece::MainHall)
				? HutongCompound::OnCourtWalk(
					HutongCompound::CourtRow(*Hall, Settings->Plan, Slot.Facing), Settings->CourtWalk)
			: (Slot.Piece == EHutongCompoundPiece::EarRoom)
				? HutongCompound::CourtRow(Settings->EarRoom, Settings->Plan, Slot.Facing)
			: (Slot.Piece == EHutongCompoundPiece::SideHouse)
				? HutongCompound::CourtWing(Settings->SideHouse, Settings->CourtWalk)
			: (Slot.Piece == EHutongCompoundPiece::RearRow)   ? Settings->RearRow
			                                                  : Settings->FrontRow);
		if (Slot.Piece == EHutongCompoundPiece::EarRoom) B->Params = HutongCompound::Subordinate(B->Params, EarCap);
		B->FootprintX = SX; B->FootprintY = SY; B->BaySide = Slot.Facing;
		Building = B;
		break;
	}
	case EHutongCompoundPiece::EarPassage:
	{
		UHutongEarPassageBuildingComponent* B = NewObject<UHutongEarPassageBuildingComponent>(Actor);
		B->Params = EarPassageFor();
		B->FootprintX = SX; B->FootprintY = SY; B->BaySide = Slot.Facing;
		Building = B;
		break;
	}
	case EHutongCompoundPiece::GateHouse:
	{
		UHutongGateHouseBuildingComponent* B = NewObject<UHutongGateHouseBuildingComponent>(Actor);
		B->Params = Gate;
		B->FootprintX = SX; B->FootprintY = SY; B->BaySide = Slot.Facing;
		Building = B;
		break;
	}
	case EHutongCompoundPiece::InnerGate:
	{
		UHutongInnerGateBuildingComponent* B = NewObject<UHutongInnerGateBuildingComponent>(Actor);
		B->Params = Settings->InnerGate;
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
		// The ring's params, not the panel's, and the ring's own walk width.
		B->Params = HutongRingCorridor(Settings->Corridor, Settings->CorridorWalkWidth);
		// On the component, not on Params.
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
		B->Params = Settings->WaterJar;
		B->Params.BellyDiameter = FMath::Min(SX, SY);
		Building = B;
		break;
	}
	case EHutongCompoundPiece::Passage:
	{
		UHutongPassageBuildingComponent* B = NewObject<UHutongPassageBuildingComponent>(Actor);
		B->Params = PassageRoof();
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
		B->Params = WallP;
		B->Length = Run; B->bLengthAlongY = bAlongY;
		// The plotted width the run was laid on.
		B->FootprintThickness = Cross;
		Building = B;
		break;
	}
	}

	if (Building)
	{
		if (Appearance) Building->Palette = Appearance->Palette;
		StampDetail(Building);
		Actor->AddInstanceComponent(Building);
		Building->RegisterComponent();
		// The houses' 窗紙 lights, on the same footing as a hand-placed one's.
		Building->ApplyPlacementAttachments();
	}
	return Actor;
}

void UHutongCompoundTool::ResolveHallEave(const TArray<FHutongCompoundSlot>& Slots,
	double SizeX, double SizeY) const
{
	HallEaveZ = 0.0;
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
	}
}

void UHutongCompoundTool::ResolveStreetRowRidge(const TArray<FHutongCompoundSlot>& Slots) const
{
	StreetRowRidgeZ = 0.0;
	if (!Settings) return;

	// Taken from the slot rather than re-derived from the plot width.
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
	// GetEffectiveRectBounds already holds the rect at the minimum, so this only catches a drag that never really started.
	if (SizeX < 100.0 || SizeY < 100.0) return;

	const TArray<FHutongCompoundSlot> Slots = HutongGen::LayOutCompound(MakeInput(SizeX, SizeY));
	if (Slots.Num() == 0) return;

	// Before anything is spawned: the 大門's own height depends on how tall its neighbours came out.
	ResolveStreetRowRidge(Slots);
	ResolveHallEave(Slots, SizeX, SizeY);
	PlacedMainHall = &MainHallFor(Settings, SizeX, SizeY);
	ON_SCOPE_EXIT { PlacedMainHall = nullptr; };

	UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();

	// One transaction round the lot, so a compound is one Ctrl+Z rather than a dozen.
	UInteractiveToolManager* ToolManager = GetToolManager();
	ToolManager->BeginUndoTransaction(LOCTEXT("PlaceCompound", "Place Hutong Compound"));

	// Every piece bakes a whole UStaticMesh, which on a full 二進 plan is a few seconds of frozen editor.
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

	// The plan itself, previewed.
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

	// The plot's own outline, warm like every other tool's footprint.
	const FLinearColor Plot(1.0f, 0.9f, 0.15f, 1.0f);
	DrawPreviewLine(PDI, At(0.0, 0.0), At(SizeX, 0.0), Plot, 5.0f);
	DrawPreviewLine(PDI, At(SizeX, 0.0), At(SizeX, SizeY), Plot, 5.0f);
	DrawPreviewLine(PDI, At(SizeX, SizeY), At(0.0, SizeY), Plot, 5.0f);
	DrawPreviewLine(PDI, At(0.0, SizeY), At(0.0, 0.0), Plot, 5.0f);

	// The ordinary plot, ghosted from the same corner where the drag has not reached it.
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

	// The street edge, which is Y = 0 in the plot's frame.
	const FLinearColor Street(1.0f, 0.55f, 0.15f, 1.0f);
	DrawPreviewLine(PDI, At(0.0, 0.0), At(SizeX, 0.0), Street, 7.0f);

	// And an arrow up the middle pointing north, so the compass is legible without counting.
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
	// The whole plan rides under the cursor at the ordinary size before anything is clicked, southeast corner on the mouse.
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
		(Settings->PlotSize == EHutongCompoundSize::Large)  ? TEXT("large (大型)") : TEXT("custom");

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
	// The plan is drawn flat on the ground; corner posts over a whole compound would be a cage.
	return 0.0;
}

void UHutongCompoundTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	// The height keys move all three storey heights together, keeping the hierarchy between them.
	// Seeded from the slot the plot gives the row, not the preset's suggested frontage: the eave
	// derives from the bay width, and a row built at the plot's width has a different one, so
	// the first press used to step every roof before the delta was applied.
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
	// The plot under the cursor while placing, the ordinary one otherwise — the same figure the preview draws.
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
		"The plan rides under the cursor with its southeast corner on the mouse. Click to set that corner, move northwest to size the plot, click again to lay it out. The orange edge is the street; the blue rectangles are where each building will land; the dashed outline is the ordinary plot.");
	Lines.Insert(NSLOCTEXT("HutongCompoundTool", "HelpNorth",
		"The plan is pinned to the compass — main hall (正房) facing south, gate in the south wall at its east end — so R does nothing here. Set North Direction below if the project's north is not +X."), 1);
	Lines.Insert(NSLOCTEXT("HutongCompoundTool", "HelpMin",
		"The plot will not go below the smallest one the current settings can be spaced on — turn the corridors on, or deepen the main hall (正房), and that minimum grows with them. Moving the cursor south or east of the corner does not flip the plan; it holds at the minimum."), 1);
	Lines.Insert(NSLOCTEXT("HutongCompoundTool", "HelpWhat",
		"Places a dozen separate buildings, each individually editable afterwards — the plan is idealised, so it is for standing types up next to each other rather than for tracing a real plot."), 1);
	return Lines;
}

#undef LOCTEXT_NAMESPACE
