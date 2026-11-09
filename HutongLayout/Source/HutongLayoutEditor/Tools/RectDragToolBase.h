#pragma once

#include "CoreMinimal.h"
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongActorSpawn.h"
#include "Generation/HutongDetail.h"
#include "Generation/HutongMetadata.h"
#include "Generation/HutongFootprint.h"
#include "Generation/BaySide.h"
#include "Tools/HutongSnap.h"
#include "Tools/HutongWallRun.h"
#include "RectDragToolBase.generated.h"

class USingleClickInputBehavior;
class UMouseHoverBehavior;
class UClickDragInputBehavior;
class AStaticMeshActor;
class FPrimitiveDrawInterface;
class FCanvas;

UCLASS()
class UHutongAppearanceProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Appearance", meta=(HutongAdvanced, ShowOnlyInnerProperties, ToolTip="Colours and materials for each surface of new placements."))
	FHutongPalette Palette;
};

UCLASS()
class UHutongDetailProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Detail", meta=(HutongAdvanced, DisplayName="Detail Level", ToolTip="How much detail new placements are built with."))
	EHutongDetail Level = EHutongDetail::Near;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Detail", meta=(HutongAdvanced, DisplayName="Bespoke Mesh", ToolTip="Marks new placements as carrying a bespoke mesh."))
	bool bBespokeMesh = false;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Detail", meta=(HutongAdvanced, DisplayName="Build LOD Chain", ToolTip="Also bakes every cheaper detail level as an LOD of each new placement."))
	bool bBuildLODChain = true;
};

// What the next placement is worth as evidence; stamped onto each placement, editable there.
// On the tool because a block is traced in one sitting off one sheet.
UCLASS()
class UHutongMetadataProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Metadata", meta=(DisplayName="Confidence", ToolTip="Confidence stamped on new placements, 5 to 1."))
	EHutongConfidence Confidence = EHutongConfidence::Attested;

	UPROPERTY(EditAnywhere, Category="Metadata", meta=(DisplayName="Notes", MultiLine=true, ToolTip="Free text stamped onto each new placement."))
	FString Notes;
};

UCLASS()
class UHutongSnapProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	// On by default: a run beside another is meant to meet it, and a small gap is invisible until
	// walked. Rotation snapping, the irritant, is its own switch. The snap key inverts per placement.
	UPROPERTY(EditAnywhere, Category="Snapping", meta=(DisplayName="Snap To Placed Buildings", ToolTip="Snaps placements to placed footprints."))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category="Snapping", meta=(HutongAdvanced, DisplayName="Snap Radius", EditCondition="bEnabled", UIMin="10", UIMax="200", ClampMin="1", Units="cm", ToolTip="Distance within which the cursor snaps to a placed footprint, in cm."))
	double Radius = 55.0;

	// Off by default: a yaw jumping under the cursor reads as the building turning itself.
	// Gates hold-R pulling onto a neighbour's line and the lane snap's bearing; an anchor snapped onto
	// a building takes that building's rotation regardless (the click, not the hover).
	UPROPERTY(EditAnywhere, Category="Snapping", meta=(HutongAdvanced, DisplayName="Adopt Neighbour Angle", EditCondition="bEnabled", ToolTip="Rotates the placement onto a neighbour's line when snapped."))
	bool bAdoptAngle = false;

	UPROPERTY(EditAnywhere, Category="Snapping", meta=(HutongAdvanced, DisplayName="Snap Lane To Street Module (胡同/小街/大街)", EditCondition="bEnabled", ToolTip="Snaps the run to a standard street width."))
	bool bSnapLaneWidth = true;

	UPROPERTY(EditAnywhere, Category="Snapping", meta=(HutongAdvanced, DisplayName="Lane Snap Tolerance", EditCondition="bEnabled && bSnapLaneWidth", UIMin="5", UIMax="100", ClampMin="1", Units="cm", ToolTip="Tolerance for snapping to a standard lane width, in cm."))
	double LaneToleranceCm = 30.0;

	UPROPERTY(EditAnywhere, Category="Snapping", meta=(HutongAdvanced, DisplayName="Angle Tolerance", EditCondition="bEnabled && bAdoptAngle", UIMin="1", UIMax="25", ClampMin="0", Units="deg", ToolTip="Angle within which a rotation snaps to a neighbour's line, in degrees."))
	double AngleToleranceDeg = 7.0;
};

UCLASS()
class UHutongPlacementProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, Category="Placement", meta=(HutongAdvanced, Units="cm", ToolTip="Width of the rectangle being placed, in cm."))
	double Width = 0.0;

	UPROPERTY(VisibleAnywhere, Category="Placement", meta=(HutongAdvanced, Units="cm", ToolTip="Depth of the rectangle being placed, in cm."))
	double Depth = 0.0;

	UPROPERTY(VisibleAnywhere, Category="Placement", meta=(HutongAdvanced, Units="cm", ToolTip="Height the placement is previewed at, in cm."))
	double Height = 0.0;

	UPROPERTY(VisibleAnywhere, Category="Placement", meta=(HutongAdvanced, Units="deg", ToolTip="Yaw of the rectangle being placed, in degrees."))
	double Rotation = 0.0;

	UPROPERTY(VisibleAnywhere, Category="Placement", meta=(HutongAdvanced, ToolTip="Extra readout for the current tool, such as the bay count."))
	FString Detail;
};

UCLASS(Abstract)
class URectDragToolBase : public UInteractiveTool, public IClickBehaviorTarget, public IHoverBehaviorTarget, public IClickDragBehaviorTarget
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;

	virtual FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos) override;
	virtual void OnClicked(const FInputDeviceRay& ClickPos) override;

	virtual void OnUpdateModifierState(int ModifierID, bool bIsOn) override {}

	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override;

	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;

	virtual void CancelPlacement();
	bool FinishPlacement();
	virtual bool CanFinishPlacement() const { return false; }

	void BeginRotateMode();
	void EndRotateMode();
	virtual void ToggleOpeningMark() { bOpeningMarked = !bOpeningMarked; }
	bool bOpeningMarked = false;
	bool IsPlacingActive() const { return bIsDragging; }

	bool IsHoverUnderCursor() const { return HudBuilding.IsValid(); }
	class UHutongBuildingComponent* GetHoverBuilding() const { return HudBuilding.Get(); }
	// A handle drag in progress; tool switches wait until it ends.
	bool IsEditingPlan() const { return PlanEdit != EPlanEdit::None; }

	virtual void AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse) {}

	virtual void FlipFacing() {}

	virtual void AdjustHeight(double DeltaCm) {}

	// Corner-post preview height: the value AdjustHeight moves. Zero disables.
	virtual double GetPreviewHeight() const { return 0.0; }

	virtual double GetLaneFaceOffset() const { return 0.0; }

	virtual bool WantsLaneWidthSnap() const { return true; }

	virtual FText GetStagePromptText() const;

	virtual TArray<FText> GetToolHelpLines() const;
	virtual bool HasRotateKey() const { return true; }

	virtual FText GetKeyHintText() const;

	bool TurnSelectedFacing(int32 Delta);
	bool FlipSelectedFacing();
	bool AdjustSelectedBays(int32 Delta);
	bool MoveSelectedDoorBay(int32 StepsRight);
	bool ToggleSelectedGate();
	bool AddSelectedGate();
	// R with nothing being placed: false with nothing to turn, leaving the key to the editor.
	bool ToggleTurnMode();
	void EndTurnMode();
	bool IsTurning() const { return bTurnMode; }

	bool CycleSelectedPreset(int32 Delta);
	bool CycleSelectedType(int32 Delta);
	bool OpenSelectedMenu(bool bTypes, const FVector2D& ScreenPosition);
	void ChooseForSelection(int32 TypeIndex, const FString& Preset);
	// Enter: when it would replace customized values, the first Enter only warns and a second applies.
	bool ConfirmSelectedCycle();
	bool CancelSelectedCycle();
	FText GetPendingCycleText() const;
	bool GetPendingCycleLines(FText& OutTitle, TArray<FText>& OutDetails, bool& bOutWarning) const;

	virtual TArray<FText> GetStageNames() const;
	virtual int32 GetStageIndex() const;

	static FVector2D HudCorner(FCanvas* Canvas);

	static FVector2D ClampToViewport(FCanvas* Canvas, const FVector2D& At, const FVector2D& BlockSize);
	static double ViewportWidth(FCanvas* Canvas);
	static TArray<FString> WrapToWidth(const FString& Text, const UFont* Font, double MaxWidth);

	FText GetPlacementSummaryText() const;

	FText GetHoverSummaryText(bool bWithPending = true) const;

protected:
	TWeakObjectPtr<class UHutongBuildingComponent> HoveredBuilding;

	FVector HoveredWorldPoint = FVector::ZeroVector;

	mutable TWeakObjectPtr<class UHutongBuildingComponent> HudBuilding;
	mutable FVector HudWorldPoint = FVector::ZeroVector;

	FVector2D AnchorSnapInward = FVector2D::ZeroVector;
	TWeakObjectPtr<class UHutongBuildingComponent> AnchorSnapBuilding;

	enum class EPlanEdit : uint8 { None, Move, Resize, Rotate, Opening, Skew, RunVertex, Marquee, Divide, Fuse, GroupMove, GroupScale, GroupRotate };
	FVector MarqueeStart = FVector::ZeroVector;
	FVector MarqueeEnd = FVector::ZeroVector;
	EPlanEdit PlanEdit = EPlanEdit::None;
	TWeakObjectPtr<class UHutongBuildingComponent> EditedPlan;
	int32 EditHandle = INDEX_NONE;
	FTransform EditStartTransform;
	FVector2D EditStartSize = FVector2D::ZeroVector;
	FHutongFootprintSkew EditStartSkew;
	// 0 local X, 1 local Y, INDEX_NONE undecided.
	int32 EditSkewAxis = INDEX_NONE;
	bool bSkewAxisHeld = false;
	FVector EditGrabLocal = FVector::ZeroVector;
	struct FRunLegState
	{
		TWeakObjectPtr<class UHutongBuildingComponent> Building;
		FTransform Transform;
		FVector2D Size = FVector2D::ZeroVector;
		FHutongFootprintSkew Skew;
		bool bAlongY = false;
		double StartExtend = 0.0;
		double EndExtend = 0.0;
	};
	HutongWallRun::FRun EditRun;
	TArray<FRunLegState> EditRunStart;
	int32 EditRunVertex = INDEX_NONE;
	bool bEditRunSideOn = false;
	static void CaptureLegState(class UHutongBuildingComponent* Building, FRunLegState& Out);
	static void RestoreLegState(const FRunLegState& State);
	TArray<class UHutongWallBuildingComponent*> GatherWallLegs() const;
	double EditStartCursorAngleDeg = 0.0;
	double EditStartYawDeg = 0.0;
	FVector EditPivot = FVector::ZeroVector;
	double EditStartOpeningCentre = 0.0;
	bool bPlanDragMoved = false;
	// Capture sends drag events for an unmoved cursor; the slop is measured from this pixel.
	FVector2D PlanPressPixel = FVector2D::ZeroVector;
	bool bPlanPressHasPixel = false;
	int32 HoverPlanHandle = INDEX_NONE;

	// Handle ids: 0..3 corners anticlockwise from origin, 4..7 edge midpoints (−Y, +X, +Y, −X), 8 rotate ring, 9 inside.
	static constexpr int32 PlanHandleRing = 8;
	static constexpr int32 PlanHandleInside = 9;
	// 10+: wall opening sliders, in GetPlanOpenings order.
	static constexpr int32 PlanHandleOpening = 10;
	static constexpr int32 PlanHandleFuseStart = 1000;
	static constexpr int32 PlanHandleFuseEnd = 1001;
	// 1010+: divide markers on interior bay lines, GetPlanBays order from 1.
	static constexpr int32 PlanHandleBay = 1010;

	TArray<class UHutongBuildingComponent*> GetGroupMembers() const;
	struct FGroupBox
	{
		double YawDeg = 0.0;
		FVector2D Min = FVector2D::ZeroVector;
		FVector2D Max = FVector2D::ZeroVector;
		double Z = 0.0;
		FVector2D Corner(int32 Index) const;
		FVector World(const FVector2D& Frame) const;
	};
	static bool GetGroupBox(const TArray<class UHutongBuildingComponent*>& Members, FGroupBox& Out);
	// A few-pixel floor: a group may span a city.
	double GroupHandleSize(const FGroupBox& Box) const;
	FVector GroupHandleWorld(const FGroupBox& Box, int32 Handle) const;
	int32 HitTestGroup(const FVector& Ground) const;
	void BeginGroupEdit(int32 Hit, const FVector& Ground);
	void UpdateGroupEdit(const FVector& Ground);
	void CommitGroupEdit();
	void CancelGroupEdit();
	void DrawGroupHandles(class FPrimitiveDrawInterface* PDI) const;
	bool IsGroupEdit() const { return PlanEdit == EPlanEdit::GroupMove || PlanEdit == EPlanEdit::GroupScale || PlanEdit == EPlanEdit::GroupRotate; }
	TArray<FRunLegState> GroupStart;
	FGroupBox GroupStartBox;
	FVector GroupPressGround = FVector::ZeroVector;
	int32 GroupGrabbed = INDEX_NONE;
	TArray<HutongSnap::FFootprint> GroupSnapTargets;
	int32 HoverGroupHandle = INDEX_NONE;

	bool bTurnMode = false;
	TArray<class UHutongBuildingComponent*> GetTurnMembers() const;
	bool bTurnCursor = false;
	FVector TurnCursorWorld = FVector::ZeroVector;
	void DrawTurnHint(class FCanvas* Canvas, class IToolsContextRenderAPI* RenderAPI) const;

	struct FPlanPivot
	{
		TWeakObjectPtr<class UHutongBuildingComponent> Building;
		int32 Corner = INDEX_NONE;
		TArray<TWeakObjectPtr<class AActor>> Selection;
	};
	TOptional<FPlanPivot> Pivot;
	bool bHoverPivot = false;
	FVector HoverPivotWorld = FVector::ZeroVector;
	bool ResolvePivot(FVector& Out) const;
	bool FindPivotCorner(const FVector& Ground, FPlanPivot& Out, FVector& OutWorld) const;
	void DrawPivot(class FPrimitiveDrawInterface* PDI) const;
	static void DrawGroupCircle(class FPrimitiveDrawInterface* PDI, const FVector& C, double R, const FLinearColor& Colour, float Thickness, double Turn = 2.0 * PI);

	class UHutongBuildingComponent* FindFuseNeighbour(const class UHutongBuildingComponent* Building, bool bAtEnd) const;
	FVector PlanBayMarkerWorld(const class UHutongBuildingComponent* Building, double Along) const;
	// A quarter of the depth off centre: on the centre it covers the end edge's resize handle.
	FVector FuseMarkerWorld(const class UHutongBuildingComponent* Building, bool bAtEnd) const;

	struct FJoint
	{
		FRunLegState Start;
		// The neighbour's edge on the join: 4..7 as the handle ids (−Y, +X, +Y, −X).
		int32 Edge = INDEX_NONE;
	};
	TArray<FJoint> Joints;
	void CaptureJoint(class UHutongBuildingComponent* Building, int32 Hit);
	void CaptureJointOnEdge(class UHutongBuildingComponent* Building, int32 Edge);
	void UpdateJoint();
	bool HasJoint() const { return Joints.Num() > 0; }
	void DivideAtMarker(class UHutongBuildingComponent* Building, int32 BayLine);
	void FuseAtMarker(class UHutongBuildingComponent* Building, bool bAtEnd);

	struct FPendingCycle
	{
		TArray<TWeakObjectPtr<class UHutongBuildingComponent>> Buildings;
		int32 TypeIndex = INDEX_NONE;	// into HutongDetailOps::ConvertTargets(); none = keep each type
		FString Preset;
		bool bChosen = false;			// a preset or type has been picked, so Enter has something to apply
		bool bArmed = false;			// warned of customizations; the next Enter applies
		TArray<FString> Customized;
		FText Note;						// why P or T could not pick
	};
	FPendingCycle Pending;
	bool IsPendingFor(const TArray<class UHutongBuildingComponent*>& Selected) const;
	void HoldPendingFor(const TArray<class UHutongBuildingComponent*>& Selected);
	TArray<class UHutongBuildingComponent*> PendingWork(const TArray<class UHutongBuildingComponent*>& Selected) const;

	class UHutongBuildingComponent* GetSelectedBuilding() const;
	class UHutongBuildingComponent* GetSelectedPlanBuilding() const;
	bool SetSelectedFacing(const FText& Title,
		TFunctionRef<EHutongBaySide(const class UHutongBuildingComponent&, EHutongBaySide)> NextSide);
	FVector PlanOpeningWorld(const class UHutongBuildingComponent* Building, int32 Index, double& OutWidth) const;
	static FVector PlanHandleLocal(const FVector2D Corners[4], int32 Handle);
	static bool IsSkewKeyDown();
	// Registered Shift binding on the drag behaviour, so the editor's click-selection ranks below it.
	static constexpr int32 SkewModifierID = 1;
	static double PlanHandleSize(const FVector2D& Size);
	static double PlanCornerHandleSize(const FVector2D& Size);
	double SkewCornerRadius(const FVector2D& Size, const FVector& At) const;
	static bool IsLineLikePlan(const FVector2D& Size);
	static bool PlanHandleEnabled(const FVector2D& Size, int32 Handle);
	bool IsPlanGrabPoint(const FVector2D& Size, double EdgeDistance, const FVector& Ground) const;
	bool PressStartsPlacement(const FVector& Ground, const class UHutongBuildingComponent* Selected, int32 SelectedHit) const;
	static bool IsOnOutline(const FVector& Point, const class UHutongBuildingComponent* Building);
	class UHutongBuildingComponent* ShiftToggleTarget(const FInputDeviceRay& Ray, const FVector& Ground, int32 SelectedHit) const;
	virtual bool StartsOnFootprintEdges() const { return false; }
	virtual bool PointSnapsOnly() const { return false; }
	virtual bool HasPlacement() const { return true; }
	virtual bool BuildsGeometry() const { return true; }

	// A run tool calls it from its hover and again on the click, which re-reads the cursor.
	void StepCursorBearing(double BaseYawDeg);

	static double SizeStepCm();
	FVector StepDrawnSize(const FVector& World) const;
	FVector PlanRotateHandleWorld(const class UHutongBuildingComponent* Building, FVector& OutEdgeMid) const;
	int32 HitTestPlan(const class UHutongBuildingComponent* Building, const FVector& Ground) const;
	void BeginPlanEdit(class UHutongBuildingComponent* Building, int32 Hit, const FVector& Ground);
	void UpdatePlanEdit(const FVector& Ground);
	void CommitPlanEdit();
	void CancelPlanEdit();
	void DrawPlanHandles(FPrimitiveDrawInterface* PDI) const;
	FText GetPlanEditPromptText() const;


	void UpdateHoverInspectionFromViewport();
	void DrawSelectedFootprints(FPrimitiveDrawInterface* PDI) const;
	void DrawHoverInspection(FPrimitiveDrawInterface* PDI) const;
	void DrawHoverInspectionHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) const;
	void DrawBuildingLabels(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) const;
	void DrawPendingCycleBanner(FCanvas* Canvas) const;
	static FString AsciiForBitmapFont(const FString& Text);

	bool TraceHoveredBuilding(const FInputDeviceRay& Ray, FHitResult& OutHit) const;

	// Ground query: laid-out buildings have no collision.
	class UHutongBuildingComponent* FindPlanBuildingAt(const FVector& Ground, double* OutEdgeDistance = nullptr) const;

	double WorldPerPixelAt(const FVector& P) const;

	bool SnappingActive() const;

	// Latched: a click starting with Alt down goes to the camera (Alt-orbit), so the key is read on hover frames.
	bool IsSnapKeyLatched() const;

	static bool IsSnapKeyDown();

	FText SnapKeyClause() const;

	const TArray<HutongSnap::FFootprint>& GetFootprints() const;

	// Each hover trace is a complex-collision query: held while the cursor pixel is unchanged.
	mutable FIntPoint LastHoverCursorPx = FIntPoint(-1, -1);
	mutable bool bHoverTraceValid = false;

	mutable bool bSnapKeyLatched = false;

	double EffectiveSnapRadius(const FVector& P) const;
	static constexpr double SnapPixelFloor = 14.0;

	// Subclass contract: build mesh in local space (origin at rectangle min corner).
	virtual void BuildMeshForRect(double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Level) {}
	virtual FString GetActorNameBase() const { return TEXT("HutongActor"); }

	virtual void AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY) {}

	EHutongDetail GetDetailLevel() const;

	bool ShouldBuildLODChain() const;

	int32 BuildLODsForRect(double SizeX, double SizeY, TArray<UE::Geometry::FDynamicMesh3>& OutLODs, HutongGen::FMeshTurn* OutTurn = nullptr);

	void StampDetail(class UHutongBuildingComponent* Building) const;

	static bool IsPlanOnly();

	// Returns LOCAL-frame bounds (offsets from StartWorld in the placement-yaw frame).
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const;

	static void HoldExtentAtCursor(double& Lo, double& Hi, double Size);

	void RegisterSettings(UInteractiveToolPropertySet* PropertySet, bool bPersist = true);

	virtual void RegisterToolSettings() {}

	virtual FString GetPlacementDetail() const { return FString(); }

	virtual void OnPlacementStarted(const FVector& HitWorld) {}
	// Return true to spawn immediately; false to defer to OnExtraStageClicked.
	virtual bool OnRectCommitted(const FVector& HitWorld) { return true; }
	// Called for clicks after the rect is committed; return true when ready to spawn.
	virtual bool OnExtraStageClicked(const FVector& HitWorld) { return true; }
	virtual void OnPlacementHover(const FVector& HitWorld) {}

	EHutongBaySide ComputeDefaultBaySide() const;
	EHutongBaySide ComputeClosestSide(double Hx, double Hy) const;

	FVector2D WorldXYToLocalRect(const FVector& World) const;
	FVector LocalRectToWorld(double LocalX, double LocalY) const;
	FVector LocalRectToWorldFrom(const FVector& Origin, double LocalX, double LocalY) const;

	virtual void RenderIdlePreview(FPrimitiveDrawInterface* PDI, const FVector& CursorGround) {}
	virtual bool DrawsRectFootprint() const { return true; }

	bool GetViewportCursorGround(FVector& OutGround) const;

	void UpdateRotateFromCursor(const FVector& CursorWorld);

	// At maps (along the facade from its start, depth inward from it) to world.
	static void DrawBaysAndFacing(FPrimitiveDrawInterface* PDI, TFunctionRef<FVector(double, double)> At,
		double Span, double Across, const TArray<double>& Bounds, int32 DoorBay = INDEX_NONE);
	void DrawRectBaysAndFacing(FPrimitiveDrawInterface* PDI, EHutongBaySide Side, double MinX, double MinY,
		double MaxX, double MaxY, const TArray<double>& Bounds, int32 DoorBay = INDEX_NONE) const;

	static void DrawPreviewLine(FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B,
		const FLinearColor& Color, float Thickness);

	// DashLength in world cm.
	static void DrawDashedPreviewLine(FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B,
		const FLinearColor& Color, float Thickness, double DashLength = 30.0);

	UPROPERTY()
	TObjectPtr<USingleClickInputBehavior> ClickBehavior;

	UPROPERTY()
	TObjectPtr<UMouseHoverBehavior> HoverBehavior;

	UPROPERTY()
	TObjectPtr<class UClickDragInputBehavior> DragBehavior;

	UPROPERTY()
	TObjectPtr<UHutongAppearanceProperties> Appearance;

	UPROPERTY()
	TObjectPtr<UHutongDetailProperties> DetailSettings;

	UPROPERTY()
	TObjectPtr<UHutongMetadataProperties> MetadataSettings;

	UPROPERTY()
	TObjectPtr<UHutongSnapProperties> Snap;

	// The tool's preset picker, found by RegisterSettings so no tool names it; read by StampDetail.
	UPROPERTY()
	TObjectPtr<class UHutongPresetProperties> PresetSettings;

	FString CacheIdentifierFor(const UInteractiveToolPropertySet* PropertySet) const;

	// Call after both picker and params are registered: the params restore overwrites a preset loaded earlier.
	void ApplyDefaultPreset(class UHutongPresetProperties* Picker, const FString& Name);

	FVector ApplySnap(const FVector& World, bool bIsAnchor);

	bool bSnapActive = false;
	FVector SnapPoint = FVector::ZeroVector;
	HutongSnap::FResult StickyCursorSnap;
	bool bStickyCursorValid = false;
	void ClearSnapBearings();

	// Yaw of the anchor's snap target; -1000 for none.
	double AnchorSnapYawDeg = -1000.0;
	double CursorSnapYawDeg = -1000.0;
	double AnchorSnapYaw2Deg = -1000.0;
	double CursorSnapYaw2Deg = -1000.0;
	FVector2D CursorSnapInward = FVector2D::ZeroVector;
	TWeakObjectPtr<class UHutongBuildingComponent> CursorSnapBuilding;

	double MiterExtend(double RunYawDeg, double NeighbourYawDeg, double Thickness) const;

	UPROPERTY()
	TObjectPtr<UHutongPlacementProperties> Placement;

	UPROPERTY()
	TArray<TObjectPtr<UInteractiveToolPropertySet>> RegisteredSettings;

	FVector StartWorld = FVector::ZeroVector;
	FVector CurrentWorld = FVector::ZeroVector;
	double PlacementYawDeg = 0.0;
	bool bIsDragging = false;
	bool bRectCommitted = false;
	bool bRotateModeActive = false;

	FVector2D RotateAnchorLocalRect = FVector2D::ZeroVector;
	double RotateAnchorCursorAngleDeg = 0.0;
	double RotateAnchorYawDeg = 0.0;
	double RotateAnchorCursorDist = 0.0;
	FVector PlacementCursorWorld = FVector::ZeroVector;

	void ProcessClick(const FVector& Hit);

	bool TryRayHitGround(const FInputDeviceRay& Ray, FVector& OutHit) const;
	FInputRayHit GroundRayHit(const FInputDeviceRay& Ray) const;
	virtual void SpawnFinalActor();
	void SpawnAndEndPlacement();
	void AdoptNeighbourBaseCourse(AStaticMeshActor* Actor);
	void AdoptBaseCourseFrom(AStaticMeshActor* Actor, const class UHutongBuildingComponent* Neighbour);


	void UpdatePlacementReadout();
};
