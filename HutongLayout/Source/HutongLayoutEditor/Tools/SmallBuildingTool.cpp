#include "Tools/SmallBuildingTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/BaySide.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"
#include "SceneManagement.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongSmallBuildingToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongSmallBuildingTool>(SceneState.ToolManager);
}

void UHutongSmallBuildingTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongSmallBuildingToolProperties>(this);
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("Small"), Settings, GET_MEMBER_NAME_CHECKED(UHutongSmallBuildingToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

FHutongSmallBuildingParams UHutongSmallBuildingTool::SizedParams(double SizeX, double SizeY) const
{
	return UHutongSmallBuildingComponent::Sized(Settings ? Settings->Params : FHutongSmallBuildingParams(),
		BaySide, BayCountOverride, SizeX, SizeY);
}

void UHutongSmallBuildingTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh, EHutongDetail Level)
{
	UHutongSmallBuildingComponent::BuildSmallBuildingMesh(Settings ? Settings->Params : FHutongSmallBuildingParams(),
		BaySide, BayCountOverride, SizeX, SizeY, OutMesh, Level);
}

void UHutongSmallBuildingTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;
	UHutongSmallBuildingComponent* Building = NewObject<UHutongSmallBuildingComponent>(Actor, TEXT("SmallBuilding"));
	if (!Building) return;
	if (Settings) Building->Params = Settings->Params;
	Building->FootprintX = SizeX;
	Building->FootprintY = SizeY;
	Building->BaySide = BaySide;
	Building->BayCountOverride = BayCountOverride;
	if (Appearance) Building->Palette = Appearance->Palette;
	StampDetail(Building);
	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	Building->ApplyPlacementAttachments();
}

void UHutongSmallBuildingTool::AdjustHeight(double DeltaCm)
{
	if (Settings) Settings->Params.EaveHeight = FMath::Clamp(Settings->Params.GetEaveHeight() + DeltaCm, 120.0, 600.0);
}

double UHutongSmallBuildingTool::GetPreviewHeight() const
{
	return Settings ? Settings->Params.GetEaveHeight() : 0.0;
}

void UHutongSmallBuildingTool::FlipFacing()
{
	// A quarter turn each press: a hut at a lane mouth may face any of its four sides.
	BaySide = static_cast<EHutongBaySide>(((int32)BaySide + 1) % 4);
}

void UHutongSmallBuildingTool::AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse)
{
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const int32 Drawn = SizedParams(MaxX - MinX, MaxY - MinY).GetBayCount();
	BayCountOverride = FMath::Clamp((BayCountOverride > 0 ? BayCountOverride : Drawn) + Delta, 1, 8);
}

void UHutongSmallBuildingTool::OnPlacementStarted(const FVector& HitWorld)
{
	BaySide = ComputeDefaultBaySide();
	// Bay count depends on the frontage drawn, so it never carries between placements.
	BayCountOverride = 0;
}

void UHutongSmallBuildingTool::CancelPlacement()
{
	BayCountOverride = 0;
	Super::CancelPlacement();
}

FString UHutongSmallBuildingTool::GetPlacementDetail() const
{
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const FHutongSmallBuildingParams P = SizedParams(MaxX - MinX, MaxY - MinY);
	return FString::Printf(TEXT("Small building (小房) · %d %s (間) · eave %.0f cm"),
		P.GetBayCount(), P.GetBayCount() == 1 ? TEXT("bay") : TEXT("bays"), P.GetEaveHeight());
}

TArray<FText> UHutongSmallBuildingTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongSmallBuildingTool", "HelpSpan",
		"Drag out a small freestanding building: a guard post (堆撥房), shed (棚), lone room or shrine (土地廟). Pick one in Presets.");
	Lines.Insert(NSLOCTEXT("HutongSmallBuildingTool", "HelpKeys",
		"F turns the front a quarter round while placing; [ and ] set the number of bays."), 1);
	return Lines;
}

void UHutongSmallBuildingTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);
	if (!bIsDragging || !RenderAPI) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	if (MaxX - MinX < 1.0 || MaxY - MinY < 1.0) return;
	const FHutongSmallBuildingParams P = SizedParams(MaxX - MinX, MaxY - MinY);
	const int32 N = P.GetBayCount();
	TArray<double> Bounds;
	for (int32 i = 0; i <= N; ++i) Bounds.Add(P.GetBayBoundary(i, N));
	DrawRectBaysAndFacing(PDI, BaySide, MinX, MinY, MaxX, MaxY, Bounds,
		P.Front == EHutongSmallFront::Door ? P.GetDoorBay(N) : INDEX_NONE);
}
