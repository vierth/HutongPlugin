#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongPresets.h"
#include "Generation/WallGenerator.h"
#include "Generation/HutongUrban.h"
#include "Tools/HutongWallChain.h"
#include "WallTool.generated.h"

UCLASS()
class UHutongWallToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Wall", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the wall run."))
	FHutongWallParams Params;
};

// One set class per tool: RestoreProperties caches on the CDO, so a shared class would share
// saved settings between the lane and court walls.
UCLASS()
class UHutongLaneWallToolProperties : public UHutongWallToolProperties
{
	GENERATED_BODY()
public:
	UHutongLaneWallToolProperties();
};

UCLASS()
class UHutongCourtWallToolProperties : public UHutongWallToolProperties
{
	GENERATED_BODY()
public:
	UHutongCourtWallToolProperties();
};

// Everything both walls do; the subclass fixes the role (lane tool 院牆, court tool 隔牆).
//
// Drawn as a run of segments: the first click anchors, each click ends a segment and starts the
// next at any angle, a click on the last end finishes. One actor per segment; joins are cut on
// the bisector, an outer end resting on a neighbour's face is cut flush. Geometry from
// HutongWallChain, for preview and spawn.
UCLASS(Abstract)
class UHutongWallTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual void AdjustHeight(double DeltaCm) override;
	virtual double GetPreviewHeight() const override;
	virtual void CancelPlacement() override;
	virtual FText GetStagePromptText() const override;
	virtual TArray<FText> GetStageNames() const override;

	// Half the thickness: the drag draws the centre line; the lane is measured to the face.
	virtual double GetLaneFaceOffset() const override;

	// [ and ] slide whichever opening the run carries along it.
	virtual void AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse) override;
	virtual TArray<FText> GetToolHelpLines() const override;
	// Segments follow the cursor; the wall has no R key.
	virtual bool HasRotateKey() const override { return false; }
	virtual FText GetKeyHintText() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	// The role this tool draws, forced onto the settings (also after a preset load) so the other
	// tool's presets arrive as this wall.
	virtual EHutongWallRole GetWallRole() const PURE_VIRTUAL(UHutongWallTool::GetWallRole, return EHutongWallRole::Perimeter;);

protected:
	virtual void RegisterToolSettings() override;
	// Per-subclass set class, so its saved settings are its own.
	virtual UHutongWallToolProperties* NewWallSettings() PURE_VIRTUAL(UHutongWallTool::NewWallSettings, return nullptr;);

	// The opening the keys move and the preview draws: the 牆垣式門, else the garden doorway;
	// false if neither.
	bool GetPreviewOpening(const FHutongWallParams& P, double Run, double& OutCentreAlong, double& OutWidth, double& OutHead,
		bool& bOutIsGate) const;

	// The run: every clicked point, each with the bearing it snapped to.
	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual bool OnRectCommitted(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;

	// Whether the raw cursor is within snap radius of the last end: the closing click.
	bool IsOnRunEnd(const FVector& RawCursor) const;
	virtual void SpawnFinalActor() override;
	// The segment under the cursor, for the readout: its length and the wall's thickness.
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const override;
	virtual FString GetPlacementDetail() const override;

	TArray<FVector> ChainPoints;
	TArray<double> ChainSnapYawDeg;
	TArray<double> ChainSnapYaw2Deg;
	TArray<FVector2D> ChainSnapInward;
	// One per segment: whether G was held on the click that closed it.
	TArray<bool> ChainGateFlags;
	TWeakObjectPtr<class UHutongBuildingComponent> ChainEndBuilding;

	// Where the drawn line sits across the wall: its centre, or the face the anchor snapped onto.
	double GetDrawnY() const;
	// Whether the cursor's segment decided the side last frame (wider tolerance keeps it).
	mutable bool bCursorSideOn = false;
	// Whether the drawn segment sat on a frame axis last frame (wider tolerance keeps it).
	bool bAxisSnapOn = false;
	// The segments as they stand, with the cursor's unfinished one when asked.
	bool BuildChain(bool bWithCursor, TArray<HutongWallChain::FSegment>& OutSegments) const;
	// The bearing of the current segment, for the lane readout and the angle snap.
	double CurrentSegmentYawDeg() const;
	// Params for segment i: the tool's, with the run's opening on G-marked legs (else the longest),
	// or a plain 牆垣式門 where G asked for a gate the settings lack. bCursorLeg reads G live.
	FHutongWallParams SegmentParams(int32 Index, int32 NumSegments, bool bCursorLeg) const;

	UPROPERTY()
	TObjectPtr<UHutongWallToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> Presets;
};

// 院牆, the boundary wall onto the lane. Blank by rule; the only wall measured against its
// street (lane readout, canonical-width snap).
UCLASS()
class UHutongLaneWallTool : public UHutongWallTool
{
	GENERATED_BODY()
public:
	virtual EHutongWallRole GetWallRole() const override { return EHutongWallRole::Perimeter; }
	virtual TArray<FText> GetToolHelpLines() const override;

protected:
	virtual UHutongWallToolProperties* NewWallSettings() override
	{
		return NewObject<UHutongLaneWallToolProperties>(this);
	}
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_LaneWall"); }
};

// 隔牆, the dividing wall inside the compound: lower, thinner, carries the openings. Forms no
// street, so no lane snap or readout.
UCLASS()
class UHutongCourtWallTool : public UHutongWallTool
{
	GENERATED_BODY()
public:
	virtual EHutongWallRole GetWallRole() const override { return EHutongWallRole::Courtyard; }
	virtual bool WantsLaneWidthSnap() const override { return false; }
	virtual TArray<FText> GetToolHelpLines() const override;

protected:
	virtual UHutongWallToolProperties* NewWallSettings() override
	{
		return NewObject<UHutongCourtWallToolProperties>(this);
	}
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_CourtWall"); }
};

UCLASS()
class UHutongLaneWallToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class UHutongCourtWallToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
