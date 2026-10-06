#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "UnknownTool.generated.h"

// Traces a footprint whose type is not known: an outline with metadata, no geometry.
UCLASS()
class UHutongUnknownTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual void Setup() override;
	virtual TArray<FText> GetToolHelpLines() const override;

protected:
	virtual bool BuildsGeometry() const override { return false; }
	virtual void AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY) override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_Unknown"); }
	virtual FString GetPlacementDetail() const override;
};

UCLASS()
class UHutongUnknownToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
