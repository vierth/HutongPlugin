#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongPresets.h"
#include "Generation/BaySide.h"
#include "Generation/SiheyuanGenerator.h"
#include "SiheyuanTool.generated.h"

UCLASS()
class UHutongSiheyuanToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Siheyuan", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the house."))
	FHutongSiheyuanParams Params;



	// The generators silently clamp contradictory values (a sill above the window top, bays with min > max), which leaves the panel showing numbers the mesh does not use.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};

UCLASS()
class UHutongSiheyuanTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual FText GetStagePromptText() const override;
	virtual TArray<FText> GetStageNames() const override;
	virtual void AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse) override;
	virtual void AdjustHeight(double DeltaCm) override;
	virtual double GetPreviewHeight() const override;
	virtual void CancelPlacement() override;

protected:
	// Snaps each dragged extent to the preset's suggested footprint when it comes close.
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const override;

	virtual void RegisterToolSettings() override;
	virtual void BuildMeshForRect(double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Level) override;
	virtual void AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY) override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_Siheyuan"); }
	virtual FString GetPlacementDetail() const override;
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual FText GetKeyHintText() const override;

	// Veranda depth after the footprint-dependent clamp — see the note on the definition.
	double GetEffectiveVerandaDepth() const;

	// The settings' params with the footprint filled in from the drag — see the note on the definition.
	FHutongSiheyuanParams GetResolvedParams() const;

	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual bool OnRectCommitted(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;

	int32 ComputeBayCountForSide(HutongGen::EBaySide Side) const;

	UPROPERTY()
	TObjectPtr<UHutongSiheyuanToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> Presets;

	HutongGen::EBaySide BaySide = HutongGen::EBaySide::MinusY;

	// Bay count forced by the bracket keys; 0 means derive it from Min/Max Bay Width.
	int32 BayCountOverride = 0;

	// Ceiling for the manual count.
	static constexpr int32 MaxBayCount = 32;
};

UCLASS()
class UHutongSiheyuanToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
