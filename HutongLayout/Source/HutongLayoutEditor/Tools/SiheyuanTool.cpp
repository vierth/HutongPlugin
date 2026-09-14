#include "Tools/SiheyuanTool.h"
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
	// The suggested footprint is snapping like any other, so it answers to the same switch.
	if (!Settings || !SnappingActive()) return;

	// One end of each extent is the anchor, so the far end is the one that moves.
	auto Snap = [&](double& Lo, double& Hi)
	{
		const double Want = Settings->Params.SnapExtent(Hi - Lo);
		if (Hi > 0.0) Hi = Lo + Want;
		else          Lo = Hi - Want;
	};
	Snap(OutMinX, OutMaxX);
	Snap(OutMinY, OutMaxY);
}

void UHutongSiheyuanTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongSiheyuanToolProperties>(this);

	// Registered before the params, and the order here is the panel order.
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("Siheyuan"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongSiheyuanToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	RegisterSettings(Presets);

	RegisterSettings(Settings);
}

// The settings' params with Width and Depth filled in from the drag.
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

// The veranda depth that will actually be built, including the clamp against the drag's depth.
double UHutongSiheyuanTool::GetEffectiveVerandaDepth() const
{
	if (!Settings) return 0.0;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const double Depth = HutongGen::BaySide::IsAlongX(BaySide) ? (MaxY - MinY) : (MaxX - MinX);

	const FHutongSiheyuanParams P = GetResolvedParams();
	const double T = P.WallThickness;
	const double ColR = FMath::Max(0.5 * P.GetColumnDiameter(), 1.0);

	// Same two guards as BuildSiheyuan, and they have to stay the same or the preview lies.
	double V = P.GetVerandaDepth();
	if (V < 4.0 * ColR) V = 0.0;
	return FMath::Clamp(V, 0.0, FMath::Max(Depth - 2.0 * T - 100.0, 0.0));
}

FString UHutongSiheyuanTool::GetPlacementDetail() const
{
	const int32 N = ComputeBayCountForSide(BaySide);
	if (N <= 0) return FString();

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const double FacadeLen = HutongGen::BaySide::IsAlongX(BaySide) ? (MaxX - MinX) : (MaxY - MinY);

	// Whether the footprint is currently sitting on the preset's suggestion.
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

	// The veranda is clamped against the drag depth, so the built figure can differ from the parameter.
	auto VerandaText = [this]()
	{
		const double V = GetEffectiveVerandaDepth();
		return V > 0.0 ? FString::Printf(TEXT(" · veranda (廊) %.0f cm"), V) : FString();
	};

	// Bays are not equal once 明間/次間 is in play.
	if (Settings && Settings->Params.bDeriveProportions && N > 1)
	{
		const int32 Central = Settings->Params.GetDoorBayIndex(N);
		// With the corner column's inset, as the generator lays the boundaries: it cancels between
		// interior boundaries and bites on a two-bay house, whose 明間 reaches the end.
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
	Lines.Insert(NSLOCTEXT("HutongSiheyuanTool", "HelpFacade",
		"After the footprint, move to pick which side gets the bay facade, then click to place. [ and ] set the bay count by hand."), 1);
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

	// Move the eave and leave the roof rise alone.
	if (P.bDeriveProportions && P.bDeriveEaveFromBays)
	{
		// Seeded through the resolved params.
		P.EaveHeight = GetResolvedParams().GetEaveHeight();
		P.bDeriveEaveFromBays = false;
	}
	P.EaveHeight = FMath::Clamp(P.EaveHeight + DeltaCm,
		FMath::Max(P.DoorTopHeight + 10.0, P.GetMinEaveHeight()), 5000.0);
}

double UHutongSiheyuanTool::GetPreviewHeight() const
{
	// Eave, not the ridge: it is the value the keys move, and the posts stand at the corners where the walls meet the eave line.
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
	// The committed stage is the extra facade-side pick, not a plain "click to place".
	if (bIsDragging && bRectCommitted && !bRotateModeActive)
	{
		return NSLOCTEXT("HutongSiheyuanTool", "PromptBaySide",
			"Move to pick the side that gets the bay facade (green ticks), [ and ] change the bay count, then click to place.");
	}
	// The preset is the building type.
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

	// The preset's suggested footprint, ghosted from the anchor so the target is visible while dragging toward it.
	if (!bRectCommitted)
	{
		const FHutongSiheyuanParams& P = Settings ? Settings->Params : FHutongSiheyuanParams();
		if (P.bSnapToSuggested && SnappingActive()
			&& (P.SuggestedFrontage > 0.0 || P.GetSuggestedDepth() > 0.0))
		{
			// Whichever suggested dimension each axis is heading for, laid out from the anchor in the direction the drag is already going.
			const double GX = P.SnapExtent(SizeX);
			const double GY = P.SnapExtent(SizeY);
			const double GhostMinX = (MaxX > 0.0) ? MinX : MaxX - GX;
			const double GhostMinY = (MaxY > 0.0) ? MinY : MaxY - GY;

			const FVector G0 = LocalRectToWorld(GhostMinX,      GhostMinY);
			const FVector G1 = LocalRectToWorld(GhostMinX + GX, GhostMinY);
			const FVector G2 = LocalRectToWorld(GhostMinX + GX, GhostMinY + GY);
			const FVector G3 = LocalRectToWorld(GhostMinX,      GhostMinY + GY);

			// Cool and dashed against the footprint's solid warm yellow, the same three cues the height preview uses to say "this is a projection, not the thing itself".
			const FLinearColor Ghost(0.35f, 0.75f, 1.0f, 1.0f);
			DrawDashedPreviewLine(PDI, G0, G1, Ghost, 2.5f, 28.0);
			DrawDashedPreviewLine(PDI, G1, G2, Ghost, 2.5f, 28.0);
			DrawDashedPreviewLine(PDI, G2, G3, Ghost, 2.5f, 28.0);
			DrawDashedPreviewLine(PDI, G3, G0, Ghost, 2.5f, 28.0);
		}
	}

	const int32 N = ComputeBayCountForSide(BaySide);
	if (N <= 0) return;

	const FLinearColor TickColor(0.25f, 1.0f, 0.45f, 1.0f);
	const float TickThickness = 5.0f;
	const double TickLen = FMath::Max(30.0, 0.04 * FMath::Max(SizeX, SizeY));
	// Ticks cross the facade edge.
	const double TickInset = 0.4 * TickLen;
	const double ColR = FMath::Max(
		0.5 * (Settings ? GetResolvedParams().GetColumnDiameter() : 30.0), 1.0);

	const FRotator Rot(0.0, PlacementYawDeg, 0.0);
	auto DrawTick = [&](double LocalX, double LocalY, const FVector2D& OutDirLocal)
	{
		const FVector Base = LocalRectToWorld(LocalX, LocalY);
		const FVector OutDir = Rot.RotateVector(FVector(OutDirLocal.X, OutDirLocal.Y, 0.0));
		DrawPreviewLine(PDI, Base - OutDir * TickInset, Base + OutDir * TickLen,
			TickColor, TickThickness);
	};

	const HutongGen::BaySide::FEdge Edge = HutongGen::BaySide::GetEdge(BaySide, MinX, MinY, MaxX, MaxY);
	const double Span = Edge.bAlongX ? SizeX : SizeY;
	const double SpanMin = Edge.bAlongX ? MinX : MinY;

	// Straight through the params helper the generator uses, including the 明間/次間 widths, and
	// turned onto the facade the way the built mesh is: +Y and -X reverse the frontage, which
	// takes the door bay with them.
	FHutongPlanBays Bays;
	for (int32 i = 0; i <= N; ++i)
	{
		Bays.Boundaries.Add(Settings ? Settings->Params.GetBayBoundary(i, N, Span, ColR) : Span * i / double(FMath::Max(N, 1)));
	}
	if (Settings && Settings->Params.bHasFrontDoorCenter) Bays.DoorBay = Settings->Params.GetDoorBayIndex(N);
	HutongGen::PlanBays::OntoFacade(Bays, BaySide, SizeX, SizeY);
	auto BoundaryAt = [&](int32 i) { return SpanMin + Bays.Boundaries[FMath::Clamp(i, 0, N)]; };

	for (int32 i = 0; i <= N; ++i)
	{
		const double T = BoundaryAt(i);
		if (Edge.bAlongX) DrawTick(T, Edge.FixedCoord, Edge.OutDir);
		else              DrawTick(Edge.FixedCoord, T, Edge.OutDir);
	}

	// 前廊: the columns stand on the footprint edge and the wall retreats behind them.
	const double VerandaDepth = GetEffectiveVerandaDepth();
	if (VerandaDepth > 0.0)
	{
		// Inward is the opposite of the edge's outward normal.
		const FVector2D In(-Edge.OutDir.X, -Edge.OutDir.Y);
		const double WallCoord = Edge.FixedCoord + (Edge.bAlongX ? In.Y : In.X) * VerandaDepth;

		const FVector A = Edge.bAlongX ? LocalRectToWorld(BoundaryAt(0), WallCoord)
									   : LocalRectToWorld(WallCoord, BoundaryAt(0));
		const FVector B = Edge.bAlongX ? LocalRectToWorld(BoundaryAt(N), WallCoord)
									   : LocalRectToWorld(WallCoord, BoundaryAt(N));
		DrawDashedPreviewLine(PDI, A, B, FLinearColor(0.45f, 0.8f, 1.0f), 3.0f);
	}

	// Mark the door bay along the facade edge.
	if (Bays.DoorBay != INDEX_NONE)
	{
		const double T0 = BoundaryAt(Bays.DoorBay);
		const double T1 = BoundaryAt(Bays.DoorBay + 1);
		const FVector A = Edge.bAlongX ? LocalRectToWorld(T0, Edge.FixedCoord)
		                               : LocalRectToWorld(Edge.FixedCoord, T0);
		const FVector B = Edge.bAlongX ? LocalRectToWorld(T1, Edge.FixedCoord)
		                               : LocalRectToWorld(Edge.FixedCoord, T1);
		DrawPreviewLine(PDI, A, B, FLinearColor(1.0f, 0.45f, 0.1f), 7.0f);
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

	// AddInstanceComponent is what makes it show up in the actor's Details panel and get saved with the actor; RegisterComponent alone would leave it invisible and transient.
	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();

	// After registration: the lights attach to the actor's root, which the component needs to be live to reach.
	Building->ApplyPlacementAttachments();
}
