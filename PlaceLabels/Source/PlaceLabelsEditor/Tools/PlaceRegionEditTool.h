#pragma once

#include "CoreMinimal.h"
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "PlaceLabelTypes.h"
#include "PlaceLabelsTopology.h"
#include "Tools/PlaceRegionEditCore.h"
#include "PlaceRegionEditTool.generated.h"

class UClickDragInputBehavior;
class UMouseHoverBehavior;
class UPlaceRegionComponent;
class FCanvas;
class FPrimitiveDrawInterface;

// How the edit tool snaps, welds and draws. Persisted between sessions.
UCLASS()
class UPlaceRegionEditToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	// Pull a dragged corner onto the corners of neighbouring regions.
	UPROPERTY(EditAnywhere, Category = "Snapping")
	bool bSnapToVertices = true;

	// Pull it onto their edges too, for a boundary that runs along a neighbour without matching any of its corners.
	UPROPERTY(EditAnywhere, Category = "Snapping")
	bool bSnapToEdges = true;

	UPROPERTY(EditAnywhere, Category = "Snapping", meta = (ClampMin = "0.0", Units = "cm"))
	double SnapRadius = 100.0;

	// How far apart two boundaries may be and still count as meant to be the same boundary, used both to report seams and to weld them.
	UPROPERTY(EditAnywhere, Category = "Boundaries", meta = (ClampMin = "0.1", Units = "cm"))
	double WeldTolerance = 25.0;

	// Amber ticks across every pair of boundaries that come close without meeting.
	UPROPERTY(EditAnywhere, Category = "Boundaries")
	bool bHighlightSeams = true;

	// Weld this region to whatever it nearly touches, at the tolerance above.
	UFUNCTION(CallInEditor, Category = "Boundaries", meta = (DisplayName = "Weld To Neighbours"))
	void WeldToNeighbours();

	// Outlines every other region while the tool is up, so you can see what you are lining up with.
	UPROPERTY(EditAnywhere, Category = "Display")
	bool bShowOtherRegions = true;

	TWeakObjectPtr<class UPlaceRegionEditTool> OwningTool;
};

// The label on the region currently being edited.
UCLASS()
class UPlaceRegionEditTargetProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Selected Region", meta = (DisplayName = "Region"))
	FString RegionLabel = TEXT("none — click a region to edit it");

	UPROPERTY(EditAnywhere, Category = "Selected Region")
	FPlaceName Name;

	UPROPERTY(EditAnywhere, Category = "Selected Region")
	TObjectPtr<UPlaceLabelTypeAsset> Type;

	UPROPERTY(EditAnywhere, Category = "Selected Region", meta = (MultiLine = true))
	FText Note;

	UPROPERTY(VisibleAnywhere, Category = "Selected Region")
	int32 CornerCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Selected Region", DisplayName = "Area (m2)")
	double AreaSquareMetres = 0.0;

	// Removes every selected corner.
	UFUNCTION(CallInEditor, Category = "Selected Region", meta = (DisplayName = "Delete Selected Corners"))
	void DeleteSelectedCorners();

	// Deselects, so the next click picks a different region rather than editing this one.
	UFUNCTION(CallInEditor, Category = "Selected Region", meta = (DisplayName = "Done Editing"))
	void DoneEditing();

	TWeakObjectPtr<class UPlaceRegionEditTool> OwningTool;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

// Reshape a region that has already been placed, without leaving Place Labels mode.
UCLASS()
class UPlaceRegionEditTool : public UInteractiveTool,
							 public IClickDragBehaviorTarget,
							 public IHoverBehaviorTarget
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;

	// Regions are drawn, deleted and imported while this tool is up, and none of that notifies it.
	virtual void OnTick(float DeltaTime) override;

	// IClickDragBehaviorTarget.
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override;

	virtual void OnUpdateModifierState(int ModifierID, bool bIsOn) override {}

	// IHoverBehaviorTarget
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;

	// Driven by the mode's global key handler and the property set buttons.
	void DeleteSelectedCorners();
	void ClearTarget();
	void WeldTargetToNeighbours();
	// Writes the panel's fields back onto the target. NAME_None writes all of them; a property
	// name writes just that one, so an edit made in the mode panel is not carried over by a
	// snapshot taken before it.
	void ApplyTargetProperties(FName ChangedProperty = NAME_None);
	void SelectAllCorners();

	bool HasTarget() const { return TargetRegion.IsValid(); }

	// The region picked up by a click. The mode panel's form follows it, so clicking a polygon in
	// the viewport is how a region is put in front of you rather than hunting it down in the list.
	UPlaceRegionComponent* GetTargetRegion() const { return TargetRegion.Get(); }
	bool HasCornerSelection() const { return SelectedCorners.Num() > 0; }

	FText GetStagePromptText() const;
	FText GetStatusText() const;

protected:
	void RegisterSettings(UInteractiveToolPropertySet* PropertySet, bool bPersist = true);

	UWorld* GetTargetWorld() const;
	bool TraceGround(const FInputDeviceRay& Ray, FVector& OutHit) const;

	void RefreshCachedRegions();
	void RefreshSeams();
	void PushTargetToPanel();
	void SetTarget(UPlaceRegionComponent* Region);

	// The region whose outline is nearest the given world point, preferring one that contains it.
	UPlaceRegionComponent* PickRegionAt(const FVector& WorldPoint) const;

	// Screen-constant handle sizing.
	double PickRadiusAt(const FVector& WorldPoint) const;
	double HandleSizeAt(const FVector& WorldPoint) const;

	enum class EHover : uint8
	{
		None,
		Corner,
		Edge,
		OtherRegion,
	};

	void UpdateHoverState(const FVector& WorldHit);

	// Snapshot / apply, so a drag produces exactly one undo step without holding a transaction open across every mouse-move.
	void BeginPointEdit();
	void CommitPointEdit(const FText& TransactionName);

	UPROPERTY()
	TObjectPtr<UClickDragInputBehavior> ClickDragBehavior;

	UPROPERTY()
	TObjectPtr<UMouseHoverBehavior> HoverBehavior;

	UPROPERTY()
	TObjectPtr<UPlaceRegionEditToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UPlaceRegionEditTargetProperties> TargetPanel;

	UPROPERTY()
	TArray<TObjectPtr<UInteractiveToolPropertySet>> RegisteredSettings;

	TWeakObjectPtr<UPlaceRegionComponent> TargetRegion;
	TSet<int32> SelectedCorners;

	TArray<TWeakObjectPtr<UPlaceRegionComponent>> CachedRegions;
	TArray<PlaceLabelsTopology::FSeamIssue> CachedSeams;

	// Hover
	EHover HoverKind = EHover::None;
	int32 HoverCorner = INDEX_NONE;
	int32 HoverEdge = INDEX_NONE;
	FVector HoverPoint = FVector::ZeroVector;
	bool bHoverValid = false;
	TWeakObjectPtr<UPlaceRegionComponent> HoverRegion;
	FString SnapDetail;

	// Drag
	bool bDragging = false;
	bool bDragMoved = false;
	FVector2D PressPixel = FVector2D::ZeroVector;
	FVector DragStartWorld = FVector::ZeroVector;
	TArray<FVector2D> PreEditLocalPoints;
	TArray<FVector2D> DragStartLocalPoints;

	double TimeSinceCacheRefresh = 0.0;

	// Cached from the last Render, so hover picking can size handles the same way they are drawn.
	FVector CameraPosition = FVector::ZeroVector;
	double WorldPerPixelOrtho = 1.0;
	double WorldPerPixelPerUnitDistance = 0.001;
	bool bCameraIsOrthographic = false;
};

UCLASS()
class UPlaceRegionEditToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
