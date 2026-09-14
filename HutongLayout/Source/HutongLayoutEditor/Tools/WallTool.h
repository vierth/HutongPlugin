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

// A set class per tool: RestoreProperties caches on the CDO, so one class shared by two tools is
// one saved set of settings shared by both, and the lane wall would come back carrying whatever
// the courtyard wall was last set to.
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

// Everything both walls do. The role is the subclass's, and it is the only thing the panel does
// not offer: a run drawn with the lane tool is a 院牆 and one drawn with the court tool is a 隔牆.
//
// A wall is drawn as a run of segments: the first click anchors it, every click after ends one
// segment and starts the next at any angle, and a click on the last end again finishes. Each
// segment is its own actor; where two meet, both ends are cut on the bisector so they meld, and
// an outer end that rests on a neighbour's face is cut flush against it. The geometry is
// HutongWallChain's, read once for the preview and once for the spawn.
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

	// Half the wall's thickness: the drag draws the run's centre line, and the face the lane is measured to sits that far off it.
	virtual double GetLaneFaceOffset() const override;

	// [ and ] slide whichever opening the run carries along it.
	virtual void AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse) override;
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual FText GetKeyHintText() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	// What this tool draws. The role is forced onto the settings, including after a preset load,
	// so a preset saved from the other tool arrives as this kind of wall.
	virtual EHutongWallRole GetWallRole() const PURE_VIRTUAL(UHutongWallTool::GetWallRole, return EHutongWallRole::Perimeter;);

protected:
	virtual void RegisterToolSettings() override;
	// The set class is the subclass's, so its saved settings are its own.
	virtual UHutongWallToolProperties* NewWallSettings() PURE_VIRTUAL(UHutongWallTool::NewWallSettings, return nullptr;);

	// The opening the keys move and the preview draws: the 牆垣式門 if the run has one, otherwise
	// the garden doorway. Returns false when the run carries neither.
	bool GetPreviewOpening(const FHutongWallParams& P, double Run, double& OutCentreAlong, double& OutWidth, double& OutHead,
		bool& bOutIsGate) const;

	// The run being drawn: every clicked point, and for each the bearing of what it snapped to.
	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual bool OnRectCommitted(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;

	// Whether the raw cursor is over the last end placed, within the snap radius: the closing click.
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
	// Whether the cursor's segment decided the side last frame, for the wider tolerance that keeps it.
	mutable bool bCursorSideOn = false;
	// Whether the segment being drawn sat on a frame axis last frame, for the wider tolerance that keeps it there.
	bool bAxisSnapOn = false;
	// The segments as they stand, with the cursor's unfinished one when asked.
	bool BuildChain(bool bWithCursor, TArray<HutongWallChain::FSegment>& OutSegments) const;
	// The bearing of the current segment, for the lane readout and the angle snap.
	double CurrentSegmentYawDeg() const;
	// The parameters segment i is built with: the tool's, with the run's opening on the legs
	// G marked, or on the longest leg when none was, and a plain 牆垣式門 where G asked for a
	// gate on a run whose settings carry no opening. bCursorLeg reads G live for the leg being drawn.
	FHutongWallParams SegmentParams(int32 Index, int32 NumSegments, bool bCursorLeg) const;

	UPROPERTY()
	TObjectPtr<UHutongWallToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> Presets;
};

// 院牆 — the boundary wall onto the lane. Blank by rule, and the only wall that is measured
// against the street it forms: it keeps the lane readout and the snap onto a canonical width.
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

// 隔牆 — the dividing wall inside the compound: lower, thinner, and the run that carries the
// openings. Nothing here forms a street, so the lane width neither snaps nor reads out.
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
