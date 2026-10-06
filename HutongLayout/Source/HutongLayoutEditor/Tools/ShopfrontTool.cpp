#include "Tools/ShopfrontTool.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"
#include "LevelEditorViewport.h"
#include "SceneManagement.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongShopfrontToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongShopfrontTool>(SceneState.ToolManager);
}

void UHutongShopfrontTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongShopfrontToolProperties>(this);

	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("Shopfront"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongShopfrontToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	// Panel order is registration order: presets after the params they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

void UHutongShopfrontTool::OnPlacementStarted(const FVector& HitWorld)
{
	BaySide = ComputeDefaultBaySide();
	// Bay count depends on the frontage being drawn; reset.
	if (Settings) Settings->Params.BayCountOverride = 0;
}

void UHutongShopfrontTool::CancelPlacement()
{
	Super::CancelPlacement();
	// Override applies only to the footprint it was set on.
	if (Settings) Settings->Params.BayCountOverride = 0;
}

bool UHutongShopfrontTool::OnRectCommitted(const FVector& HitWorld)
{
	// Third click picks the facing side by hover.
	return false;
}

void UHutongShopfrontTool::OnPlacementHover(const FVector& HitWorld)
{
	if (bRotateModeActive) return;
	BaySide = bRectCommitted
		? ComputeClosestSide(HitWorld.X, HitWorld.Y)
		: ComputeDefaultBaySide();
}

void UHutongShopfrontTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Level)
{
	const FHutongShopfrontParams P = Settings ? Settings->Params : FHutongShopfrontParams();
	UHutongShopfrontBuildingComponent::BuildShopfrontMesh(
		P, BaySide, P.BayCountOverride, SizeX, SizeY, OutMesh, Level);
}

void UHutongShopfrontTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;

	UHutongShopfrontBuildingComponent* Building =
		NewObject<UHutongShopfrontBuildingComponent>(Actor, TEXT("Shopfront"));
	if (!Building) return;

	if (Settings) Building->Params = Settings->Params;
	// Previewed count copied to the component.
	if (Settings) Building->BayCountOverride = Settings->Params.BayCountOverride;
	// Component stores rect extents and side, not frontage/depth.
	Building->FootprintX = SizeX;
	Building->FootprintY = SizeY;
	Building->BaySide = BaySide;
	if (Appearance) Building->Palette = Appearance->Palette;

	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: plan outline and lights attach to the actor root, which needs the component live.
	Building->ApplyPlacementAttachments();
}

void UHutongShopfrontTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	Settings->Params.EaveHeight = FMath::Clamp(Settings->Params.EaveHeight + DeltaCm, 120.0, 700.0);
}

double UHutongShopfrontTool::GetPreviewHeight() const
{
	return Settings ? Settings->Params.GetEaveHeight() : 0.0;
}

FString UHutongShopfrontTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const FHutongShopfrontParams& P = Settings->Params;
	const int32 Bays = ComputeBayCountForSide();
	return FString::Printf(TEXT("Shopfront (鋪面房) · %d bays (間) · %d open"),
		Bays, P.GetOpenBayCount(Bays));
}

TArray<FText> UHutongShopfrontTool::GetStageNames() const
{
	TArray<FText> Names = Super::GetStageNames();
	Names.Add(NSLOCTEXT("ShopfrontTool", "StageFacing", "Facing"));
	return Names;
}

FText UHutongShopfrontTool::GetStagePromptText() const
{
	if (bIsDragging && bRectCommitted && !bRotateModeActive)
	{
		return NSLOCTEXT("HutongShopfrontTool", "PromptSide",
			"Move to pick which side faces the street (green ticks), then click to place. "
			"[ and ] change the bay count.");
	}
	return Super::GetStagePromptText();
}

void UHutongShopfrontTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);
	if (!bIsDragging || RenderAPI == nullptr || !Settings) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	if (MaxX - MinX < 1.0 || MaxY - MinY < 1.0) return;

	// Bays as the generator spaces them.
	const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
	const double Span = bAlongX ? MaxX - MinX : MaxY - MinY;
	const double Across = bAlongX ? MaxY - MinY : MaxX - MinX;
	const int32 N = FMath::Max(1, ComputeBayCountForSide());
	const FHutongShopfrontParams& P = Settings->Params;
	TArray<double> Bounds;
	for (int32 i = 0; i <= N; ++i) Bounds.Add(P.GetBayBoundary(i, N, Span, P.GetColumnRadiusFor(Span, Across)));
	DrawRectBaysAndFacing(PDI, BaySide, MinX, MinY, MaxX, MaxY, Bounds);
}

FText UHutongShopfrontTool::GetKeyHintText() const
{
	return FText::Format(NSLOCTEXT("ShopfrontTool", "KeyHint", "[ ] bays · {0}"), Super::GetKeyHintText());
}

TArray<FText> UHutongShopfrontTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongShopfrontTool", "HelpDrag",
		"Click to anchor, move, click to fix the footprint, move toward the street side, click to place.");
	Lines.Insert(NSLOCTEXT("HutongShopfrontTool", "HelpOpen",
		"Open Bays takes the board doors (排板門) out of that many middle bays; 0 shuts the shop."), 1);
	return Lines;
}

int32 UHutongShopfrontTool::ComputeBayCountForSide() const
{
	if (!Settings) return 1;
	const FHutongShopfrontParams& P = Settings->Params;
	if (P.BayCountOverride > 0) return P.BayCountOverride;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
	const double Frontage = bAlongX ? (MaxX - MinX) : (MaxY - MinY);
	return HutongGen::ComputeBayCount(Frontage, P.MinBayWidth, P.MaxBayWidth);
}

void UHutongShopfrontTool::AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse)
{
	if (!Settings || Delta == 0) return;
	// Seeded from the count on screen.
	const int32 Current = ComputeBayCountForSide();
	Settings->Params.BayCountOverride = FMath::Clamp(Current + Delta, 1, 24);
}
