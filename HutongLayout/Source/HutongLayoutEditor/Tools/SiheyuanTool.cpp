#include "Tools/SiheyuanTool.h"
#include "Tools/HutongPresetDefaults.h"
#include "Generation/HutongPlanOutlineComponent.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"
#include "LevelEditorViewport.h"
#include "ToolContextInterfaces.h"
#include "SceneManagement.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongSiheyuanToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UHutongSiheyuanTool* Tool = NewObject<UHutongSiheyuanTool>(SceneState.ToolManager);
	return Tool;
}

void UHutongSiheyuanToolProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	Params.ClampAfterEdit(PropertyChangedEvent.GetPropertyName());
}

void UHutongSiheyuanTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	Super::GetEffectiveRectBounds(OutMinX, OutMinY, OutMaxX, OutMaxY);
	// Suggested-footprint snap obeys the snapping switch.
	if (!Settings || !SnappingActive()) return;

	// One end is the anchor; the far end moves.
	auto SnapSide = [&](double& Lo, double& Hi)
	{
		const double Want = Settings->Params.SnapExtent(Hi - Lo);
		if (Hi > 0.0) Hi = Lo + Want;
		else          Lo = Hi - Want;
	};
	SnapSide(OutMinX, OutMaxX);
	SnapSide(OutMinY, OutMaxY);
}

void UHutongSiheyuanTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongSiheyuanToolProperties>(this);

	// Registration order is panel order; presets before params.
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("Siheyuan"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongSiheyuanToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	RegisterSettings(Presets);

	RegisterSettings(Settings);

	ApplyDefaultPreset(Presets, HutongPresets::DefaultSiheyuanName());
}

// Settings' params with Width and Depth from the drag.
FHutongSiheyuanParams UHutongSiheyuanTool::GetResolvedParams() const
{
	FHutongSiheyuanParams P = Settings ? Settings->Params : FHutongSiheyuanParams();

	if (bIsDragging)
	{
		double MinX, MinY, MaxX, MaxY;
		GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
		const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
		P.Width = FMath::Max(bAlongX ? MaxX - MinX : MaxY - MinY, 1.0);
		P.Depth = FMath::Max(bAlongX ? MaxY - MinY : MaxX - MinX, 1.0);
	}
	else if (P.SuggestedFrontage > 0.0)
	{
		P.Width = P.SuggestedFrontage;
		P.Depth = FMath::Max(P.GetSuggestedDepth(), 1.0);
	}

	P.BayCountOverride = BayCountOverride;
	return P;
}

// Veranda depth as built, clamped against the drag depth.
double UHutongSiheyuanTool::GetEffectiveVerandaDepth() const
{
	if (!Settings) return 0.0;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const double Depth = HutongGen::BaySide::IsAlongX(BaySide) ? (MaxY - MinY) : (MaxX - MinX);

	const FHutongSiheyuanParams P = GetResolvedParams();
	// Same wall-thickness clamp BuildSiheyuan applies.
	const double T = FMath::Clamp(P.GetFacadeWallThickness(), 1.0, FMath::Min(FMath::Max(P.Width, 1.0), Depth) * 0.2);
	double Front, Rear;
	P.GetBuiltVerandaDepths(Depth, T, Front, Rear);
	return Front;
}

FString UHutongSiheyuanTool::GetPlacementDetail() const
{
	const int32 N = ComputeBayCountForSide(BaySide);
	if (N <= 0) return FString();

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const double FacadeLen = HutongGen::BaySide::IsAlongX(BaySide) ? (MaxX - MinX) : (MaxY - MinY);

	// Whether the footprint sits on the preset's suggestion.
	auto TargetText = [this, MinX, MinY, MaxX, MaxY]()
	{
		if (!Settings) return FString();
		const FHutongSiheyuanParams& P = Settings->Params;
		if (!P.bSnapToSuggested || !SnappingActive()
			|| (P.SuggestedFrontage <= 0.0 && P.GetSuggestedDepth() <= 0.0))
		{
			return FString();
		}
		const bool bSnapped =
			FMath::IsNearlyEqual(P.SnapExtent(MaxX - MinX), MaxX - MinX, 0.01) &&
			FMath::IsNearlyEqual(P.SnapExtent(MaxY - MinY), MaxY - MinY, 0.01) &&
			(FMath::IsNearlyEqual(MaxX - MinX, P.SuggestedFrontage, 0.01) ||
			 FMath::IsNearlyEqual(MaxX - MinX, P.GetSuggestedDepth(), 0.01)) &&
			(FMath::IsNearlyEqual(MaxY - MinY, P.SuggestedFrontage, 0.01) ||
			 FMath::IsNearlyEqual(MaxY - MinY, P.GetSuggestedDepth(), 0.01));
		return bSnapped
			? FString(TEXT(" · on suggested size"))
			: FString::Printf(TEXT(" · suggested %.0f x %.0f"), P.SuggestedFrontage, P.GetSuggestedDepth());
	};

	// Veranda clamps to the drag depth, so built value may differ from the parameter.
	auto VerandaText = [this]()
	{
		const double V = GetEffectiveVerandaDepth();
		return V > 0.0 ? FString::Printf(TEXT(" · veranda (廊) %.0f cm"), V) : FString();
	};

	// Bays unequal once 明間/次間 applies.
	if (Settings && Settings->Params.bDeriveProportions && N > 1)
	{
		const int32 Central = Settings->Params.GetDoorBayIndex(N);
		// Corner column inset included, as the generator does: cancels between interior boundaries, bites on a two-bay house whose 明間 reaches the end.
		const double ColR = GetResolvedParams().GetColumnRadius();
		const double CentralW =
			Settings->Params.GetBayBoundary(Central + 1, N, FacadeLen, ColR)
			- Settings->Params.GetBayBoundary(Central, N, FacadeLen, ColR);
		const double SideW = CentralW * FMath::Clamp(Settings->Params.SideBayWidthRatio, 0.3, 1.0);
		return FString::Printf(TEXT("%d bays · central bay (明間) %.0f / side bay (次間) %.0f cm%s%s%s"), N, CentralW, SideW,
			BayCountOverride > 0 ? TEXT(" (manual)") : TEXT(""), *VerandaText(), *TargetText());
	}

	return FString::Printf(TEXT("%d bays @ %.0f cm%s%s%s"), N, FacadeLen / N,
		BayCountOverride > 0 ? TEXT(" (manual)") : TEXT(""), *VerandaText(), *TargetText());
}

FText UHutongSiheyuanTool::GetKeyHintText() const
{
	return FText::Format(NSLOCTEXT("SiheyuanTool", "KeyHint", "[ ] bays · {0}"), Super::GetKeyHintText());
}

TArray<FText> UHutongSiheyuanTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines.Insert({
		NSLOCTEXT("HutongSiheyuanTool", "HelpPreset",
		"Choose the house under Type / Preset: main hall (正房), side house (廂房), front row (倒座房), ear room (耳房)… each has its own size and details."),
		NSLOCTEXT("HutongSiheyuanTool", "HelpSize",
			"The footprint settles on the house's usual frontage and depth (dashed outline) while snapping is on."),
		NSLOCTEXT("HutongSiheyuanTool", "HelpFacadeSide",
			"After the footprint, move toward the side that gets the doors and windows (green ticks), then click to place."),
		NSLOCTEXT("HutongSiheyuanTool", "HelpBays",
			"The bays (間) follow from the frontage; [ and ] add or remove one while placing."),
	}, 1);
	return Lines;
}

int32 UHutongSiheyuanTool::ComputeBayCountForSide(HutongGen::EBaySide Side) const
{
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const double SizeX = MaxX - MinX;
	const double SizeY = MaxY - MinY;
	if (BayCountOverride > 0) return BayCountOverride;

	const double FacadeLen = HutongGen::BaySide::IsAlongX(Side) ? SizeX : SizeY;
	const FHutongSiheyuanParams P = Settings ? Settings->Params : FHutongSiheyuanParams();
	return HutongGen::ComputeBayCount(FacadeLen, P.MinBayWidth, P.MaxBayWidth);
}

void UHutongSiheyuanTool::AdjustBracketValue(int32 Delta, bool /*bFine*/, bool /*bCoarse*/)
{
	// First press steps off the derived count.
	const int32 Current = ComputeBayCountForSide(BaySide);
	BayCountOverride = FMath::Clamp(Current + Delta, 1, MaxBayCount);
}

void UHutongSiheyuanTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	FHutongSiheyuanParams& P = Settings->Params;

	// Move the eave, keep the roof rise.
	if (P.bDeriveProportions && P.bDeriveEaveFromBays)
	{
		// Seeded from the resolved params.
		P.EaveHeight = GetResolvedParams().GetEaveHeight();
		P.bDeriveEaveFromBays = false;
	}
	P.EaveHeight = FMath::Clamp(P.EaveHeight + DeltaCm,
		FMath::Max(P.DoorTopHeight + 10.0, P.GetMinEaveHeight()), 5000.0);
}

double UHutongSiheyuanTool::GetPreviewHeight() const
{
	// Eave, not ridge: the keys move it, and corner posts stand where walls meet the eave line.
	return Settings ? GetResolvedParams().GetEaveHeight() : 0.0;
}

void UHutongSiheyuanTool::OnPlacementStarted(const FVector& HitWorld)
{
	BaySide = ComputeDefaultBaySide();
	BayCountOverride = 0;
}

void UHutongSiheyuanTool::CancelPlacement()
{
	BayCountOverride = 0;
	Super::CancelPlacement();
}

bool UHutongSiheyuanTool::OnRectCommitted(const FVector& HitWorld)
{
	BaySide = ComputeDefaultBaySide();
	return false;
}

void UHutongSiheyuanTool::OnPlacementHover(const FVector& HitWorld)
{
	if (bRotateModeActive) return;
	BaySide = bRectCommitted
		? ComputeClosestSide(HitWorld.X, HitWorld.Y)
		: ComputeDefaultBaySide();
}

TArray<FText> UHutongSiheyuanTool::GetStageNames() const
{
	TArray<FText> Names = Super::GetStageNames();
	Names.Add(NSLOCTEXT("SiheyuanTool", "StageFacing", "Facing"));
	return Names;
}

FText UHutongSiheyuanTool::GetStagePromptText() const
{
	// Committed stage is the facade-side pick, not "click to place".
	if (bIsDragging && bRectCommitted && !bRotateModeActive)
	{
		return NSLOCTEXT("HutongSiheyuanTool", "PromptBaySide",
			"Move to pick the side that gets the bay facade (green ticks), [ and ] change the bay count, then click to place.");
	}
	// Preset names the building type.
	if (!bIsDragging && Presets && !Presets->Preset.IsEmpty() && GetPlanEditPromptText().IsEmpty())
	{
		return FText::Format(NSLOCTEXT("HutongSiheyuanTool", "PromptAnchorPreset",
			"Placing {0}: click the ground to anchor a corner of the footprint."),
			FText::FromString(Presets->Preset));
	}
	return Super::GetStagePromptText();
}

void UHutongSiheyuanTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);
	if (!bIsDragging || RenderAPI == nullptr) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const double SizeX = MaxX - MinX;
	const double SizeY = MaxY - MinY;
	if (SizeX < 1.0 || SizeY < 1.0) return;

	// Preset's suggested footprint, ghosted from the anchor as a drag target.
	if (!bRectCommitted)
	{
		const FHutongSiheyuanParams& P = Settings ? Settings->Params : FHutongSiheyuanParams();
		if (P.bSnapToSuggested && SnappingActive()
			&& (P.SuggestedFrontage > 0.0 || P.GetSuggestedDepth() > 0.0))
		{
			// Suggested extent per axis, laid out from the anchor in the drag direction.
			const double GX = P.SnapExtent(SizeX);
			const double GY = P.SnapExtent(SizeY);
			const double GhostMinX = (MaxX > 0.0) ? MinX : MaxX - GX;
			const double GhostMinY = (MaxY > 0.0) ? MinY : MaxY - GY;

			const FVector G0 = LocalRectToWorld(GhostMinX,      GhostMinY);
			const FVector G1 = LocalRectToWorld(GhostMinX + GX, GhostMinY);
			const FVector G2 = LocalRectToWorld(GhostMinX + GX, GhostMinY + GY);
			const FVector G3 = LocalRectToWorld(GhostMinX,      GhostMinY + GY);

			// Cool, dashed, vs the footprint's solid warm yellow: the height preview's cues for "projection".
			const FLinearColor Ghost(0.35f, 0.75f, 1.0f, 1.0f);
			DrawDashedPreviewLine(PDI, G0, G1, Ghost, 2.5f, 28.0);
			DrawDashedPreviewLine(PDI, G1, G2, Ghost, 2.5f, 28.0);
			DrawDashedPreviewLine(PDI, G2, G3, Ghost, 2.5f, 28.0);
			DrawDashedPreviewLine(PDI, G3, G0, Ghost, 2.5f, 28.0);
		}
	}

	const int32 N = ComputeBayCountForSide(BaySide);
	if (N <= 0) return;

	const double ColR = FMath::Max(
		0.5 * (Settings ? GetResolvedParams().GetColumnDiameter() : 30.0), 1.0);

	const HutongGen::BaySide::FEdge Edge = HutongGen::BaySide::GetEdge(BaySide, MinX, MinY, MaxX, MaxY);
	const double Span = Edge.bAlongX ? SizeX : SizeY;
	const double SpanMin = Edge.bAlongX ? MinX : MinY;

	// Same params helper as the generator (明間/次間 included), turned like the mesh: +Y and -X reverse the frontage and the door bay.
	FHutongPlanBays Bays;
	for (int32 i = 0; i <= N; ++i)
	{
		Bays.Boundaries.Add(Settings ? Settings->Params.GetBayBoundary(i, N, Span, ColR) : Span * i / double(FMath::Max(N, 1)));
	}
	if (Settings && Settings->Params.bHasFrontDoorCenter) Bays.DoorBay = Settings->Params.GetDoorBayIndex(N);
	HutongGen::PlanBays::OntoFacade(Bays, BaySide, SizeX, SizeY);
	auto BoundaryAt = [&](int32 i) { return SpanMin + Bays.Boundaries[FMath::Clamp(i, 0, N)]; };

	DrawRectBaysAndFacing(PDI, BaySide, MinX, MinY, MaxX, MaxY, Bays.Boundaries, Bays.DoorBay);

	// 前廊: columns on the footprint edge, wall set back behind them.
	const double VerandaDepth = GetEffectiveVerandaDepth();
	if (VerandaDepth > 0.0)
	{
		// Inward = minus the edge's outward normal.
		const FVector2D In(-Edge.OutDir.X, -Edge.OutDir.Y);
		const double WallCoord = Edge.FixedCoord + (Edge.bAlongX ? In.Y : In.X) * VerandaDepth;

		const FVector A = Edge.bAlongX ? LocalRectToWorld(BoundaryAt(0), WallCoord)
									   : LocalRectToWorld(WallCoord, BoundaryAt(0));
		const FVector B = Edge.bAlongX ? LocalRectToWorld(BoundaryAt(N), WallCoord)
									   : LocalRectToWorld(WallCoord, BoundaryAt(N));
		DrawDashedPreviewLine(PDI, A, B, FLinearColor(0.45f, 0.8f, 1.0f), 3.0f);
	}
}

void UHutongSiheyuanTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Level)
{
	// Same entry point the placed actor rebuilds through.
	UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
		Settings ? Settings->Params : FHutongSiheyuanParams(),
		BaySide, BayCountOverride, SizeX, SizeY, OutMesh, Level);
}

void UHutongSiheyuanTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor || !Settings) return;

	UHutongSiheyuanBuildingComponent* Building = NewObject<UHutongSiheyuanBuildingComponent>(
		Actor, NAME_None, RF_Transactional);
	Building->Params = Settings->Params;
	Building->FootprintX = SizeX;
	Building->FootprintY = SizeY;
	Building->BaySide = BaySide;
	Building->BayCountOverride = BayCountOverride;
	if (Appearance)
	{
		Building->Palette = Appearance->Palette;
	}

	StampDetail(Building);

	// AddInstanceComponent, not just RegisterComponent: else absent from Details and not saved.
	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();

	// After registration: lights attach to the actor root, which needs the component live.
	Building->ApplyPlacementAttachments();
}
