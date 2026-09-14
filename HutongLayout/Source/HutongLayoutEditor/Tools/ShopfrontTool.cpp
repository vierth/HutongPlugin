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

	// Registered before the params, and the order here is the panel order.
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("Shopfront"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongShopfrontToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	// Presets after the parameters they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

void UHutongShopfrontTool::OnPlacementStarted(const FVector& HitWorld)
{
	BaySide = ComputeDefaultBaySide();
	// The right bay count depends on the frontage being drawn.
	if (Settings) Settings->Params.BayCountOverride = 0;
}

void UHutongShopfrontTool::CancelPlacement()
{
	Super::CancelPlacement();
	// The override is only meaningful for the footprint that was being drawn.
	if (Settings) Settings->Params.BayCountOverride = 0;
}

bool UHutongShopfrontTool::OnRectCommitted(const FVector& HitWorld)
{
	// Defer to a third click so the facing side can be picked by hovering.
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
	// The count that was previewed, mirrored onto the component.
	if (Settings) Building->BayCountOverride = Settings->Params.BayCountOverride;
	// The component keeps the rect's own extents and the side, not the frontage/depth pair.
	Building->FootprintX = SizeX;
	Building->FootprintY = SizeY;
	Building->BaySide = BaySide;
	if (Appearance) Building->Palette = Appearance->Palette;

	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: the plan outline (and any lights) attach to the actor's root, which the component needs to be live to reach.
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
		Bays, FMath::Clamp(P.OpenBayCount, 0, Bays));
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
	if (!bIsDragging || RenderAPI == nullptr) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	if (MaxX - MinX < 1.0 || MaxY - MinY < 1.0) return;

	// A bar along the street-facing edge.
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

FText UHutongShopfrontTool::GetKeyHintText() const
{
	return FText::Format(NSLOCTEXT("ShopfrontTool", "KeyHint", "[ ] bays · {0}"), Super::GetKeyHintText());
}

TArray<FText> UHutongShopfrontTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongShopfrontTool", "HelpDrag",
		"Click to anchor, move, click to fix the footprint, move to pick the side facing the street, click to place.");
	Lines.Insert(NSLOCTEXT("HutongShopfrontTool", "HelpOpen",
		"Open Bays below lifts the board doors (排板門) out of that many bays, counted from the middle. Zero boards the shop up for the night."), 1);
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
	// Seeded from the count currently on screen.
	const int32 Current = ComputeBayCountForSide();
	Settings->Params.BayCountOverride = FMath::Clamp(Current + Delta, 1, 24);
}
