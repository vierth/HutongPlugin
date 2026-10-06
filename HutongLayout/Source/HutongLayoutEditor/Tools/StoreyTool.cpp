#include "Tools/StoreyTool.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"
#include "LevelEditorViewport.h"
#include "SceneManagement.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongStoreyToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongStoreyTool>(SceneState.ToolManager);
}

void UHutongStoreyTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongStoreyToolProperties>(this);

	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("Storey"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongStoreyToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	// Panel order is registration order: presets after the params they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

void UHutongStoreyTool::OnPlacementStarted(const FVector& HitWorld)
{
	BaySide = ComputeDefaultBaySide();
	// Bay count depends on the frontage drawn.
	if (Settings) Settings->Params.BayCountOverride = 0;
}

void UHutongStoreyTool::CancelPlacement()
{
	Super::CancelPlacement();
	// The override applies only to the footprint being drawn.
	if (Settings) Settings->Params.BayCountOverride = 0;
}

bool UHutongStoreyTool::OnRectCommitted(const FVector& HitWorld)
{
	// Defer to a third click so the facing side is picked by hovering.
	return false;
}

void UHutongStoreyTool::OnPlacementHover(const FVector& HitWorld)
{
	if (bRotateModeActive) return;
	BaySide = bRectCommitted
		? ComputeClosestSide(HitWorld.X, HitWorld.Y)
		: ComputeDefaultBaySide();
}

void UHutongStoreyTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Level)
{
	const FHutongStoreyParams P = Settings ? Settings->Params : FHutongStoreyParams();
	UHutongStoreyBuildingComponent::BuildStoreyMesh(
		P, BaySide, P.BayCountOverride, SizeX, SizeY, OutMesh, Level);
}

void UHutongStoreyTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;

	UHutongStoreyBuildingComponent* Building =
		NewObject<UHutongStoreyBuildingComponent>(Actor, TEXT("Storey"));
	if (!Building) return;

	if (Settings) Building->Params = Settings->Params;
	// The previewed count, mirrored onto the component.
	if (Settings) Building->BayCountOverride = Settings->Params.BayCountOverride;
	// The component keeps the rect's extents and side, not frontage/depth.
	Building->FootprintX = SizeX;
	Building->FootprintY = SizeY;
	Building->BaySide = BaySide;
	if (Appearance) Building->Palette = Appearance->Palette;

	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: the plan outline and lights attach to the actor's root, which needs the component live.
	Building->ApplyPlacementAttachments();
}

void UHutongStoreyTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	// Keys move the upper storey: a 樓 rises by its own storey; the street sets the ground floor.
	Settings->Params.UpperStoreyHeight =
		FMath::Clamp(Settings->Params.UpperStoreyHeight + DeltaCm, 140.0, 500.0);
}

double UHutongStoreyTool::GetPreviewHeight() const
{
	return Settings ? Settings->Params.GetEaveHeight() : 0.0;
}

FString UHutongStoreyTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const FHutongStoreyParams& P = Settings->Params;
	const int32 Bays = ComputeBayCountForSide();
	return FString::Printf(TEXT("Multi-story building (樓) · %d bays (間) · %d open · eave %.0f cm"),
		Bays, P.GetOpenBayCount(Bays), P.GetEaveHeight());
}

TArray<FText> UHutongStoreyTool::GetStageNames() const
{
	TArray<FText> Names = Super::GetStageNames();
	Names.Add(NSLOCTEXT("StoreyTool", "StageFacing", "Facing"));
	return Names;
}

FText UHutongStoreyTool::GetStagePromptText() const
{
	if (bIsDragging && bRectCommitted && !bRotateModeActive)
	{
		return NSLOCTEXT("HutongStoreyTool", "PromptSide",
			"Move to pick which side faces the street (green ticks), then click to place. "
			"[ and ] change the bay count.");
	}
	return Super::GetStagePromptText();
}

void UHutongStoreyTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);
	if (!bIsDragging || RenderAPI == nullptr) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	if (MaxX - MinX < 1.0 || MaxY - MinY < 1.0) return;

	const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
	const double Span = bAlongX ? MaxX - MinX : MaxY - MinY;
	const double Across = bAlongX ? MaxY - MinY : MaxX - MinX;
	TArray<double> Bounds;
	if (Settings)
	{
		const int32 N = FMath::Max(1, ComputeBayCountForSide());
		const FHutongStoreyParams& P = Settings->Params;
		for (int32 i = 0; i <= N; ++i) Bounds.Add(P.GetBayBoundary(i, N, Span, P.GetColumnRadiusFor(Span, Across)));
	}
	DrawRectBaysAndFacing(PDI, BaySide, MinX, MinY, MaxX, MaxY, Bounds);
}

FText UHutongStoreyTool::GetKeyHintText() const
{
	return FText::Format(NSLOCTEXT("StoreyTool", "KeyHint", "[ ] bays · {0}"), Super::GetKeyHintText());
}

TArray<FText> UHutongStoreyTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongStoreyTool", "HelpDrag",
		"Click to anchor, move, click to fix the footprint, move toward the street side, click to place.");
	Lines.Insert(NSLOCTEXT("HutongStoreyTool", "HelpOpen",
		"Open Bays opens the shop below; - and = raise the upper story."), 1);
	return Lines;
}

int32 UHutongStoreyTool::ComputeBayCountForSide() const
{
	if (!Settings) return 1;
	const FHutongStoreyParams& P = Settings->Params;
	if (P.BayCountOverride > 0) return P.BayCountOverride;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
	const double Frontage = bAlongX ? (MaxX - MinX) : (MaxY - MinY);
	return HutongGen::ComputeBayCount(Frontage, P.MinBayWidth, P.MaxBayWidth);
}

void UHutongStoreyTool::AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse)
{
	if (!Settings || Delta == 0) return;
	// Seeded from the count currently on screen.
	const int32 Current = ComputeBayCountForSide();
	Settings->Params.BayCountOverride = FMath::Clamp(Current + Delta, 1, 24);
}
