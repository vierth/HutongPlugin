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
	UPROPERTY(EditAnywhere, Category="Appearance", meta=(ShowOnlyInnerProperties, ToolTip="Colours and materials for each surface of new placements."))
	FHutongPalette Palette;
};

UCLASS()
class UHutongDetailProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Detail", meta=(DisplayName="Detail Level", ToolTip="How much detail new placements are built with."))
	EHutongDetail Level = EHutongDetail::Near;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Detail", meta=(DisplayName="Bespoke Mesh", ToolTip="Marks new placements as carrying a bespoke mesh rather than a shared one."))
	bool bBespokeMesh = false;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Detail", meta=(DisplayName="Build LOD Chain", ToolTip="Also bakes every cheaper detail level as an LOD of each new placement."))
	bool bBuildLODChain = true;
};

// **What the next placement is worth as evidence.** Carried on the tool as well as on the
// building because a block is traced in one sitting off one sheet: the confidence and the note
// belong to the run of polygons being drawn, not to each of them separately. Both are stamped
// onto every placement and stay editable on it afterwards.
UCLASS()
class UHutongMetadataProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Metadata", meta=(DisplayName="Confidence", ToolTip="How far new placements are attested on the map and how far inferred; 5 is drawn on the map, 1 is not present in any source."))
	EHutongConfidence Confidence = EHutongConfidence::Attested;

	UPROPERTY(EditAnywhere, Category="Metadata", meta=(DisplayName="Notes", MultiLine=true, ToolTip="Free text stamped onto each new placement: what it was read from, what is uncertain about it."))
	FString Notes;
};

UCLASS()
class UHutongSnapProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	// **On by default: a run laid beside another is meant to meet it**, and a few centimetres of
	// daylight between two footprints is invisible until something is walked down. What is
	// irritating is not position snapping but the *rotation* changing under the cursor, and that is
	// its own switch below. The key suppresses this one placement by placement.
	UPROPERTY(EditAnywhere, Category="Snapping", meta=(DisplayName="Snap To Placed Buildings", ToolTip="Snaps placements to the footprints of buildings already placed. On by default; the snap key suppresses it one placement at a time, and turning this off inverts what that key does."))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category="Snapping", meta=(DisplayName="Snap Radius", EditCondition="bEnabled", UIMin="10", UIMax="200", ClampMin="1", Units="cm", ToolTip="Distance within which the cursor snaps to a placed footprint, in world cm."))
	double Radius = 55.0;

	// **Off by default, and the one part of snapping that is.** It is both halves of turning a
	// placement: the anchor taking the bearing of the building it snapped to, and hold-R pulling
	// onto a neighbour's line or a quarter turn off it. A position that jumps to a corner is the
	// corner you aimed at; a yaw that jumps is the building turning itself while you watch.
	UPROPERTY(EditAnywhere, Category="Snapping", meta=(DisplayName="Adopt Neighbour Angle", EditCondition="bEnabled", ToolTip="Turns the placement onto a neighbour's line: when the anchor snaps to one, and while holding R to rotate. Off by default; position snapping is unaffected."))
	bool bAdoptAngle = false;

	UPROPERTY(EditAnywhere, Category="Snapping", meta=(DisplayName="Snap Lane To Street Module (胡同/小街/大街)", EditCondition="bEnabled", ToolTip="Pulls the run onto a standard street width — lane (胡同), minor street (小街) or avenue (大街) — when it is already close to one."))
	bool bSnapLaneWidth = true;

	UPROPERTY(EditAnywhere, Category="Snapping", meta=(DisplayName="Lane Snap Tolerance", EditCondition="bEnabled && bSnapLaneWidth", UIMin="5", UIMax="100", ClampMin="1", Units="cm", ToolTip="How far off a standard lane width the run may be and still snap to it, in cm."))
	double LaneToleranceCm = 30.0;

	UPROPERTY(EditAnywhere, Category="Snapping", meta=(DisplayName="Angle Tolerance", EditCondition="bEnabled && bAdoptAngle", UIMin="1", UIMax="25", ClampMin="0", Units="deg", ToolTip="How close a rotation must come to a neighbour's line, or a quarter turn off it, to snap, in degrees."))
	double AngleToleranceDeg = 7.0;
};

UCLASS()
class UHutongPlacementProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, Category="Placement", meta=(Units="cm", ToolTip="Width of the rectangle being placed, in cm."))
	double Width = 0.0;

	UPROPERTY(VisibleAnywhere, Category="Placement", meta=(Units="cm", ToolTip="Depth of the rectangle being placed, in cm."))
	double Depth = 0.0;

	UPROPERTY(VisibleAnywhere, Category="Placement", meta=(Units="cm", ToolTip="Height the placement is previewed at, in cm."))
	double Height = 0.0;

	UPROPERTY(VisibleAnywhere, Category="Placement", meta=(Units="deg", ToolTip="Yaw of the rectangle being placed, in degrees."))
	double Rotation = 0.0;

	UPROPERTY(VisibleAnywhere, Category="Placement", meta=(ToolTip="Extra readout for the current tool, such as the bay count."))
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
	// Hold G on the click that ends a segment: that segment carries the run's opening.
	void SetOpeningKeyHeld(bool bHeld) { bOpeningKeyHeld = bHeld; }
	bool bOpeningKeyHeld = false;
	bool IsPlacingActive() const { return bIsDragging; }
	// A handle drag on a laid-out building in hand, which no tool switch may interrupt.
	bool IsEditingPlan() const { return PlanEdit != EPlanEdit::None; }

	// [ and ] during placement.
	virtual void AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse) {}

	// - and = adjust the tool's primary height by DeltaCm.
	virtual void AdjustHeight(double DeltaCm) {}

	// The height the corner posts preview, which is the same value AdjustHeight moves.
	virtual double GetPreviewHeight() const { return 0.0; }

	// How far this run's own face lies from the line the cursor is drawing.
	virtual double GetLaneFaceOffset() const { return 0.0; }

	// Whether this placement is forming a street at all. A run inside a compound is not, and
	// pulling its anchor onto a canonical lane width moves it for a reason that does not apply.
	virtual bool WantsLaneWidthSnap() const { return true; }

	// One-line instruction for the current placement stage.
	virtual FText GetStagePromptText() const;

	// The help lines shown in the mode panel, in order.
	virtual TArray<FText> GetToolHelpLines() const;

	// The one line of keys that stays on screen; the help lines above live behind a collapsed header.
	virtual FText GetKeyHintText() const;

	// [ and ] with nothing being placed.
	bool TurnSelectedFacing(int32 Delta);

	// The clicks a placement takes, named, and which one the tool is waiting for.
	virtual TArray<FText> GetStageNames() const;
	virtual int32 GetStageIndex() const;

	// Where the placement readout is drawn: a fixed corner of the viewport.
	static FVector2D HudCorner(FCanvas* Canvas);

	// Holds a block of HUD text inside the viewport, for the readouts that follow something.
	static FVector2D ClampToViewport(FCanvas* Canvas, const FVector2D& At, const FVector2D& BlockSize);
	// The viewport's width in the DPI-independent units the HUD is drawn in.
	static double ViewportWidth(FCanvas* Canvas);
	// Text broken onto lines no wider than MaxWidth, on spaces where it can, mid-word where it must.
	static TArray<FString> WrapToWidth(const FString& Text, const UFont* Font, double MaxWidth);

	// "820 x 540 cm · 37° · 3 bays @ 340 cm" — the HUD line under the stage prompt.
	FText GetPlacementSummaryText() const;

	// What the cursor is resting on, for the mode panel to poll.
	FText GetHoverSummaryText() const;

protected:
	// What the cursor is resting on while nothing is being placed, and where on screen it is.
	TWeakObjectPtr<class UHutongBuildingComponent> HoveredBuilding;

	// Where the ray struck it, in the world.
	FVector HoveredWorldPoint = FVector::ZeroVector;

	// What the outline was actually drawn for this frame, latched by Render for DrawHUD to read.
	mutable TWeakObjectPtr<class UHutongBuildingComponent> HudBuilding;
	mutable FVector HudWorldPoint = FVector::ZeroVector;

	// Which way is "into the neighbour" at the anchor, from the snap it landed on, or zero when the anchor did not snap.
	FVector2D AnchorSnapInward = FVector2D::ZeroVector;
	// The building the anchor snapped to, for a placement that sizes itself to what it joins. Empty when the anchor did not snap, and dropped with the bearings.
	TWeakObjectPtr<class UHutongBuildingComponent> AnchorSnapBuilding;

	// ---- Editing a laid-out building in place ----
	// Skew: one corner dragged off the rectangle, Shift held on a corner handle.
	// RunVertex: the end of a wall leg dragged, the vertex of the run it belongs to moving with
	// every leg that meets there, so the joins stay melded and a snapped end slides along a face.
	// Marquee: a press on open ground dragged out into a box; what it covers is selected on release.
	// Divide: a bay-line marker pressed; released unmoved, the building is divided there.
	// Fuse: the marker on an end shared with a like neighbour; released unmoved, the two fuse.
	enum class EPlanEdit : uint8 { None, Move, Resize, Rotate, Opening, Skew, RunVertex, Marquee, Divide, Fuse };
	FVector MarqueeStart = FVector::ZeroVector;
	FVector MarqueeEnd = FVector::ZeroVector;
	EPlanEdit PlanEdit = EPlanEdit::None;
	TWeakObjectPtr<class UHutongBuildingComponent> EditedPlan;
	int32 EditHandle = INDEX_NONE;
	FTransform EditStartTransform;
	FVector2D EditStartSize = FVector2D::ZeroVector;
	FHutongFootprintSkew EditStartSkew;
	// Under Ends a corner goes one way per drag, along the run or across it, chosen by which way
	// the cursor first pulls: 0 local X, 1 local Y, INDEX_NONE until it has pulled far enough to say.
	int32 EditSkewAxis = INDEX_NONE;
	bool bSkewAxisHeld = false;
	FVector EditGrabLocal = FVector::ZeroVector;
	// The run under a vertex drag: its legs as they stood, and which vertex moves.
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
	// The wall legs the level holds, for gathering a run.
	TArray<class UHutongWallBuildingComponent*> GatherWallLegs() const;
	double EditStartCursorAngleDeg = 0.0;
	double EditStartYawDeg = 0.0;
	double EditStartOpeningCentre = 0.0;
	bool bPlanDragMoved = false;
	// The press's own pixel. A capture delivers drag events for a cursor that has not moved, so
	// without a slop a plain selection click committed a no-op edit and left an empty undo entry.
	FVector2D PlanPressPixel = FVector2D::ZeroVector;
	bool bPlanPressHasPixel = false;
	// What the cursor is over on the selected building, refreshed each frame for the drawing.
	int32 HoverPlanHandle = INDEX_NONE;

	// Handle ids: 0..3 corners from the origin anticlockwise, 4..7 edge midpoints (−Y, +X, +Y, −X), 8 the rotate ring, 9 the inside of the footprint.
	static constexpr int32 PlanHandleRing = 8;
	static constexpr int32 PlanHandleInside = 9;
	// 10 and up: the sliders on a wall's openings, one per opening in GetPlanOpenings order.
	static constexpr int32 PlanHandleOpening = 10;
	// The fuse markers on the origin end and the far end, offered only where a like neighbour stands on the line.
	static constexpr int32 PlanHandleFuseStart = 1000;
	static constexpr int32 PlanHandleFuseEnd = 1001;
	// 1010 and up: the divide markers on the interior bay lines, in GetPlanBays order from 1.
	static constexpr int32 PlanHandleBay = 1010;

	// The like building standing end to end with this one on its line at that end, if any.
	class UHutongBuildingComponent* FindFuseNeighbour(const class UHutongBuildingComponent* Building, bool bAtEnd) const;
	// World position of a divide marker (bay line) or a fuse marker (an end), on the footprint's centre line.
	FVector PlanBayMarkerWorld(const class UHutongBuildingComponent* Building, double Along) const;
	// The click that divides or fuses, once the press on its marker has been released unmoved.
	void DivideAtMarker(class UHutongBuildingComponent* Building, int32 BayLine);
	void FuseAtMarker(class UHutongBuildingComponent* Building, bool bAtEnd);

	// The one selected building, of any kind; and the same only when it is laid out and not built.
	class UHutongBuildingComponent* GetSelectedBuilding() const;
	class UHutongBuildingComponent* GetSelectedPlanBuilding() const;
	FVector PlanOpeningWorld(const class UHutongBuildingComponent* Building, int32 Index, double& OutWidth) const;
	// Corners 0..3 are the placement's own, offsets included; 4..7 the midpoints of the edges between them.
	static FVector PlanHandleLocal(const FVector2D Corners[4], int32 Handle);
	// Shift on a corner handle angles the footprint instead of resizing it; read live, as rotate reads it.
	static bool IsSkewKeyDown();
	// The drag behaviour's registered Shift binding; the state itself is read live.
	static constexpr int32 SkewModifierID = 1;
	// Handles are sized in world units off the footprint.
	static double PlanHandleSize(const FVector2D& Size);
	// The corner squares under Shift: on a run they must fit inside its thickness, or the two at one end are one blob.
	static double PlanCornerHandleSize(const FVector2D& Size);
	// A wall, a path, a corridor: a footprint whose short side is a fraction of its long one.
	static bool IsLineLikePlan(const FVector2D& Size);
	static bool PlanHandleEnabled(const FVector2D& Size, int32 Handle);
	// Whether a ground point inside a footprint counts as a grab (select / move).
	bool IsPlanGrabPoint(const FVector2D& Size, double EdgeDistance, const FVector& Ground) const;
	FVector PlanRotateHandleWorld(const class UHutongBuildingComponent* Building, FVector& OutEdgeMid) const;
	// Which handle, ring or inside a ground point is on for this building; INDEX_NONE for none.
	int32 HitTestPlan(const class UHutongBuildingComponent* Building, const FVector& Ground) const;
	void BeginPlanEdit(class UHutongBuildingComponent* Building, int32 Hit, const FVector& Ground);
	void UpdatePlanEdit(const FVector& Ground);
	void CommitPlanEdit();
	void CancelPlanEdit();
	void DrawPlanHandles(FPrimitiveDrawInterface* PDI) const;
	// The prompt while a laid-out building is selected or being edited; empty otherwise.
	FText GetPlanEditPromptText() const;


	// What the cursor is resting on, from the editor's own cursor, run once a frame out of Render.
	void UpdateHoverInspectionFromViewport();
	void DrawHoverInspection(FPrimitiveDrawInterface* PDI) const;
	void DrawHoverInspectionHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) const;

	// Does this ray have something under it worth hovering for?
	bool TraceHoveredBuilding(const FInputDeviceRay& Ray, FHitResult& OutHit) const;

	// A laid-out building has no collision.
	class UHutongBuildingComponent* FindPlanBuildingAt(const FVector& Ground, double* OutEdgeDistance = nullptr) const;

	// World centimetres per screen pixel at a point, for the active viewport.
	double WorldPerPixelAt(const FVector& P) const;

	// **The one answer to whether this placement snaps**, so the checkbox and the key are not read
	// separately at six sites that could disagree. The key does the *other* thing for the placement,
	// whichever way the checkbox stands: with snapping off it snaps, with snapping on it lets go.
	bool SnappingActive() const;

	// The key: Alt (Option on a Mac) or Command, **latched for the placement** once seen.
	// A click that begins with Alt down never reaches a tool — the editor's tools context hands it
	// to the camera so Alt-orbit keeps working — so the key cannot be what is held at the moment
	// of the click. It is read on the frames it can be, and holding it at any point during a
	// placement carries for the rest of that placement.
	bool IsSnapKeyLatched() const;

	// The keys themselves, without the latch: what the prompt is describing.
	static bool IsSnapKeyDown();

	// What the key is about to do, which follows the standing setting rather than being fixed.
	FText SnapKeyClause() const;

	// The placed buildings, gathered once rather than by walking the level per query. Everything
	// that asks about a neighbour — the corner snap, the lane readout, the angle snap and the
	// laid-out footprint under the cursor — reads this.
	const TArray<HutongSnap::FFootprint>& GetFootprints() const;

	// The hover readout runs out of Render, so it is asked every frame whether the mouse moved or
	// not, and answering costs a complex-collision trace. Held between frames on the cursor's own
	// pixel position: a still cursor is over what it was over last frame, and HoveredBuilding
	// still holds what that was.
	mutable FIntPoint LastHoverCursorPx = FIntPoint(-1, -1);
	mutable bool bHoverTraceValid = false;

	// Set the moment the key is seen down, cleared when nothing is being placed or edited.
	mutable bool bSnapKeyLatched = false;

	// The snap radius with its pixel floor applied at this point.
	double EffectiveSnapRadius(const FVector& P) const;
	static constexpr double SnapPixelFloor = 14.0;

	// Subclass contract: build mesh in local space (origin at rectangle min corner).
	virtual void BuildMeshForRect(double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Level) {}
	virtual FString GetActorNameBase() const { return TEXT("HutongActor"); }

	// Attach the UHutongBuildingComponent that keeps the placed actor editable, filled with the same parameters and footprint that produced the mesh.
	virtual void AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY) {}

	// What the next placement is built at, and what the placed component has to be told so a later rebuild does not silently promote it back to Near.
	EHutongDetail GetDetailLevel() const;

	// Whether the next placement bakes the levels below its own as LODs.
	bool ShouldBuildLODChain() const;

	// The chain for the current placement, built through BuildMeshForRect.
	void BuildLODsForRect(double SizeX, double SizeY, TArray<UE::Geometry::FDynamicMesh3>& OutLODs);

	// Copies both detail fields onto a freshly created component.
	void StampDetail(class UHutongBuildingComponent* Building) const;

	// The mode's "lay out only" setting.
	static bool IsPlanOnly();

	// Returns LOCAL-frame bounds (offsets from StartWorld in the placement-yaw frame).
	virtual void GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const;

	// Puts an extent the tool has fixed under the cursor instead of in the anchor's quadrant.
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

	// For a type with a facade: the side the camera is on before the rect is committed, and the
	// side nearest a ground point once it is — the same answer for every tool that asks.
	EHutongBaySide ComputeDefaultBaySide() const;
	EHutongBaySide ComputeClosestSide(double Hx, double Hy) const;

	// Rotated rect-local frame anchored at StartWorld; yaw rotation is around world Z.
	FVector2D WorldXYToLocalRect(const FVector& World) const;
	FVector LocalRectToWorld(double LocalX, double LocalY) const;
	// The same frame stood on another origin — for a preview drawn before there is an anchor.
	FVector LocalRectToWorldFrom(const FVector& Origin, double LocalX, double LocalY) const;

	// Drawn every frame while nothing is being placed, with the ground point under the cursor.
	virtual void RenderIdlePreview(FPrimitiveDrawInterface* PDI, const FVector& CursorGround) {}

	// The editor's own cursor, dropped onto the ground plane.
	bool GetViewportCursorGround(FVector& OutGround) const;

	void UpdateRotateFromCursor(const FVector& CursorWorld);

	// Draws a preview line twice: a wider near-black pass first, then the colored line on top.
	static void DrawPreviewLine(FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B,
		const FLinearColor& Color, float Thickness);

	// Dashed variant, for geometry that is not on the ground. Dash length is in world cm.
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

	// Whichever of the tool's own sets is the preset picker, noticed by RegisterSettings rather
	// than named by each of the fourteen tools: it is what says which of a type a placement is,
	// and StampDetail is the one place that has to read it.
	UPROPERTY()
	TObjectPtr<class UHutongPresetProperties> PresetSettings;

	// Applies the snap to a ground hit, and remembers what it landed on so Render can mark it and the anchor can adopt its angle.
	FVector ApplySnap(const FVector& World, bool bIsAnchor);

	// Live snap feedback, reset every hover.
	bool bSnapActive = false;
	FVector SnapPoint = FVector::ZeroVector;
	// What the cursor end snapped to last, kept while the cursor stays near it: an end resting on
	// a face slides along the face and does not drop off it, or jump to a corner, with a pixel's
	// movement. An edge is held as the line through the point; a corner as the point.
	HutongSnap::FResult StickyCursorSnap;
	bool bStickyCursorValid = false;
	// Both bearings dropped, so nothing a miter is built from outlives the placement that found it.
	void ClearSnapBearings();

	// Yaw of whatever the anchor snapped to, or a large negative for nothing.
	double AnchorSnapYawDeg = -1000.0;
	// The same for the far end, updated on every hover so it is current when the click lands.
	double CursorSnapYawDeg = -1000.0;
	// The other edge at a snapped corner, for each end; and the cursor end's inward direction.
	double AnchorSnapYaw2Deg = -1000.0;
	double CursorSnapYaw2Deg = -1000.0;
	FVector2D CursorSnapInward = FVector2D::ZeroVector;
	// The building the cursor end snapped to, for the same reason the anchor's is kept.
	TWeakObjectPtr<class UHutongBuildingComponent> CursorSnapBuilding;

	// The miter a run of the given thickness needs to fill its corner with a neighbour at NeighbourYawDeg, or zero if that end joined nothing.
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

	// The stage machine behind OnClicked, split out so the readout refresh happens once for every click regardless of which stage handled it.
	void ProcessClick(const FVector& Hit);

	bool TryRayHitGround(const FInputDeviceRay& Ray, FVector& OutHit) const;
	FInputRayHit GroundRayHit(const FInputDeviceRay& Ray) const;
	// Virtual so a tool that places more than one thing can take the whole step over.
	virtual void SpawnFinalActor();
	// After the component is on: the 下鹼 line of whatever the placement snapped to, when both have one.
	void AdoptNeighbourBaseCourse(AStaticMeshActor* Actor);
	void AdoptBaseCourseFrom(AStaticMeshActor* Actor, const class UHutongBuildingComponent* Neighbour);


	// Pushes the current rect into the Placement set, notifying the details panel only when a value actually changed.
	void UpdatePlacementReadout();
};
