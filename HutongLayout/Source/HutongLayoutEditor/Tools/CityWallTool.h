#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongPresets.h"
#include "Tools/HutongWallChain.h"
#include "Generation/CityWallGenerator.h"
#include "CityWallTool.generated.h"

UCLASS()
class UHutongCityWallToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="City Wall", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the city wall (城牆)."))
	FHutongCityWallParams Params;

	UPROPERTY(EditAnywhere, Category="City Wall", meta=(DisplayName="Battlements on the Other Side", ToolTip="Puts the crenellated outer parapet on the other side of the run; the preview marks it in orange. F flips it while placing."))
	bool bFlipOuterSide = false;
};

// Lays a 城牆 as a run of straight legs: the first click anchors, each click ends a leg, a click on the
// last end finishes, a click on the first point closes the circuit. One actor per leg, joins mitred
// (HutongWallChain), drawn on the centre line.
UCLASS()
class UHutongCityWallTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual void AdjustHeight(double DeltaCm) override;
	virtual double GetPreviewHeight() const override;
	virtual void FlipFacing() override;
	// G: a ramp on the leg being drawn, rising forward; again, turned round; again, none.
	virtual void ToggleOpeningMark() override;
	// [ and ]: the ramp's top along its leg.
	virtual void AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse) override;
	virtual void CancelPlacement() override;
	virtual FText GetStagePromptText() const override;
	virtual TArray<FText> GetStageNames() const override;
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual FText GetKeyHintText() const override;
	virtual bool HasRotateKey() const override { return false; }
	virtual bool WantsLaneWidthSnap() const override { return false; }
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	// A City chosen in the panel takes its figures.
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

protected:
	virtual void RegisterToolSettings() override;
	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual bool OnRectCommitted(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;
	virtual void SpawnFinalActor() override;
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_CityWall"); }
	virtual FString GetPlacementDetail() const override;
	virtual bool PointSnapsOnly() const override { return true; }

	bool IsNear(const FVector& Cursor, const FVector& Point) const;
	// The legs as they stand, with the cursor's unfinished one when asked; a closed run mitres all round.
	bool BuildChain(bool bWithCursor, TArray<HutongWallChain::FSegment>& OutSegments) const;
	EHutongBaySide OuterSide() const;

	// The params leg i is built with: its ramp from G, else the panel's on the longest leg.
	FHutongCityWallParams SegmentParams(int32 Index, const TArray<HutongWallChain::FSegment>& Segments) const;

	TArray<FVector> ChainPoints;
	bool bChainClosed = false;
	// Per leg drawn, and the leg under the cursor: 0 none, 1 rising toward the leg's end, 2 toward its start.
	TArray<int32> ChainRamps;
	int32 CursorRamp = 0;
	// The City last seen in the panel: a nested edit may report the struct, not the field.
	EHutongCityWallRank LastRank = EHutongCityWallRank::Inner;

	UPROPERTY()
	TObjectPtr<UHutongCityWallToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> Presets;
};

UCLASS()
class UHutongCityWallToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
