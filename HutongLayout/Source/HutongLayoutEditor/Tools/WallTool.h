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

UCLASS()
class UHutongImperialWallToolProperties : public UHutongWallToolProperties
{
	GENERATED_BODY()
public:
	UHutongImperialWallToolProperties();
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
	virtual bool CanFinishPlacement() const override;
	virtual FText GetStagePromptText() const override;
	virtual TArray<FText> GetStageNames() const override;

	// Half the thickness: the drag draws the centre line; the lane is measured to the face.
	virtual double GetLaneFaceOffset() const override;

	virtual void AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse) override;
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual bool HasRotateKey() const override { return false; }
	virtual bool StartsOnFootprintEdges() const override { return true; }
	virtual FText GetKeyHintText() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	// Forced onto the settings, also after a preset load, so the other tool's presets arrive as this wall.
	virtual EHutongWallRole GetWallRole() const PURE_VIRTUAL(UHutongWallTool::GetWallRole, return EHutongWallRole::Perimeter;);

protected:
	virtual void RegisterToolSettings() override;
	virtual UHutongWallToolProperties* NewWallSettings() PURE_VIRTUAL(UHutongWallTool::NewWallSettings, return nullptr;);

	// X centre along the run, Y width, Z head.
	TArray<FVector> GetPreviewOpenings(const FHutongWallParams& P, double Run) const;

	static constexpr int32 MaxGatesPerLeg = 4;
	int32 CursorGateCount = 0;
	virtual void ToggleOpeningMark() override;

	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual bool OnRectCommitted(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;

	bool IsOnRunEnd(const FVector& RawCursor) const;
	virtual void SpawnFinalActor() override;
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const override;
	virtual FString GetPlacementDetail() const override;

	TArray<FVector> ChainPoints;
	TArray<double> ChainSnapYawDeg;
	TArray<double> ChainSnapYaw2Deg;
	TArray<FVector2D> ChainSnapInward;
	TArray<int32> ChainGateCounts;
	TWeakObjectPtr<class UHutongBuildingComponent> ChainEndBuilding;

	double GetDrawnY() const;
	void EndsOnNeighbours(bool bWithCursor, double& OutDrawnY, HutongWallChain::FEndFace& OutStart, HutongWallChain::FEndFace& OutEnd) const;
	virtual bool PointSnapsOnly() const override { return true; }
	bool BuildChain(bool bWithCursor, TArray<HutongWallChain::FSegment>& OutSegments) const;
	double CurrentSegmentYawDeg() const;
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

// 皇城牆, the wall round the imperial city: red plaster, battered, under a glazed cap.
UCLASS()
class UHutongImperialWallTool : public UHutongWallTool
{
	GENERATED_BODY()
public:
	virtual EHutongWallRole GetWallRole() const override { return EHutongWallRole::ImperialCity; }
	virtual TArray<FText> GetToolHelpLines() const override;

protected:
	virtual UHutongWallToolProperties* NewWallSettings() override
	{
		return NewObject<UHutongImperialWallToolProperties>(this);
	}
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_ImperialWall"); }
};

UCLASS()
class UHutongImperialWallToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
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
