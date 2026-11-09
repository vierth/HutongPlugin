#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Generation/GateComplexLayout.h"
#include "GateComplexTool.generated.h"

UCLASS()
class UHutongGateComplexToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Gate Complex", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the gate complex."))
	FHutongGateComplexParams Params;

	UPROPERTY(EditAnywhere, Category="Gate Complex", meta=(HutongBasic, DisplayName="Outside Faces", ToolTip="Which way the gate faces out of the city."))
	EHutongCompassSide OutsideFaces = EHutongCompassSide::North;

	UPROPERTY(EditAnywhere, Category="Gate Complex", meta=(DisplayName="North Direction (yaw)", UIMin="-180", UIMax="180", ClampMin="-360", ClampMax="360", Units="deg", ToolTip="World yaw that faces north, in degrees; 0 is +X, 180 is -X."))
	double NorthYawDeg = 180.0;
};

// Stamps 德勝門 at once: click the middle of the gate's outer face on the city wall, move toward the inside of the
// city, click again. Every piece is an ordinary building; the whole is one undo step.
UCLASS()
class UHutongGateComplexTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual FText GetStagePromptText() const override;
	virtual bool HasRotateKey() const override { return false; }
	virtual bool CanFinishPlacement() const override { return true; }

protected:
	virtual void RegisterToolSettings() override;
	virtual void SpawnFinalActor() override;
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_GateComplex"); }
	virtual FString GetPlacementDetail() const override;
	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual void RenderIdlePreview(FPrimitiveDrawInterface* PDI, const FVector& CursorGround) override;

	// The complex's frame: outside the city toward -Y.
	void ApplyCompass();
	// The params with the side gate on the hand east lies on in this facing.
	FHutongGateComplexParams FacedParams() const;
	void DrawPieces(FPrimitiveDrawInterface* PDI, const FVector& Origin) const;

	UPROPERTY()
	TObjectPtr<UHutongGateComplexToolProperties> Settings;
};

UCLASS()
class UHutongGateComplexToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
