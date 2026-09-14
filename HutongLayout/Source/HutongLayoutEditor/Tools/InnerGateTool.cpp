#include "Tools/InnerGateTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"
#include "LevelEditorViewport.h"
#include "SceneManagement.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongInnerGateToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongInnerGateTool>(SceneState.ToolManager);
}

#if WITH_EDITOR
void UHutongInnerGateToolProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// The head first, then the eave above it.
	Params.DoorHeadHeight = FMath::Max(Params.DoorHeadHeight,
		HutongGen::Passage::MinHeadZ(Params.FloorHeight, Params.ThresholdHeight));

	const FHutongInnerGateParams::FSizeRange R = Params.GetSizeRange();
	if (R.EaveMax > 0.0)
	{
		Params.EaveHeight = FMath::Clamp(Params.EaveHeight, R.EaveMin, R.EaveMax);
	}
	Params.EaveHeight = FMath::Max(Params.EaveHeight, Params.GetMinEaveHeight());

	// The head has to stay under the beam the hanging posts drop from, whatever the eave just did.
	Params.DoorHeadHeight = FMath::Min(Params.DoorHeadHeight, Params.EaveHeight - 45.0);
}
#endif

void UHutongInnerGateTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	Super::GetEffectiveRectBounds(OutMinX, OutMinY, OutMaxX, OutMaxY);

	if (!Settings) return;
	const FHutongInnerGateParams::FSizeRange R = Settings->Params.GetSizeRange();
	if (R.FrontageMax <= 0.0) return;

	// One end of each extent is the anchor the drag started from.
	auto ClampAxis = [](double& Lo, double& Hi, double MinLen, double MaxLen)
	{
		const double Want = FMath::Clamp(Hi - Lo, MinLen, MaxLen);
		if (Hi > 0.0) Hi = Lo + Want;
		else          Lo = Hi - Want;
	};

	if (HutongGen::BaySide::IsAlongX(BaySide))
	{
		ClampAxis(OutMinX, OutMaxX, R.FrontageMin, R.FrontageMax);
		ClampAxis(OutMinY, OutMaxY, R.DepthMin, R.DepthMax);
	}
	else
	{
		ClampAxis(OutMinY, OutMaxY, R.FrontageMin, R.FrontageMax);
		ClampAxis(OutMinX, OutMaxX, R.DepthMin, R.DepthMax);
	}
}

void UHutongInnerGateTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongInnerGateToolProperties>(this);

	// Registered before the params, and the order here is the panel order.
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("InnerGate"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongInnerGateToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	// Presets after the parameters they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

HutongGen::EBaySide UHutongInnerGateTool::ComputeClosestSide(double Hx, double Hy) const
{
	const FVector2D Local = WorldXYToLocalRect(FVector(Hx, Hy, 0.0));
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	// Front or back only, as on the gate house: the two ends are the 進深 and carry no doorway.
	return HutongGen::BaySide::ClosestOnAxis(
		HutongGen::BaySide::IsAlongX(BaySide), Local.X, Local.Y, MinX, MinY, MaxX, MaxY);
}

void UHutongInnerGateTool::OnPlacementStarted(const FVector& HitWorld)
{
	BaySide = ComputeDefaultBaySide();
}

bool UHutongInnerGateTool::OnRectCommitted(const FVector& HitWorld)
{
	// Defer to a third click so the facing side can be picked by hovering.
	return false;
}

void UHutongInnerGateTool::OnPlacementHover(const FVector& HitWorld)
{
	if (bRotateModeActive) return;
	// Left alone until the rect is committed.
	if (bRectCommitted)
	{
		BaySide = ComputeClosestSide(HitWorld.X, HitWorld.Y);
	}
}

void UHutongInnerGateTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Level)
{
	UHutongInnerGateBuildingComponent::BuildInnerGateMesh(
		Settings ? Settings->Params : FHutongInnerGateParams(),
		BaySide, SizeX, SizeY, OutMesh, Level);
}

void UHutongInnerGateTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;

	UHutongInnerGateBuildingComponent* Building =
		NewObject<UHutongInnerGateBuildingComponent>(Actor, TEXT("InnerGate"));
	if (!Building) return;

	if (Settings) Building->Params = Settings->Params;
	const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
	Building->Width = bAlongX ? SizeX : SizeY;
	Building->Depth = bAlongX ? SizeY : SizeX;
	Building->BaySide = BaySide;
	if (Appearance) Building->Palette = Appearance->Palette;

	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: the plan outline (and any lights) attach to the actor's root, which the component needs to be live to reach.
	Building->ApplyPlacementAttachments();
}

void UHutongInnerGateTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	// The floor is the height a walkable doorway needs.
	Settings->Params.EaveHeight = FMath::Clamp(Settings->Params.EaveHeight + DeltaCm,
		FMath::Max(120.0, Settings->Params.GetMinEaveHeight()), 600.0);
}

double UHutongInnerGateTool::GetPreviewHeight() const
{
	return Settings ? Settings->Params.GetEaveHeight() : 0.0;
}

FString UHutongInnerGateTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const FHutongInnerGateParams& P = Settings->Params;
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
	return FString::Printf(TEXT("Inner gate (垂花門) · frontage (面闊) %.0f / depth (進深) %.0f cm%s%s"),
		bAlongX ? (MaxX - MinX) : (MaxY - MinY),
		bAlongX ? (MaxY - MinY) : (MaxX - MinX),
		P.bHasHangingPosts ? TEXT(" · hanging lotus posts (垂蓮柱)") : TEXT(""),
		P.bConstrainToHistoricalSize ? TEXT("") : TEXT(" (free)"));
}

TArray<FText> UHutongInnerGateTool::GetStageNames() const
{
	TArray<FText> Names = Super::GetStageNames();
	Names.Add(NSLOCTEXT("InnerGateTool", "StageFacing", "Facing"));
	return Names;
}

FText UHutongInnerGateTool::GetStagePromptText() const
{
	if (bIsDragging && bRectCommitted && !bRotateModeActive)
	{
		return NSLOCTEXT("HutongInnerGateTool", "PromptSide",
			"Move to the front or the back to pick which way the hanging lotus posts (垂蓮柱) face (green ticks), "
			"then click to place.");
	}
	return Super::GetStagePromptText();
}

void UHutongInnerGateTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);
	if (!bIsDragging || RenderAPI == nullptr) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	if (MaxX - MinX < 1.0 || MaxY - MinY < 1.0) return;

	// A bar along the facing edge.
	const HutongGen::BaySide::FEdge Edge =
		HutongGen::BaySide::GetEdge(BaySide, MinX, MinY, MaxX, MaxY);
	const double SpanMin = Edge.bAlongX ? MinX : MinY;
	const double SpanMax = Edge.bAlongX ? MaxX : MaxY;

	const FLinearColor Green(0.25f, 1.0f, 0.45f);
	const int32 Ticks = 9;
	for (int32 i = 0; i < Ticks; ++i)
	{
		const double A0 = FMath::Lerp(SpanMin, SpanMax, (i + 0.15) / Ticks);
		const double A1 = FMath::Lerp(SpanMin, SpanMax, (i + 0.85) / Ticks);
		const FVector P0 = Edge.bAlongX
			? LocalRectToWorld(A0, Edge.FixedCoord) : LocalRectToWorld(Edge.FixedCoord, A0);
		const FVector P1 = Edge.bAlongX
			? LocalRectToWorld(A1, Edge.FixedCoord) : LocalRectToWorld(Edge.FixedCoord, A1);
		DrawPreviewLine(PDI, P0, P1, Green, 7.0f);
	}
}

TArray<FText> UHutongInnerGateTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongInnerGateTool", "HelpDrag",
		"Click to anchor, move, click to fix the footprint, move to pick which way it faces, click to place.");
	Lines.Insert(NSLOCTEXT("HutongInnerGateTool", "HelpWhat",
		"An inner gate (垂花門) divides the outer courtyard from the inner one. The hanging lotus posts (垂蓮柱) hang on its outer face."), 1);
	return Lines;
}
