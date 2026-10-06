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

	// IClickBehaviorTarget
	virtual FInputRayHit IsHitByClick(const FInputDeviceRay& ClickPos) override;
	virtual void OnClicked(const FInputDeviceRay& ClickPos) override;

	// IModifierToggleBehaviorTarget (from IClickBehaviorTarget/IHoverBehaviorTarget)
	virtual void OnUpdateModifierState(int ModifierID, bool bIsOn) override {}

	// IClickDragBehaviorTarget — press-drag-release on a laid-out building.
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override;

	// IHoverBehaviorTarget
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;

	// Abort any in-progress placement (Escape key).
	virtual void CancelPlacement();

	// Hold R to enter mouse-driven rotation around StartWorld (Z axis).
	void BeginRotateMode();
	void EndRotateMode();
	// G toggles the mark on the segment being drawn: that segment carries the run's opening.
	virtual void ToggleOpeningMark() { bOpeningMarked = !bOpeningMarked; }
	bool bOpeningMarked = false;
	bool IsPlacingActive() const { return bIsDragging; }

	// True when GetHoverSummaryText describes the cursor's building, not the fallback selection.
	bool IsHoverUnderCursor() const { return HudBuilding.IsValid(); }
	// A handle drag in progress; no tool switch may interrupt it.
	bool IsEditingPlan() const { return PlanEdit != EPlanEdit::None; }

	// [ and ] during placement.
	virtual void AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse) {}

	// F during placement: flips the tool's second facing (street row's gates).
	virtual void FlipFacing() {}

	// - and = adjust the tool's primary height by DeltaCm.
	virtual void AdjustHeight(double DeltaCm) {}

	// Corner-post preview height: the value AdjustHeight moves. Zero disables.
	virtual double GetPreviewHeight() const { return 0.0; }

	// Distance from the cursor's line to this run's own face.
	virtual double GetLaneFaceOffset() const { return 0.0; }

	// False when the placement forms no street (a run inside a compound): no lane-width snap.
	virtual bool WantsLaneWidthSnap() const { return true; }

	// One-line instruction for the current placement stage.
	virtual FText GetStagePromptText() const;

	// The help lines shown in the mode panel, in order.
	virtual TArray<FText> GetToolHelpLines() const;
	// False drops the hold-R line from the help.
	virtual bool HasRotateKey() const { return true; }

	// The one always-visible line of keys; help lines sit behind a collapsed header.
	virtual FText GetKeyHintText() const;

	// [ and ] with nothing being placed: facade a step round (the building's FacadeTurnStep each).
	bool TurnSelectedFacing(int32 Delta);
	// F with nothing being placed: facade to the other side, bays and door with it.
	bool FlipSelectedFacing();
	// [ and ] with nothing being placed: one bay fewer or more on each selected building that counts them.
	bool AdjustSelectedBays(int32 Delta);
	// G with nothing being placed: the gate on each selected wall, on or off.
	bool ToggleSelectedGate();

	// P / T with nothing being placed: step the selection's pending preset or building type (Delta -1
	// with Shift); nothing changes until Enter. False with no building selected.
	bool CycleSelectedPreset(int32 Delta);
	bool CycleSelectedType(int32 Delta);
	// P / T: a menu at the cursor of the selection's presets, or of every type with its presets; a pick
	// is applied as Enter would (warning first when values would be lost). False with no building selected.
	bool OpenSelectedMenu(bool bTypes, const FVector2D& ScreenPosition);
	// A menu pick: type (INDEX_NONE keeps each building's) and preset, applied at once.
	void ChooseForSelection(int32 TypeIndex, const FString& Preset);
	// Enter: applies the pending change; when it would replace customized values, the first Enter
	// only warns and a second applies. False with nothing pending.
	bool ConfirmSelectedCycle();
	// Esc: drops the pending change; false when there was none.
	bool CancelSelectedCycle();
	// The pending change and how to apply it, or its warning; empty with nothing pending.
	FText GetPendingCycleText() const;
	// The same as a large title and smaller detail lines, for the viewport banner; false with nothing
	// pending. bWarning when the next Enter only confirms the warning.
	bool GetPendingCycleLines(FText& OutTitle, TArray<FText>& OutDetails, bool& bOutWarning) const;

	// Named clicks of a placement, and the one awaited.
	virtual TArray<FText> GetStageNames() const;
	virtual int32 GetStageIndex() const;

	// Fixed viewport corner the placement readout is drawn at.
	static FVector2D HudCorner(FCanvas* Canvas);

	// Keeps a following HUD text block inside the viewport.
	static FVector2D ClampToViewport(FCanvas* Canvas, const FVector2D& At, const FVector2D& BlockSize);
	// Viewport width in the HUD's DPI-independent units.
	static double ViewportWidth(FCanvas* Canvas);
	// Wraps to MaxWidth at spaces, mid-word only when needed.
	static TArray<FString> WrapToWidth(const FString& Text, const UFont* Font, double MaxWidth);

	// "820 x 540 cm · 37° · 3 bays @ 340 cm" — the HUD line under the stage prompt.
	FText GetPlacementSummaryText() const;

	// Readout of what the cursor rests on, polled by the mode panel.
	// bWithPending heads it with the pending P / T change (the panel); the viewport has its banner.
	FText GetHoverSummaryText(bool bWithPending = true) const;

protected:
	// Building under the cursor while nothing is being placed.
	TWeakObjectPtr<class UHutongBuildingComponent> HoveredBuilding;

	// World hit point on it.
	FVector HoveredWorldPoint = FVector::ZeroVector;

	// Outline drawn this frame, latched by Render for DrawHUD.
	mutable TWeakObjectPtr<class UHutongBuildingComponent> HudBuilding;
	mutable FVector HudWorldPoint = FVector::ZeroVector;

	// Direction into the neighbour at the anchor's snap; zero if unsnapped.
	FVector2D AnchorSnapInward = FVector2D::ZeroVector;
	// Building the anchor snapped to, for placements sized to what they join; dropped with the bearings.
	TWeakObjectPtr<class UHutongBuildingComponent> AnchorSnapBuilding;

	// ---- Editing a laid-out building in place ----
	// Skew: Shift-drag a corner handle off the rectangle.
	// RunVertex: a wall leg's end; every leg meeting at that run vertex moves, joins stay melded.
	// Marquee: open-ground drag box; selects what it covers on release.
	// Divide / Fuse: marker press released unmoved divides at the bay line / fuses with the like neighbour.
	enum class EPlanEdit : uint8 { None, Move, Resize, Rotate, Opening, Skew, RunVertex, Marquee, Divide, Fuse };
	FVector MarqueeStart = FVector::ZeroVector;
	FVector MarqueeEnd = FVector::ZeroVector;
	EPlanEdit PlanEdit = EPlanEdit::None;
	TWeakObjectPtr<class UHutongBuildingComponent> EditedPlan;
	int32 EditHandle = INDEX_NONE;
	FTransform EditStartTransform;
	FVector2D EditStartSize = FVector2D::ZeroVector;
	FHutongFootprintSkew EditStartSkew;
	// Ends mode: one axis per drag, from the dominant pull. 0 local X, 1 local Y, INDEX_NONE undecided.
	int32 EditSkewAxis = INDEX_NONE;
	bool bSkewAxisHeld = false;
	FVector EditGrabLocal = FVector::ZeroVector;
	// Vertex drag: the run's legs as they stood, and the moving vertex.
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
	// All wall legs in the level, for gathering a run.
	TArray<class UHutongWallBuildingComponent*> GatherWallLegs() const;
	double EditStartCursorAngleDeg = 0.0;
	double EditStartYawDeg = 0.0;
	double EditStartOpeningCentre = 0.0;
	bool bPlanDragMoved = false;
	// Press pixel, for the drag slop: capture sends drag events for an unmoved cursor, else a click leaves an empty undo entry.
	FVector2D PlanPressPixel = FVector2D::ZeroVector;
	bool bPlanPressHasPixel = false;
	// Handle under the cursor on the selected building, refreshed each frame.
	int32 HoverPlanHandle = INDEX_NONE;

	// Handle ids: 0..3 corners anticlockwise from origin, 4..7 edge midpoints (−Y, +X, +Y, −X), 8 rotate ring, 9 inside.
	static constexpr int32 PlanHandleRing = 8;
	static constexpr int32 PlanHandleInside = 9;
	// 10+: wall opening sliders, in GetPlanOpenings order.
	static constexpr int32 PlanHandleOpening = 10;
	// Fuse markers at origin / far end, only where a like neighbour stands on the line.
	static constexpr int32 PlanHandleFuseStart = 1000;
	static constexpr int32 PlanHandleFuseEnd = 1001;
	// 1010+: divide markers on interior bay lines, GetPlanBays order from 1.
	static constexpr int32 PlanHandleBay = 1010;

	// Like building end to end with this one on its line at that end, if any.
	class UHutongBuildingComponent* FindFuseNeighbour(const class UHutongBuildingComponent* Building, bool bAtEnd) const;
	// World position of a divide or fuse marker on the footprint's centre line.
	FVector PlanBayMarkerWorld(const class UHutongBuildingComponent* Building, double Along) const;
	// The fuse marker: on the join, a quarter of the depth off centre, clear of the end edge's resize handle.
	FVector FuseMarkerWorld(const class UHutongBuildingComponent* Building, bool bAtEnd) const;

	// Dragging an edge another laid-out building shares (not a wall), or a corner on it, slides the
	// join: the neighbour's facing edge follows unless Ctrl is held. Captured at the press, rewound and reapplied with the
	// edited building.
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
	// Acts once the marker press is released unmoved.
	void DivideAtMarker(class UHutongBuildingComponent* Building, int32 BayLine);
	void FuseAtMarker(class UHutongBuildingComponent* Building, bool bAtEnd);

	// A preset or type chosen with P / T for the selection, not yet applied. Dropped when the selection
	// changes.
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
	// Pending belongs to exactly this selection.
	bool IsPendingFor(const TArray<class UHutongBuildingComponent*>& Selected) const;
	// Starts a fresh pending change unless one is already held for this selection.
	void HoldPendingFor(const TArray<class UHutongBuildingComponent*>& Selected);
	// Selected buildings the pending change would alter.
	TArray<class UHutongBuildingComponent*> PendingWork(const TArray<class UHutongBuildingComponent*>& Selected) const;

	// The single selected building of any kind; the second only if laid out, not built.
	class UHutongBuildingComponent* GetSelectedBuilding() const;
	class UHutongBuildingComponent* GetSelectedPlanBuilding() const;
	// Every selected building with a facade to NextSide, in one transaction; false if none has one.
	bool SetSelectedFacing(const FText& Title,
		TFunctionRef<EHutongBaySide(const class UHutongBuildingComponent&, EHutongBaySide)> NextSide);
	FVector PlanOpeningWorld(const class UHutongBuildingComponent* Building, int32 Index, double& OutWidth) const;
	// Corners 0..3 include skew offsets; 4..7 are the edge midpoints between them.
	static FVector PlanHandleLocal(const FVector2D Corners[4], int32 Handle);
	// Shift on a corner handle skews instead of resizing; read live, like rotate.
	static bool IsSkewKeyDown();
	// Registered Shift binding on the drag behaviour, so selection cannot outrank it; state read live.
	static constexpr int32 SkewModifierID = 1;
	// Handles sized in world units off the footprint.
	static double PlanHandleSize(const FVector2D& Size);
	// Shift corner squares; on a run they fit its thickness, else an end's two merge.
	static double PlanCornerHandleSize(const FVector2D& Size);
	// Half-width a Shift corner is drawn at here: the handle size, never under a few pixels.
	double SkewCornerRadius(const FVector2D& Size, const FVector& At) const;
	// Wall, path, corridor: short side a fraction of the long.
	static bool IsLineLikePlan(const FVector2D& Size);
	static bool PlanHandleEnabled(const FVector2D& Size, int32 Handle);
	// Whether a ground point inside a footprint is a grab (select / move).
	bool IsPlanGrabPoint(const FVector2D& Size, double EdgeDistance, const FVector& Ground) const;
	// Whether a press here starts a placement though a footprint is under it: a tool drawing from
	// outlines (the wall), within snap reach of any outline, unless on a handle it still honours.
	bool PressStartsPlacement(const FVector& Ground, const class UHutongBuildingComponent* Selected, int32 SelectedHit) const;
	// Under Shift, the building a press toggles in the selection (laid out or built), unless the
	// press is on a handle Shift drives; null otherwise.
	// Whether a point lies on that building's footprint outline (within 2 cm), not on a face's line past it.
	static bool IsOnOutline(const FVector& Point, const class UHutongBuildingComponent* Building);
	class UHutongBuildingComponent* ShiftToggleTarget(const FInputDeviceRay& Ray, const FVector& Ground, int32 SelectedHit) const;
	virtual bool StartsOnFootprintEdges() const { return false; }
	// True for a tool whose ends snap onto neighbours' corners and faces and nothing else (the wall
	// run): no lane width, so it moves freely off a building.
	virtual bool PointSnapsOnly() const { return false; }
	// False for a tool that only edits what is placed (heights): clicks select, nothing is anchored.
	virtual bool HasPlacement() const { return true; }
	// False for a tool that only traces footprints: every placement is laid out, whatever the mode says.
	virtual bool BuildsGeometry() const { return true; }
	FVector PlanRotateHandleWorld(const class UHutongBuildingComponent* Building, FVector& OutEdgeMid) const;
	// Handle, ring or inside hit by a ground point; INDEX_NONE for none.
	int32 HitTestPlan(const class UHutongBuildingComponent* Building, const FVector& Ground) const;
	void BeginPlanEdit(class UHutongBuildingComponent* Building, int32 Hit, const FVector& Ground);
	void UpdatePlanEdit(const FVector& Ground);
	void CommitPlanEdit();
	void CancelPlanEdit();
	void DrawPlanHandles(FPrimitiveDrawInterface* PDI) const;
	// Prompt while a laid-out building is selected or edited; empty otherwise.
	FText GetPlanEditPromptText() const;


	// Hover trace off the editor's cursor, once a frame from Render.
	void UpdateHoverInspectionFromViewport();
	// Selected built buildings' footprints, white under their type colour, as a selected plan draws.
	void DrawSelectedFootprints(FPrimitiveDrawInterface* PDI) const;
	void DrawHoverInspection(FPrimitiveDrawInterface* PDI) const;
	void DrawHoverInspectionHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) const;
	// Type and preset on every footprint large enough on screen to hold them.
	void DrawBuildingLabels(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) const;
	// The pending P / T change, large, across the top of the viewport.
	void DrawPendingCycleBanner(FCanvas* Canvas) const;
	// Text the engine's bitmap fonts can draw: bracketed Chinese dropped, the middle dot a dash.
	static FString AsciiForBitmapFont(const FString& Text);

	// Whether the ray hits a building worth hovering.
	bool TraceHoveredBuilding(const FInputDeviceRay& Ray, FHitResult& OutHit) const;

	// Ground query: laid-out buildings have no collision.
	class UHutongBuildingComponent* FindPlanBuildingAt(const FVector& Ground, double* OutEdgeDistance = nullptr) const;

	// World cm per screen pixel at P, active viewport.
	double WorldPerPixelAt(const FVector& P) const;

	// Single answer for all six snap sites: the key inverts the checkbox for this placement.
	bool SnappingActive() const;

	// Alt/Option or Command, latched for the placement once seen: a click starting with Alt
	// down goes to the camera (Alt-orbit), never the tool, so the key is read on hover frames.
	bool IsSnapKeyLatched() const;

	// Raw key state, no latch; what the prompt describes.
	static bool IsSnapKeyDown();

	// What the key will do, given the current setting.
	FText SnapKeyClause() const;

	// Placed footprints, gathered once; every neighbour query reads this.
	const TArray<HutongSnap::FFootprint>& GetFootprints() const;

	// Render asks for hover every frame and each trace is a complex-collision query:
	// held while the cursor pixel is unchanged.
	mutable FIntPoint LastHoverCursorPx = FIntPoint(-1, -1);
	mutable bool bHoverTraceValid = false;

	// Set when the key is seen down; cleared when nothing is placed or edited.
	mutable bool bSnapKeyLatched = false;

	// Snap radius with its pixel floor applied at P.
	double EffectiveSnapRadius(const FVector& P) const;
	static constexpr double SnapPixelFloor = 14.0;

	// Subclass contract: build mesh in local space (origin at rectangle min corner).
	virtual void BuildMeshForRect(double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Level) {}
	virtual FString GetActorNameBase() const { return TEXT("HutongActor"); }

	// Attaches the editable building component, with the params and footprint the mesh was built from.
	virtual void AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY) {}

	// Next placement's level; stamped on the component so a rebuild does not revert to Near.
	EHutongDetail GetDetailLevel() const;

	// Whether the next placement bakes the cheaper levels as LODs.
	bool ShouldBuildLODChain() const;

	// LOD chain for the current placement, via BuildMeshForRect.
	int32 BuildLODsForRect(double SizeX, double SizeY, TArray<UE::Geometry::FDynamicMesh3>& OutLODs);

	// Copies both detail fields onto a freshly created component.
	void StampDetail(class UHutongBuildingComponent* Building) const;

	// The mode's "layout only" setting.
	static bool IsPlanOnly();

	// Returns LOCAL-frame bounds (offsets from StartWorld in the placement-yaw frame).
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const;

	// Carries a tool-fixed extent under the cursor, not in the anchor's quadrant.
	static void HoldExtentAtCursor(double& Lo, double& Hi, double Size);

	// Registers a tool property set.
	void RegisterSettings(UInteractiveToolPropertySet* PropertySet, bool bPersist = true);

	// Where subclasses register their own property sets.
	virtual void RegisterToolSettings() {}

	// Extra line for the placement readout and the viewport HUD, e.g. the bay count.
	virtual FString GetPlacementDetail() const { return FString(); }

	// Stage hooks. Override these instead of OnClicked / OnUpdateHover.
	virtual void OnPlacementStarted(const FVector& HitWorld) {}
	// Return true to spawn immediately; false to defer to OnExtraStageClicked.
	virtual bool OnRectCommitted(const FVector& HitWorld) { return true; }
	// Called for clicks after the rect is committed; return true when ready to spawn.
	virtual bool OnExtraStageClicked(const FVector& HitWorld) { return true; }
	// Called every hover tick during placement (after CurrentWorld / rotation handling).
	virtual void OnPlacementHover(const FVector& HitWorld) {}

	// Facade side: the camera's before the rect is committed, nearest a ground point after.
	EHutongBaySide ComputeDefaultBaySide() const;
	EHutongBaySide ComputeClosestSide(double Hx, double Hy) const;

	// Rotated rect-local frame anchored at StartWorld; yaw rotation is around world Z.
	FVector2D WorldXYToLocalRect(const FVector& World) const;
	FVector LocalRectToWorld(double LocalX, double LocalY) const;
	// Same frame on another origin, for previews before an anchor exists.
	FVector LocalRectToWorldFrom(const FVector& Origin, double LocalX, double LocalY) const;

	// Drawn each frame while nothing is being placed.
	virtual void RenderIdlePreview(FPrimitiveDrawInterface* PDI, const FVector& CursorGround) {}
	// False: the tool draws its own outline and height posts while placing (a round plan).
	virtual bool DrawsRectFootprint() const { return true; }

	// Editor cursor projected onto the ground plane.
	bool GetViewportCursorGround(FVector& OutGround) const;

	void UpdateRotateFromCursor(const FVector& CursorWorld);

	// Preview line: wider near-black pass, then the coloured line on top.
	// A placement's facing and bays, one look for every type: the facade edge heavy, a line right across at
	// each inner bay boundary, a chevron per bay pointing out through the facade, the door bay heavier on
	// the edge. At maps (along the facade from its start, depth inward from it) to world. Bounds run
	// 0..Span; fewer than two draws one bay.
	static void DrawBaysAndFacing(FPrimitiveDrawInterface* PDI, TFunctionRef<FVector(double, double)> At,
		double Span, double Across, const TArray<double>& Bounds, int32 DoorBay = INDEX_NONE);
	// The same for a footprint in the drag frame, facade on Side.
	void DrawRectBaysAndFacing(FPrimitiveDrawInterface* PDI, EHutongBaySide Side, double MinX, double MinY,
		double MaxX, double MaxY, const TArray<double>& Bounds, int32 DoorBay = INDEX_NONE) const;

	static void DrawPreviewLine(FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B,
		const FLinearColor& Color, float Thickness);

	// Dashed, for off-ground geometry. DashLength in world cm.
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

	// Property cache key; a preset picker's is per tool.
	FString CacheIdentifierFor(const UInteractiveToolPropertySet* PropertySet) const;

	// Loads Name into the picker if nothing is picked. Call after both picker and params are
	// registered: the params restore overwrites a preset loaded earlier.
	void ApplyDefaultPreset(class UHutongPresetProperties* Picker, const FString& Name);

	// Snaps a ground hit, recording the target for Render's marker and the anchor's angle.
	FVector ApplySnap(const FVector& World, bool bIsAnchor);

	// Live snap feedback, reset every hover.
	bool bSnapActive = false;
	FVector SnapPoint = FVector::ZeroVector;
	// Cursor end's last snap, held while near: an edge as its line, a corner as its point,
	// so an end slides along a face instead of dropping off or hopping to a corner.
	HutongSnap::FResult StickyCursorSnap;
	bool bStickyCursorValid = false;
	// Drops both ends' bearings so no miter outlives its placement.
	void ClearSnapBearings();

	// Yaw of the anchor's snap target; -1000 for none.
	double AnchorSnapYawDeg = -1000.0;
	// Same for the far end, updated every hover.
	double CursorSnapYawDeg = -1000.0;
	// Second edge at a snapped corner, per end; and the cursor end's inward direction.
	double AnchorSnapYaw2Deg = -1000.0;
	double CursorSnapYaw2Deg = -1000.0;
	FVector2D CursorSnapInward = FVector2D::ZeroVector;
	// Building the cursor end snapped to.
	TWeakObjectPtr<class UHutongBuildingComponent> CursorSnapBuilding;

	// Miter extension filling the corner with a neighbour at NeighbourYawDeg; zero if unjoined.
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

	// Stage machine behind OnClicked; the readout refreshes once per click whichever stage handled it.
	void ProcessClick(const FVector& Hit);

	bool TryRayHitGround(const FInputDeviceRay& Ray, FVector& OutHit) const;
	FInputRayHit GroundRayHit(const FInputDeviceRay& Ray) const;
	// Virtual for tools that place more than one actor.
	virtual void SpawnFinalActor();
	// After the component is attached: adopts the snapped neighbour's 下鹼 line, if both have one.
	void AdoptNeighbourBaseCourse(AStaticMeshActor* Actor);
	void AdoptBaseCourseFrom(AStaticMeshActor* Actor, const class UHutongBuildingComponent* Neighbour);


	// Writes the rect into the Placement set; notifies the details panel only on a real change.
	void UpdatePlacementReadout();
};
