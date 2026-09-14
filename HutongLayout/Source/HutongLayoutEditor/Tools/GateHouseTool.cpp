#include "Tools/GateHouseTool.h"
#include "Generation/GateHouseGenerator.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongGateRow.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"
#include "LevelEditorViewport.h"
#include "SceneManagement.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongGateHouseToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongGateHouseTool>(SceneState.ToolManager);
}

#if WITH_EDITOR
void UHutongGateHouseToolProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// The head first, since the eave is then held above it.
	Params.DoorHeadHeight = FMath::Max(Params.DoorHeadHeight,
		HutongGen::Passage::MinHeadZ(Params.FloorHeight, Params.ThresholdHeight));

	const FHutongGateHouseParams::FSizeRange R = Params.GetSizeRange();
	if (R.EaveMax > 0.0)
	{
		Params.EaveHeight = FMath::Clamp(Params.EaveHeight, R.EaveMin, R.EaveMax);
	}
	Params.EaveHeight = FMath::Max(Params.EaveHeight, Params.GetMinEaveHeight());

	// The door head has to stay under the eave whatever the style just did to it.
	Params.DoorHeadHeight = FMath::Min(Params.DoorHeadHeight, Params.EaveHeight - 20.0);
}
#endif

void UHutongGateHouseTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	Super::GetEffectiveRectBounds(OutMinX, OutMinY, OutMaxX, OutMaxY);

	if (!Settings) return;
	const FHutongGateHouseParams::FSizeRange R = Settings->Params.GetSizeRange();
	if (R.FrontageMax <= 0.0) return;

	// One end of each extent is the anchor the drag started from, so grow the other.
	auto ClampAxis = [](double& Lo, double& Hi, double MinLen, double MaxLen)
	{
		const double Want = FMath::Clamp(Hi - Lo, MinLen, MaxLen);
		if (Hi > 0.0) Hi = Lo + Want;
		else          Lo = Hi - Want;
	};

	// The row's depth beats the band's: the street face is one run of masonry. And the body
	// stands behind the front line whatever side of it the cursor is on — the anchor is the
	// row's front corner, and a gate that grew out into the lane would have its back on the row's front.
	const double RowDepth = MatchedRowDepth();
	auto PinDepth = [&](double& Lo, double& Hi, double Inward)
	{
		if (Inward > 0.0) { Lo = 0.0; Hi = RowDepth; }
		else              { Lo = -RowDepth; Hi = 0.0; }
	};

	if (HutongGen::BaySide::IsAlongX(BaySide))
	{
		ClampAxis(OutMinX, OutMaxX, R.FrontageMin, R.FrontageMax);
		if (RowDepth > 0.0 && RowInwardLocal.Y != 0.0) PinDepth(OutMinY, OutMaxY, RowInwardLocal.Y);
		else ClampAxis(OutMinY, OutMaxY, R.DepthMin, R.DepthMax);
	}
	else
	{
		ClampAxis(OutMinY, OutMaxY, R.FrontageMin, R.FrontageMax);
		if (RowDepth > 0.0 && RowInwardLocal.X != 0.0) PinDepth(OutMinX, OutMaxX, RowInwardLocal.X);
		else ClampAxis(OutMinX, OutMaxX, R.DepthMin, R.DepthMax);
	}
}

double UHutongGateHouseTool::MatchedRowEave() const
{
	if (!Settings || !Settings->bMatchNeighbouringRow || !bIsDragging) return 0.0;
	const UHutongBuildingComponent* Row = AnchorSnapBuilding.Get();
	return Row ? Row->GetEaveHeight() : 0.0;
}

double UHutongGateHouseTool::MatchedRowDepth() const
{
	const double Eave = MatchedRowEave();
	if (Eave <= 0.0) return 0.0;
	const UHutongBuildingComponent* Row = AnchorSnapBuilding.Get();
	if (!Row) return 0.0;

	EHutongBaySide RowSide = EHutongBaySide::MinusY;
	const bool bHasFacade = Row->GetFacade(RowSide);
	const double Depth = HutongGen::GateRow::RowDepth(
		Row->GetFootprintSize(), bHasFacade, HutongGen::BaySide::IsAlongX(RowSide));
	return HutongGen::GateRow::IsRow(Eave, Depth) ? Depth : 0.0;
}

double UHutongGateHouseTool::MatchedGateEave() const
{
	const double Depth = MatchedRowDepth();
	if (Depth <= 0.0 || !Settings) return 0.0;
	const UHutongBuildingComponent* Row = AnchorSnapBuilding.Get();
	FHutongGateHouseParams P = Settings->Params;
	HutongGen::GateRow::LiftGateAboveRidge(P, Depth, Row ? Row->GetRidgeHeight() : 0.0,
		Settings->RowRidgeClearance);
	return P.GetEaveHeight();
}

void UHutongGateHouseTool::TakeRowBearing()
{
	const UHutongBuildingComponent* Row = AnchorSnapBuilding.Get();
	const AActor* Owner = Row ? Row->GetOwner() : nullptr;
	EHutongBaySide RowSide = EHutongBaySide::MinusY;
	if (!Owner || !Row->GetFacade(RowSide)) return;

	// The gate's run is the row's run, whatever the snap or the camera said.
	const FTransform& Xf = Owner->GetActorTransform();
	const bool bRowAlongX = HutongGen::BaySide::IsAlongX(RowSide);
	PlacementYawDeg = HutongGen::GateRow::RunYawDeg(Xf.Rotator().Yaw, bRowAlongX);

	// And it faces the way the row faces: the row's outward facade normal, carried into the
	// gate's frame now that the frame is the row's.
	const FVector2D Size = Row->GetFootprintSize();
	const HutongGen::BaySide::FEdge Edge =
		HutongGen::BaySide::GetEdge(RowSide, 0.0, 0.0, Size.X, Size.Y);
	const FVector OutWorld = Xf.TransformVectorNoScale(FVector(Edge.OutDir.X, Edge.OutDir.Y, 0.0));
	const FVector Origin = LocalRectToWorld(0.0, 0.0);
	const FVector2D OutLocal = WorldXYToLocalRect(Origin + OutWorld) - WorldXYToLocalRect(Origin);
	BaySide = HutongGen::BaySide::DefaultFromCameraDelta(OutLocal.X, OutLocal.Y);

	// The body extends the other way from the front line, on the axis the facade is across.
	RowInwardLocal = FVector2D::ZeroVector;
	if (HutongGen::BaySide::IsAlongX(BaySide)) RowInwardLocal.Y = OutLocal.Y < 0.0 ? 1.0 : -1.0;
	else                                        RowInwardLocal.X = OutLocal.X < 0.0 ? 1.0 : -1.0;

	// The anchor goes onto the row's nearer front corner: the snap landed it wherever on the end
	// face the cursor was, and a gate set into a row has its front on the row's front line.
	const FVector CornerA = Edge.bAlongX
		? Xf.TransformPosition(FVector(0.0, Edge.FixedCoord, 0.0))
		: Xf.TransformPosition(FVector(Edge.FixedCoord, 0.0, 0.0));
	const FVector CornerB = Edge.bAlongX
		? Xf.TransformPosition(FVector(Size.X, Edge.FixedCoord, 0.0))
		: Xf.TransformPosition(FVector(Edge.FixedCoord, Size.Y, 0.0));
	const FVector Near = FVector::DistSquared2D(StartWorld, CornerA) <= FVector::DistSquared2D(StartWorld, CornerB)
		? CornerA : CornerB;
	StartWorld = FVector(Near.X, Near.Y, StartWorld.Z);
	CurrentWorld = StartWorld;
	SnapPoint = StartWorld;
}

void UHutongGateHouseTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongGateHouseToolProperties>(this);

	// Registered before the params, and the order here is the panel order.
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("GateHouse"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongGateHouseToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	// Presets after the parameters they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

void UHutongGateHouseTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Level)
{
	UHutongGateHouseBuildingComponent::BuildGateHouseMesh(
		Settings ? Settings->Params : FHutongGateHouseParams(),
		BaySide, SizeX, SizeY, OutMesh, Level);
}

void UHutongGateHouseTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;

	UHutongGateHouseBuildingComponent* Building =
		NewObject<UHutongGateHouseBuildingComponent>(Actor, TEXT("GateHouse"));
	if (!Building) return;

	if (Settings) Building->Params = Settings->Params;
	Building->FootprintX = SizeX;
	Building->FootprintY = SizeY;
	Building->BaySide = BaySide;
	if (Appearance)
	{
		Building->Palette = Appearance->Palette;
	}

	// AddInstanceComponent as well as RegisterComponent, or the component is invisible in the Details panel and is not saved with the actor.
	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: the plan outline (and any lights) attach to the actor's root, which the component needs to be live to reach.
	Building->ApplyPlacementAttachments();
}

void UHutongGateHouseTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	FHutongGateHouseParams& P = Settings->Params;
	// Keep clear of the door head, which the generator clamps to Eave - 10.
	const FHutongGateHouseParams::FSizeRange R = P.GetSizeRange();
	const double Lo = FMath::Max3(P.DoorHeadHeight + 20.0, R.EaveMin, P.GetMinEaveHeight());
	double Hi = (R.EaveMax > 0.0) ? R.EaveMax : 5000.0;
	// Set into a row the eave stands where the row put it, and the band's ceiling gives way.
	const double Matched = MatchedGateEave();
	if (Matched > 0.0) Hi = FMath::Max(Hi, Matched);
	// Otherwise held inside the style's band, so - and = cannot walk a 如意門 up to hall height.
	P.EaveHeight = FMath::Clamp(P.EaveHeight + DeltaCm, Lo, FMath::Max(Hi, Lo));
}

double UHutongGateHouseTool::GetPreviewHeight() const
{
	return Settings ? Settings->Params.GetEaveHeight() : 0.0;
}

TArray<FText> UHutongGateHouseTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines.Insert(NSLOCTEXT("HutongGateHouseTool", "HelpSide",
		"After the footprint, move to pick the side the gate faces, then click to place."), 1);
	Lines.Insert(NSLOCTEXT("HutongGateHouseTool", "HelpStyle",
		"The Style below is the difference between the four gate types: it sets how deep the door sits behind the columns."), 2);
	Lines.Insert(NSLOCTEXT("HutongGateHouseTool", "HelpRow",
		"Start the footprint snapped to a placed building and, with Match Neighbouring Row on, the gate takes that row's depth and stands its eave a step above the row's."), 3);
	return Lines;
}

HutongGen::EBaySide UHutongGateHouseTool::ComputeCameraFacingOnAxis() const
{
	if (!GCurrentLevelEditingViewportClient) return BaySide;
	const FVector2D CamLocal =
		WorldXYToLocalRect(GCurrentLevelEditingViewportClient->GetViewLocation());
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	return HutongGen::BaySide::ClosestOnAxis(
		HutongGen::BaySide::IsAlongX(BaySide), CamLocal.X, CamLocal.Y, MinX, MinY, MaxX, MaxY);
}

HutongGen::EBaySide UHutongGateHouseTool::ComputeClosestSide(double Hx, double Hy) const
{
	const FVector2D Local = WorldXYToLocalRect(FVector(Hx, Hy, 0.0));
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);

	// Front or back only.
	return HutongGen::BaySide::ClosestOnAxis(
		HutongGen::BaySide::IsAlongX(BaySide), Local.X, Local.Y, MinX, MinY, MaxX, MaxY);
}

void UHutongGateHouseTool::OnPlacementStarted(const FVector& HitWorld)
{
	BaySide = ComputeDefaultBaySide();
	RowInwardLocal = FVector2D::ZeroVector;

	// Reroll which leaf is shut and how far the other stands.
	if (Settings)
	{
		Settings->Params.RandomSeed = FMath::Rand();
	}

	// Set into a row, the gate runs and faces as the row does, and its ridge stands clear of the
	// row's. The eave is written onto the params, as the height keys write, so - and = still move
	// it from there and the preview reads what will be built.
	if (MatchedRowDepth() > 0.0)
	{
		TakeRowBearing();
		FHutongGateHouseParams& P = Settings->Params;
		P.EaveHeight = FMath::Max(MatchedGateEave(), P.GetMinEaveHeight());
		P.DoorHeadHeight = FMath::Min(P.DoorHeadHeight, P.EaveHeight - 20.0);
	}
}

bool UHutongGateHouseTool::OnRectCommitted(const FVector& HitWorld)
{
	// Whichever of the two 面闊 sides the camera is on.
	BaySide = ComputeCameraFacingOnAxis();
	// Defer to a third click so the facing side can be picked by hovering.
	return false;
}

void UHutongGateHouseTool::OnPlacementHover(const FVector& HitWorld)
{
	if (bRotateModeActive) return;

	// Before the commit the facing is left exactly as the first click set it.
	if (bRectCommitted)
	{
		BaySide = ComputeClosestSide(HitWorld.X, HitWorld.Y);
	}
}

TArray<FText> UHutongGateHouseTool::GetStageNames() const
{
	TArray<FText> Names = Super::GetStageNames();
	Names.Add(NSLOCTEXT("GateHouseTool", "StageFacing", "Facing"));
	return Names;
}

FText UHutongGateHouseTool::GetStagePromptText() const
{
	if (bIsDragging && bRectCommitted && !bRotateModeActive)
	{
		return NSLOCTEXT("HutongGateHouseTool", "PromptSide",
			"Move to the front or the back to pick which way the gate faces (green ticks), "
			"then click to place. The two ends are gable walls and take no doorway.");
	}
	return Super::GetStagePromptText();
}

FString UHutongGateHouseTool::GetPlacementDetail() const
{
	if (!Settings) return FString();

	const UEnum* StyleEnum = StaticEnum<EHutongGateStyle>();
	const FText Style = StyleEnum
		? StyleEnum->GetDisplayNameTextByValue(static_cast<int64>(Settings->Params.Style))
		: FText::GetEmpty();

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
	const double Frontage = bAlongX ? (MaxX - MinX) : (MaxY - MinY);
	const double Depth = bAlongX ? (MaxY - MinY) : (MaxX - MinX);

	const UHutongBuildingComponent* RowBuilding = MatchedRowDepth() > 0.0 ? AnchorSnapBuilding.Get() : nullptr;
	const FString Row = RowBuilding
		? FString::Printf(TEXT(" · set into row (ridge %.0f, gate ridge %.0f)"), RowBuilding->GetRidgeHeight(),
			HutongGen::Ridge::Gate(Settings->Params, Depth))
		: FString();

	return FString::Printf(TEXT("%s · frontage (面闊) %.0f / depth (進深) %.0f cm%s%s"),
		*Style.ToString(), Frontage, Depth,
		Settings->Params.bConstrainToHistoricalSize ? TEXT("") : TEXT(" (free)"), *Row);
}

void UHutongGateHouseTool::Render(IToolsContextRenderAPI* RenderAPI)
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

	// Which way the gate faces is the one thing the user has to get right at placement time, and a single line along the edge was too quiet to read against the footprint rectangle.
	const FLinearColor Green(0.25f, 1.0f, 0.45f);
	const FLinearColor Dark(0.02f, 0.35f, 0.12f);

	const HutongGen::BaySide::FEdge Edge =
		HutongGen::BaySide::GetEdge(BaySide, MinX, MinY, MaxX, MaxY);
	const double SpanMin = Edge.bAlongX ? MinX : MinY;
	const double SpanMax = Edge.bAlongX ? MaxX : MaxY;
	const double Span = SpanMax - SpanMin;

	auto EdgePoint = [&](double Along, double Out)
	{
		const double Fixed = Edge.FixedCoord + (Edge.bAlongX ? Edge.OutDir.Y : Edge.OutDir.X) * Out;
		return Edge.bAlongX ? LocalRectToWorld(Along, Fixed) : LocalRectToWorld(Fixed, Along);
	};

	const double Reach = FMath::Clamp(0.22 * FMath::Min(SizeX, SizeY), 40.0, 220.0);

	// A heavy double bar on the edge itself.
	DrawPreviewLine(PDI, EdgePoint(SpanMin, 0.0), EdgePoint(SpanMax, 0.0), Green, 14.0f);
	DrawPreviewLine(PDI, EdgePoint(SpanMin, 0.32 * Reach), EdgePoint(SpanMax, 0.32 * Reach),
		Dark, 6.0f);

	// Hatching between the two, so the facing side reads as a filled band rather than a line.
	const int32 Hatches = FMath::Clamp(FMath::RoundToInt32(Span / 45.0), 4, 40);
	for (int32 i = 0; i <= Hatches; ++i)
	{
		const double A = FMath::Lerp(SpanMin, SpanMax, double(i) / double(Hatches));
		DrawPreviewLine(PDI, EdgePoint(A, 0.0), EdgePoint(A, 0.32 * Reach), Green, 3.0f);
	}

	// Three arrows pointing out the way the gate faces.
	for (int32 i = 1; i <= 3; ++i)
	{
		const double A = FMath::Lerp(SpanMin, SpanMax, double(i) / 4.0);
		const double Head = 0.95 * Reach;
		const double Barb = FMath::Min(0.28 * Reach, 0.12 * Span);

		DrawPreviewLine(PDI, EdgePoint(A, 0.32 * Reach), EdgePoint(A, Head), Green, 7.0f);
		DrawPreviewLine(PDI, EdgePoint(A, Head), EdgePoint(A - Barb, Head - Barb), Green, 7.0f);
		DrawPreviewLine(PDI, EdgePoint(A, Head), EdgePoint(A + Barb, Head - Barb), Green, 7.0f);
	}
}
