#include "Tools/UnknownTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"

UInteractiveTool* UHutongUnknownToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongUnknownTool>(SceneState.ToolManager);
}

void UHutongUnknownTool::Setup()
{
	Super::Setup();
	// Nothing is built, so detail and materials have nothing to say.
	SetToolPropertySourceEnabled(DetailSettings, false);
	SetToolPropertySourceEnabled(Appearance, false);
}

void UHutongUnknownTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;

	UHutongUnknownBuildingComponent* Building =
		NewObject<UHutongUnknownBuildingComponent>(Actor, TEXT("Unknown"));
	if (!Building) return;

	Building->FootprintX = SizeX;
	Building->FootprintY = SizeY;

	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	Building->ApplyPlacementAttachments();
}

FString UHutongUnknownTool::GetPlacementDetail() const
{
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	return FString::Printf(TEXT("Unknown (未知) · %.0f × %.0f cm"), MaxX - MinX, MaxY - MinY);
}

TArray<FText> UHutongUnknownTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	// No height to change.
	Lines.RemoveAt(2);
	Lines[0] = NSLOCTEXT("HutongUnknownTool", "HelpSpan",
		"Trace a footprint whose type is not known.");
	Lines.Insert(NSLOCTEXT("HutongUnknownTool", "HelpWhat",
		"Only the outline and its metadata are kept; press T on it later to give it a type."), 1);
	return Lines;
}
