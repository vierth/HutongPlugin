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

	// Placement decision, not a gate parameter: a placed gate has no neighbour to match.
	UPROPERTY(EditAnywhere, Category="Gate House", meta=(DisplayName="Match Neighbouring Row", ToolTip="Takes the depth and eave of the row the first click snaps to."))
	bool bMatchNeighbouringRow = true;

	UPROPERTY(EditAnywhere, Category="Gate House", meta=(DisplayName="Ridge Above Row", EditCondition="bMatchNeighbouringRow", UIMin="0", UIMax="120", ClampMin="0", Units="cm", ToolTip="How far the gate's ridge stands above the matched row's ridge, in cm."))
	double RowRidgeClearance = HutongCanon::Gate::RidgeAboveRowCm;

#if WITH_EDITOR
	// Clamps the eave into the style's band when style or height changes.
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

	// Holds the drag in the style's size band, or to the matched row.
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const override;

	// Matched row's depth along this gate's depth axis; zero if the anchor joined no row.
	double MatchedRowDepth() const;
	// Matched row's eave; zero likewise.
	double MatchedRowEave() const;
	// Eave for the match: tool params with the ridge lifted clear of the row's.
	double MatchedGateEave() const;
	// Aligns the placement to the row's run and facing, anchor on the row's nearer front corner so fronts line up.
	void TakeRowBearing();

	// Body direction from the anchor on the depth axis (gate frame), pinned at the click: +/-1, zero if
	// unmatched. Not re-derived from BaySide, which commit may flip to the camera side.
	FVector2D RowInwardLocal = FVector2D::ZeroVector;

	// Extra stage, as in the siheyuan.
	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual bool OnRectCommitted(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;

	// Side on that axis the camera is on.
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
