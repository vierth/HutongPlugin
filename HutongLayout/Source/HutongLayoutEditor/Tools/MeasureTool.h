#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Generation/HutongUrban.h"
#include "MeasureTool.generated.h"

UCLASS()
class UHutongMeasureToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	// --- Reading ---

	UPROPERTY(VisibleAnywhere, Category="Measurement", meta=(DisplayName="Distance", Units="cm", ToolTip="Measured distance between the two clicked points, in cm."))
	double DistanceCm = 0.0;

	UPROPERTY(VisibleAnywhere, Category="Measurement", meta=(DisplayName="In Yuan Paces (步)", ToolTip="The measured distance in Yuan paces (步)."))
	double DistanceBu = 0.0;

	UPROPERTY(VisibleAnywhere, Category="Measurement", meta=(DisplayName="Read As Street Width", ToolTip="The measured distance classed as a street width."))
	FString StreetReading;

	// --- Calibration ---

	UPROPERTY(EditAnywhere, Category="Calibration", meta=(DisplayName="Known Real Distance", UIMin="0", ClampMin="0", Units="cm", ToolTip="True length of the measured span, in cm, for calibration."))
	double KnownRealDistanceCm = 0.0;

	UPROPERTY(VisibleAnywhere, Category="Calibration", meta=(DisplayName="Scale Correction", ToolTip="Known real distance divided by the measured distance; one means true scale."))
	double ScaleCorrection = 0.0;

	// Multiplies the selected actor's scale by the correction, keeping the first clicked point fixed so the calibration does not slide the map out from under the measurement that produced it.
	UFUNCTION(CallInEditor, Category="Calibration", meta=(DisplayName="Apply Scale To Selected", ToolTip="Multiplies the selected actor's scale by the correction, keeping the first clicked point fixed."))
	void ApplyScaleToSelected();

	// Set by the tool so the button knows what it is scaling about.
	FVector AnchorWorld = FVector::ZeroVector;
};

// Measures, and calibrates a map image against what it measures.
UCLASS()
class UHutongMeasureTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual FText GetStagePromptText() const override;
	virtual TArray<FText> GetStageNames() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

protected:
	virtual void RegisterToolSettings() override;
	virtual FString GetPlacementDetail() const override;
	virtual double GetPreviewHeight() const override { return 0.0; }

	// A measurement is not a placement.
	virtual void SpawnFinalActor() override {}
	virtual bool OnRectCommitted(const FVector& HitWorld) override;
	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;
	virtual void BuildMeshForRect(double, double, UE::Geometry::FDynamicMesh3&, EHutongDetail) override {}

	void RefreshReading();

	UPROPERTY()
	TObjectPtr<UHutongMeasureToolProperties> Settings;
};

UCLASS()
class UHutongMeasureToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
