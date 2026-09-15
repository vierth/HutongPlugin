#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongPresets.h"
#include "Generation/BaySide.h"
#include "Generation/GateHouseGenerator.h"
#include "GateHouseTool.generated.h"

UCLASS()
class UHutongGateHouseToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Gate House", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the main gate (大門) house."))
	FHutongGateHouseParams Params;

	// A placement decision rather than a parameter of the gate, so it lives beside the params and not in them: a placed gate has no neighbour to match.
	UPROPERTY(EditAnywhere, Category="Gate House", meta=(DisplayName="Match Neighbouring Row", ToolTip="Takes the depth and eave of the row the first click snaps to."))
	bool bMatchNeighbouringRow = true;

	UPROPERTY(EditAnywhere, Category="Gate House", meta=(DisplayName="Ridge Above Row", EditCondition="bMatchNeighbouringRow", UIMin="0", UIMax="120", ClampMin="0", Units="cm", ToolTip="How far the gate's ridge stands above the matched row's ridge, in cm."))
	double RowRidgeClearance = HutongCanon::Gate::RidgeAboveRowCm;

#if WITH_EDITOR
	// Pull the eave into the style's band when the style or the height changes.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

// Places one of the four ordinary courtyard gate types.
UCLASS()
class UHutongGateHouseTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual void AdjustHeight(double DeltaCm) override;
	virtual double GetPreviewHeight() const override;
	virtual FText GetStagePromptText() const override;
	virtual TArray<FText> GetStageNames() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual TArray<FText> GetToolHelpLines() const override;

protected:
	virtual void RegisterToolSettings() override;
	virtual void BuildMeshForRect(double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Level) override;
	virtual void AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY) override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_Gate"); }
	virtual FString GetPlacementDetail() const override;

	// Holds the drag inside the style's size band, or to the row it is set into.
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const override;

	// The depth of the row the anchor snapped to, measured along this gate's own depth axis, or zero when the anchor joined nothing that is a row.
	double MatchedRowDepth() const;
	// The row's eave, or zero for the same reasons.
	double MatchedRowEave() const;
	// The eave the match wants: the tool's params with the ridge lifted clear of the row's.
	double MatchedGateEave() const;
	// Turns the placement onto the row's run, faces it the row's way, and moves the anchor onto
	// the row's nearer front corner so the two fronts are one line.
	void TakeRowBearing();

	// Which way the body extends from the anchor along the depth axis, in the gate's frame, pinned
	// at the click: +1 or -1 on the depth axis, zero when no row was matched. Not re-derived from
	// BaySide, which the commit may flip to the camera's side.
	FVector2D RowInwardLocal = FVector2D::ZeroVector;

	// Same extra stage as the siheyuan.
	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual bool OnRectCommitted(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;

	// Whichever of the two sides on that axis the camera is on.
	HutongGen::EBaySide ComputeCameraFacingOnAxis() const;
	HutongGen::EBaySide ComputeClosestSide(double Hx, double Hy) const;

	UPROPERTY()
	TObjectPtr<UHutongGateHouseToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> Presets;

	HutongGen::EBaySide BaySide = HutongGen::EBaySide::MinusY;
};

UCLASS()
class UHutongGateHouseToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
