#pragma once

#include "CoreMinimal.h"
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "PlaceRegionSelectTool.generated.h"

// **The tool that takes no clicks, which is the whole of what it is for.** A tool's input
// behaviours take every click on the ground before any hit proxy sees one, so with the pen up
// there is no way to click a region and have it selected — the click draws a corner instead. This
// registers no behaviours at all, so clicks fall through to the level editor: a region is picked by
// clicking it, moved with the ordinary gizmo, deleted with Delete, and the mode panel's form
// follows the selection as it always does.
//
// It is a tool rather than a "no tool" state because the palette is where you look: three buttons,
// one of them pressed, and the pressed one says what a click will do.
UCLASS()
class UPlaceRegionSelectTool : public UInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;

	FText GetStagePromptText() const;
};

UCLASS()
class UPlaceRegionSelectToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
