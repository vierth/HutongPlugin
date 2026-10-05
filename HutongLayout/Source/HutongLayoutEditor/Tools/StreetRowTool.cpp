#include "Tools/StreetRowTool.h"
#include "Tools/HutongPresetDefaults.h"
#include "Tools/HutongSnap.h"
#include "Generation/StreetRowLayout.h"
#include "Generation/HutongGateRow.h"
#include "Generation/HutongBays.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongActorSpawn.h"
#include "Generation/HutongDetail.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"
#include "ToolContextInterfaces.h"
#include "LevelEditorViewport.h"
#include "SceneManagement.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "HutongStreetRowTool"

using UE::Geometry::FDynamicMesh3;

UHutongStreetRowToolProperties::UHutongStreetRowToolProperties()
{
	// Street row's own type.
	House = HutongPresets::MakeHouse(HutongCanon::House::FrontRow);
}

UInteractiveTool* UHutongStreetRowToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongStreetRowTool>(SceneState.ToolManager);
}

namespace
{
	FString PresetForPosition(EHutongStreetRowPosition Position)
	{
		return Position == EHutongStreetRowPosition::Rear ? FString(TEXT("Rear Row (後罩房)")) : HutongPresets::DefaultStreetRowHouseName();
	}
}

void UHutongStreetRowTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongStreetRowToolProperties>(this);
	RegisterSettings(Settings);

	// House presets, so a row can be 倒座房 or 廂房 by name. Keyed as the house's, so StampDetail names the preset on houses, not gates.
	HousePresets = NewObject<UHutongPresetProperties>(this);
	HousePresets->Initialize(TEXT("Siheyuan"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongStreetRowToolProperties, House));
	HousePresets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	RegisterSettings(HousePresets);

	ApplyDefaultPreset(HousePresets, PresetForPosition(Settings->Position));
}

void UHutongStreetRowTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	Super::OnPropertyModified(PropertySet, Property);
	if (PropertySet != Settings || !Property || !HousePresets
		|| Property->GetFName() != GET_MEMBER_NAME_CHECKED(UHutongStreetRowToolProperties, Position)) return;
	// The position is the house preset: front row or rear row, loaded over the houses' parameters.
	HousePresets->Preset = PresetForPosition(Settings->Position);
	HousePresets->LoadSelectedPreset();
	NotifyOfPropertyChangeByTool(HousePresets);
}

bool UHutongStreetRowTool::IsRunAlongX() const
{
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	return (MaxX - MinX) >= (MaxY - MinY);
}

double UHutongStreetRowTool::RunLength() const
{
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	return FMath::Max(MaxX - MinX, MaxY - MinY);
}

double UHutongStreetRowTool::RowDepth() const
{
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	return FMath::Min(MaxX - MinX, MaxY - MinY);
}

int32 UHutongStreetRowTool::GetBayCount() const
{
	if (!Settings) return 1;
	if (Settings->BayCountOverride > 0) return Settings->BayCountOverride;
	const double L = RunLength();
	return Settings->Kind == EHutongStreetRowKind::Houses
		? HutongGen::ComputeBayCount(L, Settings->House.MinBayWidth, Settings->House.MaxBayWidth)
		: HutongGen::ComputeBayCount(L, Settings->Shop.MinBayWidth, Settings->Shop.MaxBayWidth);
}

int32 UHutongStreetRowTool::BayUnder(const FVector& World) const
{
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const FVector2D L = WorldXYToLocalRect(World);
	const bool bX = IsRunAlongX();
	// Across the row too, else a click beside it picks a bay.
	const double Across = bX ? L.Y : L.X;
	const double AcrossMin = bX ? MinY : MinX;
	const double AcrossMax = bX ? MaxY : MaxX;
	if (Across < AcrossMin || Across > AcrossMax) return INDEX_NONE;
	const double Along = bX ? (L.X - MinX) : (L.Y - MinY);
	return HutongGen::StreetRow::BayAt(Along, RunLength(), GetBayCount());
}

HutongGen::EBaySide UHutongStreetRowTool::SideUnder(const FVector& World) const
{
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const FVector2D L = WorldXYToLocalRect(World);
	return HutongGen::BaySide::ClosestOnAxis(IsRunAlongX(), L.X, L.Y, MinX, MinY, MaxX, MaxY);
}

void UHutongStreetRowTool::PieceRect(double From, double To,
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	if (IsRunAlongX())
	{
		OutMinX = MinX + From; OutMaxX = MinX + To; OutMinY = MinY; OutMaxY = MaxY;
	}
	else
	{
		OutMinY = MinY + From; OutMaxY = MinY + To; OutMinX = MinX; OutMaxX = MaxX;
	}
}

void UHutongStreetRowTool::OnPlacementStarted(const FVector& HitWorld)
{
	Extra = EExtra::BuildingsFace;
	GateBays.Reset();
	HoverBay = INDEX_NONE;
	bGatesFaceBack = false;
	if (Settings)
	{
		Settings->BayCountOverride = 0;
		Settings->Gate.RandomSeed = FMath::Rand();
	}
}

bool UHutongStreetRowTool::OnRectCommitted(const FVector& HitWorld)
{
	Extra = EExtra::BuildingsFace;
	// Camera's side of the run until the hover decides.
	if (GCurrentLevelEditingViewportClient)
	{
		BuildingSide = SideUnder(GCurrentLevelEditingViewportClient->GetViewLocation());
	}
	bGatesFaceBack = false;
	return false;
}

void UHutongStreetRowTool::OnPlacementHover(const FVector& HitWorld)
{
	if (bRotateModeActive || !bRectCommitted) return;
	switch (Extra)
	{
	case EExtra::BuildingsFace: BuildingSide = SideUnder(HitWorld); break;
	case EExtra::GateBays:      HoverBay = BayUnder(HitWorld); break;
	}
}

bool UHutongStreetRowTool::OnExtraStageClicked(const FVector& HitWorld)
{
	switch (Extra)
	{
	case EExtra::BuildingsFace:
		BuildingSide = SideUnder(HitWorld);
		Extra = EExtra::GateBays;
		HoverBay = BayUnder(HitWorld);
		return false;

	case EExtra::GateBays:
	{
		const int32 Bay = BayUnder(HitWorld);
		if (Bay != INDEX_NONE)
		{
			if (GateBays.Contains(Bay)) GateBays.Remove(Bay); else GateBays.Add(Bay);
			return false;
		}
		// Click off the row builds; gates face as the ticks show.
		return true;
	}
	}
	return true;
}

void UHutongStreetRowTool::FlipFacing()
{
	if (bRectCommitted) bGatesFaceBack = !bGatesFaceBack;
}

void UHutongStreetRowTool::CancelPlacement()
{
	Super::CancelPlacement();
	Extra = EExtra::BuildingsFace;
	GateBays.Reset();
	HoverBay = INDEX_NONE;
	bGatesFaceBack = false;
}

void UHutongStreetRowTool::AdjustBracketValue(int32 Delta, bool /*bFine*/, bool /*bCoarse*/)
{
	if (!Settings) return;

	// Keys slide the gates once bays are being picked; before that they count bays.
	if (bRectCommitted && Extra != EExtra::BuildingsFace && GateBays.Num() > 0)
	{
		const int32 N = GetBayCount();
		TSet<int32> Moved;
		for (int32 Bay : GateBays)
		{
			const int32 To = Bay + Delta;
			if (To < 0 || To >= N) return;
			Moved.Add(To);
		}
		GateBays = Moved;
		return;
	}

	const int32 Current = GetBayCount();
	Settings->BayCountOverride = FMath::Clamp(Current + Delta, 1, 32);
	// Drop gate bays that no longer exist.
	const int32 N = Settings->BayCountOverride;
	for (auto It = GateBays.CreateIterator(); It; ++It) if (*It >= N) It.RemoveCurrent();
}

void UHutongStreetRowTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	if (Settings->Kind == EHutongStreetRowKind::Shops)
	{
		FHutongShopfrontParams& P = Settings->Shop;
		P.EaveHeight = FMath::Clamp(P.EaveHeight + DeltaCm, 150.0, 5000.0);
		return;
	}

	// As the house tool: keys move the eave, derivation stands aside.
	FHutongSiheyuanParams& P = Settings->House;
	P = HutongGen::StreetRow::RowHouse(P, RunLength(), RowDepth(), GetBayCount());
	P.EaveHeight = FMath::Clamp(P.EaveHeight + DeltaCm,
		FMath::Max(P.DoorTopHeight + 10.0, P.GetMinEaveHeight()), 5000.0);
}

double UHutongStreetRowTool::GetPreviewHeight() const
{
	if (!Settings) return 0.0;
	if (Settings->Kind == EHutongStreetRowKind::Shops) return Settings->Shop.GetEaveHeight();
	FHutongSiheyuanParams P = HutongGen::StreetRow::RowHouse(Settings->House, RunLength(), RowDepth(), GetBayCount());
	P.Width = FMath::Max(RunLength(), 1.0);
	P.Depth = FMath::Max(RowDepth(), 1.0);
	return P.GetEaveHeight();
}

TArray<FText> UHutongStreetRowTool::GetStageNames() const
{
	TArray<FText> Names = Super::GetStageNames();
	Names.Add(LOCTEXT("StageBuildingsFace", "Buildings face"));
	Names.Add(LOCTEXT("StageGateBays", "Gate bays"));
	return Names;
}

int32 UHutongStreetRowTool::GetStageIndex() const
{
	if (!bIsDragging || !bRectCommitted) return Super::GetStageIndex();
	switch (Extra)
	{
	case EExtra::BuildingsFace: return 2;
	default:                    return 3;
	}
}

FText UHutongStreetRowTool::GetStagePromptText() const
{
	if (!bIsDragging || !bRectCommitted || bRotateModeActive) return Super::GetStagePromptText();
	switch (Extra)
	{
	case EExtra::BuildingsFace:
		return LOCTEXT("PromptBuildingsFace",
			"Move to the side the buildings face (green ticks), then click to set it.");
	default:
		return LOCTEXT("PromptGateBays",
			"Click a bay to toggle a gate. [ and ] slide the gates, F flips them (orange ticks). Click off the row to build.");
	}
}

FString UHutongStreetRowTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const int32 N = GetBayCount();
	TArray<int32> Sorted = GateBays.Array();
	Sorted.Sort();
	FString Gates;
	for (int32 B : Sorted) Gates += FString::Printf(TEXT("%s%d"), Gates.IsEmpty() ? TEXT("") : TEXT(", "), B + 1);
	return FString::Printf(TEXT("%s · %d bays @ %.0f cm · gate at %s"),
		Settings->Kind == EHutongStreetRowKind::Houses ? TEXT("houses (房)") : TEXT("shops (鋪面房)"),
		N, HutongGen::StreetRow::BayWidth(RunLength(), N),
		Gates.IsEmpty() ? TEXT("none") : *Gates);
}

FText UHutongStreetRowTool::GetKeyHintText() const
{
	return FText::Format(LOCTEXT("KeyHint", "[ ] bays, slide gates · F flip gates · {0}"), Super::GetKeyHintText());
}

TArray<FText> UHutongStreetRowTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines.Insert(LOCTEXT("HelpRow",
		"After the footprint, move toward the street side, then click the bays that are gates. F turns the gates round."), 1);
	Lines.Insert(LOCTEXT("HelpBays",
		"[ and ] change the bay count, then slide the gates once picked."), 2);
	return Lines;
}

void UHutongStreetRowTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);
	if (!bIsDragging || RenderAPI == nullptr) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	if (MaxX - MinX < 1.0 || MaxY - MinY < 1.0) return;

	const bool bX = IsRunAlongX();
	const double L = RunLength();
	const int32 N = GetBayCount();
	const double W = HutongGen::StreetRow::BayWidth(L, N);

	const FLinearColor Division(1.0f, 0.85f, 0.3f);
	const FLinearColor GateFill(1.0f, 0.55f, 0.1f);
	const FLinearColor Hover(1.0f, 1.0f, 1.0f);

	auto At = [&](double Along, double Across)
	{
		return bX ? LocalRectToWorld(MinX + Along, Across) : LocalRectToWorld(Across, MinY + Along);
	};
	const double AcrossMin = bX ? MinY : MinX;
	const double AcrossMax = bX ? MaxY : MaxX;

	// Building boundaries.
	for (int32 i = 1; i < N; ++i)
	{
		DrawPreviewLine(PDI, At(i * W, AcrossMin), At(i * W, AcrossMax), Division, 2.0f);
	}

	// Gate bays, plus the hovered bay while picking.
	auto Bracket = [&](int32 Bay, const FLinearColor& C, float Thick, double Inset)
	{
		const double A0 = Bay * W + Inset, A1 = (Bay + 1) * W - Inset;
		const double C0 = AcrossMin + Inset, C1 = AcrossMax - Inset;
		DrawPreviewLine(PDI, At(A0, C0), At(A1, C0), C, Thick);
		DrawPreviewLine(PDI, At(A1, C0), At(A1, C1), C, Thick);
		DrawPreviewLine(PDI, At(A1, C1), At(A0, C1), C, Thick);
		DrawPreviewLine(PDI, At(A0, C1), At(A0, C0), C, Thick);
		DrawPreviewLine(PDI, At(A0, C0), At(A1, C1), C, Thick);
		DrawPreviewLine(PDI, At(A1, C0), At(A0, C1), C, Thick);
	};
	for (int32 Bay : GateBays) Bracket(Bay, GateFill, 3.0f, 0.06 * W);
	if (bRectCommitted && Extra == EExtra::GateBays && HoverBay != INDEX_NONE && !GateBays.Contains(HoverBay))
	{
		Bracket(HoverBay, Hover, 1.5f, 0.12 * W);
	}

	if (!bRectCommitted) return;

	// Facing ticks: green on the buildings' side over the whole run, orange on the gates' side over gate bays only.
	const double TickLen = FMath::Max(30.0, 0.04 * L);
	auto Ticks = [&](HutongGen::EBaySide Side, const FLinearColor& C, double From, double To, int32 Count)
	{
		const HutongGen::BaySide::FEdge Edge = HutongGen::BaySide::GetEdge(Side, MinX, MinY, MaxX, MaxY);
		for (int32 i = 0; i <= Count; ++i)
		{
			const double Along = From + (To - From) * (double)i / FMath::Max(Count, 1);
			const FVector Base = bX ? LocalRectToWorld(MinX + Along, Edge.FixedCoord)
			                        : LocalRectToWorld(Edge.FixedCoord, MinY + Along);
			const FVector OutDir = bX ? LocalRectToWorld(MinX + Along, Edge.FixedCoord + Edge.OutDir.Y) - Base
			                          : LocalRectToWorld(Edge.FixedCoord + Edge.OutDir.X, MinY + Along) - Base;
			DrawPreviewLine(PDI, Base - OutDir * 0.4 * TickLen, Base + OutDir * TickLen, C, 5.0f);
		}
	};
	{
		TArray<double> Bounds;
		for (int32 i = 0; i <= N; ++i) Bounds.Add(i * W);
		DrawRectBaysAndFacing(PDI, (EHutongBaySide)BuildingSide, MinX, MinY, MaxX, MaxY, Bounds);
	}
	for (int32 Bay : GateBays) Ticks(GateSide(), GateFill, Bay * W, (Bay + 1) * W, 2);
}

void UHutongStreetRowTool::SpawnFinalActor()
{
	if (!Settings) return;
	const double L = RunLength();
	const double D = RowDepth();
	if (L < 100.0 || D < 100.0) return;

	const int32 N = GetBayCount();
	const TArray<HutongGen::StreetRow::FPiece> Pieces = HutongGen::StreetRow::LayOut(L, N, GateBays);
	if (Pieces.Num() == 0) return;

	// One eave for the whole row, whatever each piece would derive.
	const FHutongSiheyuanParams House = HutongGen::StreetRow::RowHouse(Settings->House, L, D, N);

	// Gates rise above the row, so the row ridge comes first.
	double RowRidgeZ = 0.0;
	for (const HutongGen::StreetRow::FPiece& P : Pieces)
	{
		if (P.Piece != HutongGen::StreetRow::EPiece::Building) continue;
		RowRidgeZ = FMath::Max(RowRidgeZ, Settings->Kind == EHutongStreetRowKind::Houses
			? HutongGen::Ridge::House(House, P.To - P.From, D)
			: HutongGen::Ridge::Shop(Settings->Shop));
	}
	FHutongGateHouseParams Gate = Settings->Gate;
	// Bay is the gate's footprint, whatever the band says.
	Gate.bConstrainToHistoricalSize = false;
	HutongGen::GateRow::LiftGateAboveRidge(Gate, D, RowRidgeZ, Settings->GateRidgeClearance);

	UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();
	UInteractiveToolManager* ToolManager = GetToolManager();
	ToolManager->BeginUndoTransaction(LOCTEXT("PlaceRow", "Place Hutong Street Row"));

	FScopedSlowTask Task((float)Pieces.Num(), LOCTEXT("BuildingRow", "Building street row…"));
	Task.MakeDialog();

	const bool bPlanOnly = IsPlanOnly();
	const FHutongPalette Palette = Appearance ? Appearance->Palette : FHutongPalette();

	for (const HutongGen::StreetRow::FPiece& P : Pieces)
	{
		const bool bGate = P.Piece == HutongGen::StreetRow::EPiece::Gate;
		Task.EnterProgressFrame(1.0f, bGate ? LOCTEXT("BuildingGate", "Building gate (大門)…")
			: LOCTEXT("BuildingHouse", "Building house…"));

		double PMinX, PMinY, PMaxX, PMaxY;
		PieceRect(P.From, P.To, PMinX, PMinY, PMaxX, PMaxY);
		const double SX = PMaxX - PMinX;
		const double SY = PMaxY - PMinY;
		const HutongGen::EBaySide Side = bGate ? GateSide() : BuildingSide;

		auto BuildAt = [&](FDynamicMesh3& Mesh, EHutongDetail Level)
		{
			if (bGate)
			{
				UHutongGateHouseBuildingComponent::BuildGateHouseMesh(Gate, Side, SX, SY, Mesh, Level);
			}
			else if (Settings->Kind == EHutongStreetRowKind::Houses)
			{
				UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
					House, Side, P.BayCount, SX, SY, Mesh, Level);
			}
			else
			{
				UHutongShopfrontBuildingComponent::BuildShopfrontMesh(
					Settings->Shop, Side, P.BayCount, SX, SY, Mesh, Level);
			}
		};

		TArray<FDynamicMesh3> LODs;
		const int32 CollisionLOD = HutongGen::Detail::BuildPlacementLODs(bPlanOnly, SX, SY,
			GetDetailLevel(), ShouldBuildLODChain(), BuildAt, LODs);
		if (!bPlanOnly && (LODs.Num() == 0 || LODs[0].TriangleCount() == 0)) continue;

		const FVector Loc = LocalRectToWorld(PMinX, PMinY);
		const FTransform Xform(FRotator(0.0, PlacementYawDeg, 0.0), FVector(Loc.X, Loc.Y, StartWorld.Z));
		const FString NameBase = bGate ? TEXT("Hutong_Gate")
			: (Settings->Kind == EHutongStreetRowKind::Houses ? TEXT("Hutong_Fang") : TEXT("Hutong_Pumianfang"));
		AStaticMeshActor* Actor = bPlanOnly
			? HutongGen::SpawnEmptyActor(World, Xform, NameBase)
			: HutongGen::SpawnStaticMeshActor(World, LODs, Xform, NameBase, Palette, CollisionLOD);
		if (!Actor) continue;

		UHutongBuildingComponent* Building = nullptr;
		if (bGate)
		{
			UHutongGateHouseBuildingComponent* B = NewObject<UHutongGateHouseBuildingComponent>(Actor);
			B->Params = Gate;
			B->FootprintX = SX; B->FootprintY = SY; B->BaySide = Side;
			Building = B;
		}
		else if (Settings->Kind == EHutongStreetRowKind::Houses)
		{
			UHutongSiheyuanBuildingComponent* B = NewObject<UHutongSiheyuanBuildingComponent>(Actor);
			B->Params = House;
			B->FootprintX = SX; B->FootprintY = SY; B->BaySide = Side;
			B->BayCountOverride = P.BayCount;
			Building = B;
		}
		else
		{
			UHutongShopfrontBuildingComponent* B = NewObject<UHutongShopfrontBuildingComponent>(Actor);
			B->Params = Settings->Shop;
			B->FootprintX = SX; B->FootprintY = SY; B->BaySide = Side;
			B->BayCountOverride = P.BayCount;
			Building = B;
		}

		Building->Palette = Palette;
		StampDetail(Building);
		Actor->AddInstanceComponent(Building);
		Building->RegisterComponent();
		Building->ApplyPlacementAttachments();
	}

	ToolManager->EndUndoTransaction();
	HutongSnap::Invalidate();
}

#undef LOCTEXT_NAMESPACE
