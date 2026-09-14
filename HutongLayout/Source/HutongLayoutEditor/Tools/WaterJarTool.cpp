#include "Tools/WaterJarTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongWaterJarToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongWaterJarTool>(SceneState.ToolManager);
}

void UHutongWaterJarTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongWaterJarToolProperties>(this);

	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("WaterJar"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongWaterJarToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	// Presets after the parameters they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

double UHutongWaterJarTool::DraggedBellyDiameter() const
{
	const FHutongWaterJarParams P = Settings ? Settings->Params : FHutongWaterJarParams();
	if (!bIsDragging && !bRectCommitted) return FMath::Max(P.BellyDiameter, 20.0);

	// The larger extent, so the jar follows the hand rather than the shorter of two axes the user is not thinking about.
	const FVector2D Local = WorldXYToLocalRect(CurrentWorld);
	const double Reach = FMath::Max(FMath::Abs(Local.X), FMath::Abs(Local.Y));
	return FMath::Clamp(Reach, 20.0, 140.0);
}

void UHutongWaterJarTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	// Square, and at the object's true extent. A jar has one dimension.
	FHutongWaterJarParams P = Settings ? Settings->Params : FHutongWaterJarParams();
	P.BellyDiameter = DraggedBellyDiameter();
	const double Span = FMath::Max(P.GetFootprint(), 1.0);

	const FVector2D Local = WorldXYToLocalRect(CurrentWorld);
	const double SX = (Local.X < 0.0) ? -Span : Span;
	const double SY = (Local.Y < 0.0) ? -Span : Span;

	OutMinX = FMath::Min(0.0, SX); OutMaxX = FMath::Max(0.0, SX);
	OutMinY = FMath::Min(0.0, SY); OutMaxY = FMath::Max(0.0, SY);
}

void UHutongWaterJarTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Level)
{
	FHutongWaterJarParams P = Settings ? Settings->Params : FHutongWaterJarParams();
	P.BellyDiameter = DraggedBellyDiameter();
	UHutongWaterJarBuildingComponent::BuildWaterJarMesh(P, OutMesh, Level);
}

void UHutongWaterJarTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;

	UHutongWaterJarBuildingComponent* Building =
		NewObject<UHutongWaterJarBuildingComponent>(Actor, TEXT("WaterJar"));
	if (!Building) return;

	if (Settings) Building->Params = Settings->Params;
	// The dragged belly, not the panel's.
	Building->Params.BellyDiameter = DraggedBellyDiameter();
	if (Appearance) Building->Palette = Appearance->Palette;

	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: the plan outline (and any lights) attach to the actor's root, which the component needs to be live to reach.
	Building->ApplyPlacementAttachments();
}

void UHutongWaterJarTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	Settings->Params.Height =
		FMath::Clamp(Settings->Params.Height + 0.2 * DeltaCm, 15.0, 140.0);
}

double UHutongWaterJarTool::GetPreviewHeight() const
{
	if (!Settings) return 0.0;
	const FHutongWaterJarParams& P = Settings->Params;
	return P.Height + (P.bHasBase ? FMath::Max(P.BaseHeight, 0.0) : 0.0);
}

FString UHutongWaterJarTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	return FString::Printf(TEXT("Water jar (魚缸) · diameter (徑) %.0f cm · height (高) %.0f cm"),
		DraggedBellyDiameter(), Settings->Params.Height);
}

TArray<FText> UHutongWaterJarTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongWaterJarTool", "HelpSpan",
		"Drag to set the belly diameter — the rect is held square, since a jar has only the one dimension. "
		"- and = change its height.");
	Lines.Insert(NSLOCTEXT("HutongWaterJarTool", "HelpWhat",
		"Courtyard furnishing (天棚魚缸石榴樹) — matting overhead, the fish jar, the pomegranate. It stands on the axis "
		"in front of the main hall (正房), and the courtyard is arranged around it."), 1);
	return Lines;
}
