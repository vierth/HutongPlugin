#pragma once

#include "CoreMinimal.h"
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "PlaceRegionSelectTool.generated.h"

// Registers no input behaviours, so clicks reach the level editor: select, gizmo, Delete. Tool
// behaviours take ground clicks before any hit proxy. A tool, not "no tool", so the palette shows it.
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
