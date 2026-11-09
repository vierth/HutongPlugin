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

UCLASS()
class UHutongPalaceWallToolProperties : public UHutongCityWallToolProperties
{
	GENERATED_BODY()
public:
	UHutongPalaceWallToolProperties();
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
	virtual void ToggleOpeningMark() override;
	virtual void AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse) override;
	virtual void CancelPlacement() override;
	virtual bool CanFinishPlacement() const override;
	virtual FText GetStagePromptText() const override;
	virtual TArray<FText> GetStageNames() const override;
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual FText GetKeyHintText() const override;
	virtual bool HasRotateKey() const override { return false; }
	virtual bool WantsLaneWidthSnap() const override { return false; }
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

protected:
	virtual void RegisterToolSettings() override;
	virtual UHutongCityWallToolProperties* NewCitySettings() { return NewObject<UHutongCityWallToolProperties>(this); }
	// Set: the tool lays only this city's wall, whatever a preset says.
	virtual TOptional<EHutongCityWallRank> ForcedRank() const { return {}; }
	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual bool OnRectCommitted(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;
	virtual void SpawnFinalActor() override;
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_CityWall"); }
	virtual FString GetPlacementDetail() const override;
	virtual bool PointSnapsOnly() const override { return true; }

	bool IsNear(const FVector& Cursor, const FVector& Point) const;
	bool BuildChain(bool bWithCursor, TArray<HutongWallChain::FSegment>& OutSegments) const;
	EHutongBaySide OuterSide() const;

	FHutongCityWallParams SegmentParams(int32 Index, const TArray<HutongWallChain::FSegment>& Segments) const;

	// A placed leg's end with no mitre yet, where a new run can carry it on.
	// Or a corner's port, which the run abuts square: Out is the way a leg leaves the corner there.
	struct FLegEnd
	{
		TWeakObjectPtr<class UHutongCityWallBuildingComponent> Leg;
		bool bAtStart = false;
		FVector Point = FVector::ZeroVector;
		FVector2D Far = FVector2D::ZeroVector;
		bool bPort = false;
		FVector2D Out = FVector2D::ZeroVector;
		FVector2D OuterCorner = FVector2D::ZeroVector;
	};
	bool IsSameEnd(const FLegEnd& A, const FLegEnd& B) const;
	// The battlements to the side of the corner's outside, the run leaving along Dir from Point.
	void TakeOuterSideFromPort(const FLegEnd& Port, const FVector2D& Dir);
	bool FindFreeEnd(const FVector& Near, FLegEnd& Out) const;
	// The chain with the joined legs' far ends added, so its own ends mitre against them.
	bool BuildJoinedChain(bool bWithCursor, TArray<HutongWallChain::FSegment>& OutSegments,
		HutongWallChain::FSegment* OutStartLeg, HutongWallChain::FSegment* OutEndLeg) const;
	void MitreJoinedLeg(const FLegEnd& End, const HutongWallChain::FSegment& Rebuilt) const;

	TOptional<FLegEnd> StartJoin;
	TOptional<FLegEnd> EndJoin;

	TArray<FVector> ChainPoints;
	bool bChainClosed = false;
	// Per leg drawn, and the leg under the cursor: 0 none, 1 rising toward the leg's end, 2 toward its start.
	TArray<int32> ChainRamps;
	int32 CursorRamp = 0;
	// A nested edit may report the struct as the changed property.
	EHutongCityWallRank LastRank = EHutongCityWallRank::Inner;

	UPROPERTY()
	TObjectPtr<UHutongCityWallToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> Presets;
};

// 紫禁城城牆: the city wall at the palace's figures, red on the side facing the palace.
UCLASS()
class UHutongPalaceWallTool : public UHutongCityWallTool
{
	GENERATED_BODY()
public:
	virtual TArray<FText> GetToolHelpLines() const override;

protected:
	virtual UHutongCityWallToolProperties* NewCitySettings() override { return NewObject<UHutongPalaceWallToolProperties>(this); }
	virtual TOptional<EHutongCityWallRank> ForcedRank() const override { return EHutongCityWallRank::Palace; }
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_PalaceWall"); }
};

UCLASS()
class UHutongPalaceWallToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class UHutongCityWallToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
