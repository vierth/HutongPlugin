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

	// Where the name came from and how far it is attested. Stamped onto each region as it is
	// placed and, unlike the name, deliberately *not* cleared afterwards: a lane is traced off one
	// sheet in one sitting, and the source is a fact about that sitting rather than about one
	// polygon. HutongLayout's metadata property set is the same decision.
	UPROPERTY(EditAnywhere, Category = "New Region", meta = (MultiLine = true))
	FText Source;

	UPROPERTY(EditAnywhere, Category = "New Region")
	EPlaceConfidence Confidence = EPlaceConfidence::Attested;

	UPROPERTY(EditAnywhere, Category = "New Region")
	bool bAutoParent = true;

	// World Outliner folder the new region actor is filed under.
	UPROPERTY(EditAnywhere, Category = "New Region")
	FName OutlinerFolder = TEXT("RegionLabels");

	// Set by the tool so the buttons above can reach it.
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

	// Snap new points onto the corners of regions already placed, so shared boundaries meet exactly rather than nearly.
	UPROPERTY(EditAnywhere, Category = "Snapping")
	bool bSnapToExistingVertices = true;

	// Snap onto the edges too, for a compound whose front runs along a hutong without matching any of its corners.
	UPROPERTY(EditAnywhere, Category = "Snapping")
	bool bSnapToExistingEdges = true;

	UPROPERTY(EditAnywhere, Category = "Snapping", meta = (ClampMin = "0.0", Units = "cm"))
	double SnapRadius = 100.0;

	// Click a placed HutongLayout building to take its footprint as the outline rather than tracing
	// four corners by eye. Nothing is offered while HutongLayout's plan outlines are hidden.
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

	// Regions can be deleted, imported or edited while this tool is up, and none of that notifies it.
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

	// Driven by the mode's global key handler and by the property set's buttons.

	// Backspace.
	void UndoLastPoint();

	// Delete. Removes whichever corner is under the cursor, at either stage.
	void DeleteHoveredPoint();

	// Closes the outline and moves to the confirm stage.
	void CloseOutline();

	// Spawns the region from the confirmed outline and clears the name and type for the next one.
	void ConfirmRegion();

	// Abandons whatever stage is in progress.
	void CancelDrawing();

	bool IsDrawing() const { return PendingPoints.Num() > 0; }
	bool IsAwaitingConfirm() const { return bAwaitingConfirm; }

	// True whenever there is an outline on screen, at either stage.
	bool IsActive() const { return PendingPoints.Num() > 0; }

	FText GetStagePromptText() const;
	FText GetDrawingSummaryText() const;

	// What the region about to be placed will be called. The mode panel's form edits these
	// directly while an outline is in hand, so there is one place to type a name rather than two.
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

	// Scene trace onto whatever the ray hits, falling back to a horizontal plane.
	bool TryHitGround(const FInputDeviceRay& Ray, FVector& OutHit) const;
	FInputRayHit GroundRayHit(const FInputDeviceRay& Ray) const;

	// Applies the Shift angle constraint and then the snap ladder.
	FVector ResolveCandidatePoint(const FVector& TracedHit, int32 IgnorePointIndex = INDEX_NONE);

	void RefreshCachedRegions();
	void UpdateReadout();

	// Screen-constant handle sizing, cached from the last Render.
	double PickRadiusAt(const FVector& WorldPoint) const;
	double HandleSizeAt(const FVector& WorldPoint) const;

	enum class EHover : uint8
	{
		// Free ground. A click appends a point while drawing, and does nothing once closed.
		Ground,
		// Over an existing corner: drag moves it, Alt+click removes it.
		Point,
		// Over a segment between two corners: a click inserts one there.
		Segment,
		// Over the first corner with three or more placed: a click closes the outline.
		CloseTarget,
		// Over a HutongLayout building, with nothing drawn yet: a click takes its footprint.
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

	// The building footprint under the cursor, previewed before it is adopted.
	TArray<FVector> HoveredFootprint;
	FString HoveredFootprintLabel;

	// Outline finished, waiting for a name and a confirmation.
	bool bAwaitingConfirm = false;

	// Drag
	bool bDraggingPoint = false;
	bool bDragMoved = false;
	int32 DraggedPointIndex = INDEX_NONE;
	FVector2D PressPixel = FVector2D::ZeroVector;

	// Snapshot of the level's regions, refreshed periodically and after each spawn rather than walked on every hover tick.
	TArray<TWeakObjectPtr<UPlaceRegionComponent>> CachedRegions;
	double TimeSinceCacheRefresh = 0.0;

	// What the last placement produced.
	TWeakObjectPtr<AActor> LastPlacedActor;

	// Cached from the last Render so hover picking can size handles the way they are drawn.
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
