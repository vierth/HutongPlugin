#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongPresets.h"
#include "Generation/ArrowTowerGenerator.h"
#include "ArrowTowerTool.generated.h"

UCLASS()
class UHutongArrowTowerToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Arrow Tower", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the arrow tower (箭樓)."))
	FHutongArrowTowerParams Params;
};

// Places a 箭樓: click the middle of the platform's front, move toward the inside of the city, click again.
UCLASS()
class UHutongArrowTowerTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual void AdjustHeight(double DeltaCm) override;
	virtual double GetPreviewHeight() const override;
	virtual TArray<FText> GetToolHelpLines() const override;

protected:
	virtual void RegisterToolSettings() override;
	virtual void BuildMeshForRect(double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) override;
	virtual void AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY) override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_ArrowTower"); }
	virtual FString GetPlacementDetail() const override;
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const override;

	// The front faces away from the cursor.
	EHutongBaySide FacadeFromCursor() const;

	UPROPERTY()
	TObjectPtr<UHutongArrowTowerToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> Presets;
};

UCLASS()
class UHutongArrowTowerToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
