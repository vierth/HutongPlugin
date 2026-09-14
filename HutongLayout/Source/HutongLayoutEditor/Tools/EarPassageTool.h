#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongPresets.h"
#include "Generation/BaySide.h"
#include "Generation/EarPassageGenerator.h"
#include "EarPassageTool.generated.h"

UCLASS()
class UHutongEarPassageToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Ear Room With Passage", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the ear room and the covered passage beside it."))
	FHutongEarPassageParams Params;

	// The room's contradictory values are clamped the way the house tool clamps them.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};

// 耳房 with a 過道 beside it, placed like a house: footprint, then the facade side.
UCLASS()
class UHutongEarPassageTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual FText GetStagePromptText() const override;
	virtual TArray<FText> GetStageNames() const override;
	virtual void AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse) override;
	virtual void AdjustHeight(double DeltaCm) override;
	virtual double GetPreviewHeight() const override;

protected:
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const override;
	virtual void RegisterToolSettings() override;
	virtual void BuildMeshForRect(double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Level) override;
	virtual void AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY) override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_EarPassage"); }
	virtual FString GetPlacementDetail() const override;
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual FText GetKeyHintText() const override;

	// The settings' params with the footprint filled in from the drag.
	FHutongEarPassageParams GetResolvedParams() const;

	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual bool OnRectCommitted(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;

	UPROPERTY()
	TObjectPtr<UHutongEarPassageToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> Presets;

	HutongGen::EBaySide BaySide = HutongGen::EBaySide::MinusY;
};

UCLASS()
class UHutongEarPassageToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
