#pragma once

#include "CoreMinimal.h"
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "PlaceLabelTypes.h"
#include "Tools/PlaceRegionEditCore.h"
#include "PlaceRegionPenTool.generated.h"

class UClickDragInputBehavior;
class UMouseHoverBehavior;
class UPlaceRegionComponent;
class FPrimitiveDrawInterface;
class FCanvas;

// What the pending region gets when you confirm it, plus how the tool snaps and previews.
UCLASS()
class UPlaceRegionPenToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	// District, hutong, compound...
	UPROPERTY(EditAnywhere, Category = "New Region")
	TObjectPtr<UPlaceLabelTypeAsset> Type;

	// 大柵欄, not 大柵欄胡同 — the type supplies the 胡同.
	UPROPERTY(EditAnywhere, Category = "New Region")
	FPlaceName Name;

	// Creates the region you just outlined. Enter does the same thing.
	UFUNCTION(CallInEditor, Category = "New Region", meta = (DisplayName = "Create Region"))
	void CreateRegion();

	// Throws away the outline without placing anything. Escape does the same thing.
	UFUNCTION(CallInEditor, Category = "New Region", meta = (DisplayName = "Discard Outline"))
	void DiscardOutline();

	// Where the name came from. Stamped on each new region and kept for the next one.
	UPROPERTY(EditAnywhere, Category = "New Region", meta = (MultiLine = true))
	FText Source;

	UPROPERTY(EditAnywhere, Category = "New Region")
	EPlaceConfidence Confidence = EPlaceConfidence::Attested;

	UPROPERTY(EditAnywhere, Category = "New Region")
	bool bAutoParent = true;

	// World Outliner folder the new region actor is filed under.
	UPROPERTY(EditAnywhere, Category = "New Region")
	FName OutlinerFolder = TEXT("RegionLabels");

	TWeakObjectPtr<class UPlaceRegionPenTool> OwningTool;

	// Turn on for interiors and single floors, where the sky above should not count as inside.
	UPROPERTY(EditAnywhere, Category = "New Region")
	bool bUseHeightBounds = false;

	UPROPERTY(EditAnywhere, Category = "New Region",
		meta = (EditCondition = "bUseHeightBounds", ClampMin = "0.0", Units = "cm"))
	double HeightAboveGround = 500.0;

	UPROPERTY(EditAnywhere, Category = "New Region",
		meta = (EditCondition = "bUseHeightBounds", ClampMin = "0.0", Units = "cm"))
	double DepthBelowGround = 100.0;

	// Snap new points onto placed regions' corners so shared boundaries meet exactly.
	UPROPERTY(EditAnywhere, Category = "Snapping")
	bool bSnapToExistingVertices = true;

	// Also snap onto their edges.
	UPROPERTY(EditAnywhere, Category = "Snapping")
	bool bSnapToExistingEdges = true;

	UPROPERTY(EditAnywhere, Category = "Snapping", meta = (ClampMin = "0.0", Units = "cm"))
	double SnapRadius = 100.0;

	// Click a HutongLayout building to take its footprint as the outline. Off while plan outlines are hidden.
	UPROPERTY(EditAnywhere, Category = "Snapping", meta = (DisplayName = "Trace Building Footprints"))
	bool bTraceFootprints = true;

	// Outlines every region already in the level while the tool is active.
	UPROPERTY(EditAnywhere, Category = "Display")
	bool bShowExistingRegions = true;
};

// Live feedback on the polygon being drawn. Output only, never restored.
UCLASS()
class UPlaceRegionPenToolReadout : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Drawing")
	int32 PointCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Drawing", meta = (Units = "cm"))
	double Perimeter = 0.0;

	UPROPERTY(VisibleAnywhere, Category = "Drawing", DisplayName = "Area (m2)")
	double AreaSquareMetres = 0.0;

	// What the cursor is currently snapping to, if anything.
	UPROPERTY(VisibleAnywhere, Category = "Drawing")
	FString Detail;
};

// Click points to define a region polygon, Illustrator-pen style.
UCLASS()
class UPlaceRegionPenTool : public UInteractiveTool,
							public IClickDragBehaviorTarget,
							public IHoverBehaviorTarget
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;

	// Polls the level's regions: region deletes, imports and edits send the tool no notice.
	virtual void OnTick(float DeltaTime) override;

	// IClickDragBehaviorTarget.
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override;

	// Pure virtual on IModifierToggleBehaviorTarget, which both target interfaces inherit.
	virtual void OnUpdateModifierState(int ModifierID, bool bIsOn) override {}

	// IHoverBehaviorTarget
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;

	// Backspace.
	void UndoLastPoint();

	// Delete.
	void DeleteHoveredPoint();

	void CloseOutline();

	void ConfirmRegion();

	void CancelDrawing();

	bool IsDrawing() const { return PendingPoints.Num() > 0; }
	bool IsAwaitingConfirm() const { return bAwaitingConfirm; }

	bool IsActive() const { return PendingPoints.Num() > 0; }

	FText GetStagePromptText() const;
	FText GetDrawingSummaryText() const;

	UPlaceRegionPenToolProperties* GetPendingSettings() const { return Settings; }
	void NotifyPendingSettingsChanged()
	{
		if (Settings)
		{
			NotifyOfPropertyChangeByTool(Settings);
		}
	}

protected:
	void RegisterSettings(UInteractiveToolPropertySet* PropertySet, bool bPersist = true);

	UWorld* GetTargetWorld() const;

	bool TryHitGround(const FInputDeviceRay& Ray, FVector& OutHit) const;
	FInputRayHit GroundRayHit(const FInputDeviceRay& Ray) const;

	FVector ResolveCandidatePoint(const FVector& TracedHit, int32 IgnorePointIndex = INDEX_NONE);

	void RefreshCachedRegions();
	void UpdateReadout();

	double PickRadiusAt(const FVector& WorldPoint) const;
	double HandleSizeAt(const FVector& WorldPoint) const;

	enum class EHover : uint8
	{
		Ground,
		Point,
		Segment,
		CloseTarget,
		Footprint,
	};

	void UpdateHoverState(const FVector& TracedHit);
	void AdoptFootprint();

	UPROPERTY()
	TObjectPtr<UClickDragInputBehavior> ClickDragBehavior;

	UPROPERTY()
	TObjectPtr<UMouseHoverBehavior> HoverBehavior;

	UPROPERTY()
	TObjectPtr<UPlaceRegionPenToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UPlaceRegionPenToolReadout> Readout;

	UPROPERTY()
	TArray<TObjectPtr<UInteractiveToolPropertySet>> RegisteredSettings;

	// World space, each point carrying the Z it was traced at.
	TArray<FVector> PendingPoints;

	FVector HoverPoint = FVector::ZeroVector;
	bool bHoverValid = false;

	EHover HoverKind = EHover::Ground;
	int32 HoverPointIndex = INDEX_NONE;
	int32 HoverSegmentIndex = INDEX_NONE;

	TArray<FVector> HoveredFootprint;
	FString HoveredFootprintLabel;

	bool bAwaitingConfirm = false;

	bool bDraggingPoint = false;
	bool bDragMoved = false;
	int32 DraggedPointIndex = INDEX_NONE;
	FVector2D PressPixel = FVector2D::ZeroVector;

	TArray<TWeakObjectPtr<UPlaceRegionComponent>> CachedRegions;
	double TimeSinceCacheRefresh = 0.0;

	TWeakObjectPtr<AActor> LastPlacedActor;

	FVector CameraPosition = FVector::ZeroVector;
	double WorldPerPixelOrtho = 1.0;
	double WorldPerPixelPerUnitDistance = 0.001;
	bool bCameraIsOrthographic = false;
};

UCLASS()
class UPlaceRegionPenToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
