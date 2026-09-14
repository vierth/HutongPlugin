#include "Tools/FlowerBedTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongFlowerBedToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongFlowerBedTool>(SceneState.ToolManager);
}

void UHutongFlowerBedTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongFlowerBedToolProperties>(this);

	// Presets above the params, as everywhere: registration order is panel order, and a preset is how a type is picked.
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("FlowerBed"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongFlowerBedToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	// Presets after the parameters they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

void UHutongFlowerBedTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Level)
{
	const FHutongFlowerBedParams P = Settings ? Settings->Params : FHutongFlowerBedParams();
	UHutongFlowerBedBuildingComponent::BuildFlowerBedMesh(P, SizeX, SizeY, OutMesh, Level);
}

void UHutongFlowerBedTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;

	UHutongFlowerBedBuildingComponent* Building =
		NewObject<UHutongFlowerBedBuildingComponent>(Actor, TEXT("FlowerBed"));
	if (!Building) return;

	if (Settings) Building->Params = Settings->Params;
	Building->FootprintX = SizeX;
	Building->FootprintY = SizeY;
	if (Appearance) Building->Palette = Appearance->Palette;

	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: the plan outline (and any lights) attach to the actor's root, which the component needs to be live to reach.
	Building->ApplyPlacementAttachments();
}

void UHutongFlowerBedTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	// A tenth of the step, like the path's.
	Settings->Params.KerbHeight =
		FMath::Clamp(Settings->Params.KerbHeight + 0.1 * DeltaCm, 3.0, 80.0);
}

double UHutongFlowerBedTool::GetPreviewHeight() const
{
	return Settings ? Settings->Params.KerbHeight : 0.0;
}

FString UHutongFlowerBedTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	return FString::Printf(TEXT("Flower bed (花池) · %.0f × %.0f cm · edge (沿) %.0f cm"),
		MaxX - MinX, MaxY - MinY, Settings->Params.KerbHeight);
}

TArray<FText> UHutongFlowerBedTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongFlowerBedTool", "HelpSpan",
		"Drag out the bed. Both axes are free — a flower bed (花池) is whatever shape the ground beside the paving leaves.");
	Lines.Insert(NSLOCTEXT("HutongFlowerBedTool", "HelpWhat",
		"A courtyard is swept earth with planting in raised beds, not lawn and not pavement. "
		"The plant itself is a foliage asset; this is the masonry that says where it goes."), 1);
	return Lines;
}
