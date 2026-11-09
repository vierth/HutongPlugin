#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongPresets.h"
#include "Generation/SmallBuildingGenerator.h"
#include "SmallBuildingTool.generated.h"

UCLASS()
class UHutongSmallBuildingToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Small Building", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the small building (小房)."))
	FHutongSmallBuildingParams Params;
};

// Places a 小房: guard post, shed, lone room or shrine. A third click picks the facade side.
UCLASS()
class UHutongSmallBuildingTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual void AdjustHeight(double DeltaCm) override;
	virtual double GetPreviewHeight() const override;
	virtual void AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse) override;
	virtual void CancelPlacement() override;
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual FText GetStagePromptText() const override;
	virtual TArray<FText> GetStageNames() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

protected:
	virtual void RegisterToolSettings() override;
	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual bool OnRectCommitted(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;
	virtual void BuildMeshForRect(double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Level) override;
	virtual void AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY) override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_SmallBuilding"); }
	virtual FString GetPlacementDetail() const override;

	FHutongSmallBuildingParams SizedParams(double SizeX, double SizeY) const;

	UPROPERTY()
	TObjectPtr<UHutongSmallBuildingToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> Presets;

	EHutongBaySide BaySide = EHutongBaySide::MinusY;
	// Zero: from the frontage.
	int32 BayCountOverride = 0;
};

UCLASS()
class UHutongSmallBuildingToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
