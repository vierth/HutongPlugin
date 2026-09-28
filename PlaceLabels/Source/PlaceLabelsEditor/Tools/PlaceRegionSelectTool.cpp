#include "Tools/PlaceRegionSelectTool.h"

#include "InteractiveToolManager.h"

#define LOCTEXT_NAMESPACE "PlaceRegionSelectTool"

void UPlaceRegionSelectTool::Setup()
{
	UInteractiveTool::Setup();
	// No behaviours or property sets: clicks fall through to the level editor.
}

FText UPlaceRegionSelectTool::GetStagePromptText() const
{
	return LOCTEXT("SelectPrompt",
		"Click a region to select it — the form below follows what is selected, and the ordinary "
		"gizmo moves it. Pen draws a new one; Edit reshapes this one.");
}

UInteractiveTool* UPlaceRegionSelectToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UPlaceRegionSelectTool>(SceneState.ToolManager);
}

#undef LOCTEXT_NAMESPACE
