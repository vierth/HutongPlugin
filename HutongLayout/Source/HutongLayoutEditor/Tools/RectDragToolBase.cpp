#include "Tools/RectDragToolBase.h"
#include "Engine/StaticMeshActor.h"
#include "Tools/HutongWallRun.h"
#include "HutongLayoutEdMode.h"
#include "HutongLayoutModeSettings.h"
#include "SEditorViewport.h"
#include "LevelEditorViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Tools/HutongSnap.h"
#include "Tools/HutongPresets.h"
#include "Tools/HutongDetailOps.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Generation/HutongUrban.h"
#include "BaseBehaviors/SingleClickBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "InteractiveToolManager.h"
#include "ToolContextInterfaces.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "SceneManagement.h"
#include "Framework/Application/SlateApplication.h"
#include "Generation/HutongActorSpawn.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Misc/ScopedSlowTask.h"
#include "SceneView.h"
#include "Engine/Engine.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongPlanOutlineComponent.h"
#include "Generation/BaySide.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/Actor.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "UnrealWidget.h"

#define LOCTEXT_NAMESPACE "HutongRectDragTool"

using UE::Geometry::FDynamicMesh3;

namespace
{
	// How far the cursor leaves the press's pixel before a plan edit is a drag and not a click.
	constexpr double PlanClickSlopPixels = 4.0;
	// A box on open ground asks for a real drag: a trackpad click wanders a few pixels, and at
	// the handle slop every such click became an empty box instead of the placement it was.
	constexpr double MarqueeSlopPixels = 12.0;

	// **An orthographic cursor ray does not start at a camera.** It starts on the view plane, and
	// in the top-down view a map is traced in that plane is the ground — so the origin arrives at
	// Z = 0 with the whole scene behind it. The origin is arbitrary along such a ray, so it is
	// pulled back until everything is in front of it. Ten kilometres: past any district, and well
	// inside the trace length below.
	constexpr double OrthoRayBackOff = 1000000.0;

	bool IsActiveViewportOrtho()
	{
		FViewport* Viewport = GEditor ? GEditor->GetActiveViewport() : nullptr;
		const FEditorViewportClient* Client = Viewport
			? static_cast<const FEditorViewportClient*>(Viewport->GetClient()) : nullptr;
		return Client && Client->IsOrtho();
	}
}

// The name of the key that ignores snapping, for the prompts.
FText SnapKeyName();

void URectDragToolBase::Setup()
{
	UInteractiveTool::Setup();

	ClickBehavior = NewObject<USingleClickInputBehavior>(this);
	ClickBehavior->Initialize(this);
	AddInputBehavior(ClickBehavior);

	// Ahead of the click: a press over a laid-out building is this behaviour's, and it declines everything else so the click still places.
	DragBehavior = NewObject<UClickDragInputBehavior>(this);
	DragBehavior->Initialize(this);
	DragBehavior->SetDefaultPriority(FInputCapturePriority(FInputCapturePriority::DEFAULT_TOOL_PRIORITY - 1));
	// Shift is this behaviour's own binding, for angling a corner. Declared here rather than only
	// read at the press: the editor's click-selection behaviour lists Shift as an optional binding
	// and is promoted a step for it when held, past a plain tool-priority drag — so the press went
	// to selection and the corner never moved. A behaviour with a registered modifier is promoted
	// the same step when that modifier is down, and this one only ever asks for a press over a
	// handle of the selected building.
	DragBehavior->Modifiers.RegisterModifier(SkewModifierID, FInputDeviceState::IsShiftKeyDown);
	AddInputBehavior(DragBehavior);

	HoverBehavior = NewObject<UMouseHoverBehavior>(this);
	HoverBehavior->Initialize(this);
	AddInputBehavior(HoverBehavior);

	// Panel order is registration order.

	// First: the confidence and the note belong to the sitting, not to the placement, and they are
	// what a tracer sets before drawing anything.
	MetadataSettings = NewObject<UHutongMetadataProperties>(this);
	RegisterSettings(MetadataSettings);

	// Then the tool's own sets, the preset picker first among them: what is being placed comes
	// right under who is placing it, before the readout of where.
	RegisterToolSettings();

	Placement = NewObject<UHutongPlacementProperties>(this);
	RegisterSettings(Placement, false);

	DetailSettings = NewObject<UHutongDetailProperties>(this);
	RegisterSettings(DetailSettings);

	Snap = NewObject<UHutongSnapProperties>(this);
	RegisterSettings(Snap);

	Appearance = NewObject<UHutongAppearanceProperties>(this);
	RegisterSettings(Appearance);
}

EHutongDetail URectDragToolBase::GetDetailLevel() const
{
	return DetailSettings ? DetailSettings->Level : EHutongDetail::Near;
}

bool URectDragToolBase::ShouldBuildLODChain() const
{
	return DetailSettings ? DetailSettings->bBuildLODChain : true;
}

void URectDragToolBase::BuildLODsForRect(double SizeX, double SizeY, TArray<FDynamicMesh3>& OutLODs)
{
	HutongGen::Detail::BuildPlacementLODs(IsPlanOnly(), SizeX, SizeY,
		GetDetailLevel(), ShouldBuildLODChain(),
		[this, SizeX, SizeY](FDynamicMesh3& Mesh, EHutongDetail Level)
		{
			BuildMeshForRect(SizeX, SizeY, Mesh, Level);
		},
		OutLODs);
}

bool URectDragToolBase::IsPlanOnly()
{
	const UHutongLayoutModeSettings* Settings = UHutongLayoutEdMode::GetActiveSettings();
	return Settings && Settings->bPlanOnly;
}

void URectDragToolBase::StampDetail(UHutongBuildingComponent* Building) const
{
	if (!Building) return;
	if (DetailSettings)
	{
		Building->DetailLevel = DetailSettings->Level;
		Building->bBespokeMesh = DetailSettings->bBespokeMesh;
		Building->bBuildLODChain = DetailSettings->bBuildLODChain;
	}
	if (MetadataSettings)
	{
		Building->Confidence = MetadataSettings->Confidence;
		Building->Notes = MetadataSettings->Notes;
	}
	Building->bPlanOnly = IsPlanOnly();
	// **Which preset laid a building down is a fact about the placement, and nothing was recording
	// it.** A 正房 and a 耳房 are one generator, so a building that does not carry the name is one
	// whose type nothing can answer for afterwards — the params are on it, but which of the shipped
	// buildings they came from is not. The picker's own selection is what the house tool's prompt
	// already calls the loaded preset, so it is the same answer said in one more place. Empty when
	// no preset was picked, which is the truth rather than a gap.
	//
	// **Only when the picker is picking this kind of building.** The compound and the gallery stamp
	// every piece they lay out through here, and their own picker names a compound — writing that
	// onto a 正房 would put a confident wrong answer where a blank one is honest. The two keys come
	// from opposite ends (the tool's save key, the component's class name) and agreeing is what
	// says the name means this building.
	if (PresetSettings && PresetSettings->GetToolKey() == Building->GetPresetKey())
	{
		Building->Preset = PresetSettings->Preset;
	}
}

void URectDragToolBase::RegisterSettings(UInteractiveToolPropertySet* PropertySet, bool bPersist)
{
	if (!PropertySet) return;
	if (bPersist)
	{
		PropertySet->RestoreProperties(this);
		RegisteredSettings.Add(PropertySet);
	}
	// Noticed here rather than assigned by each tool: every tool that has a preset picker registers
	// it through this one call, and a fifteenth tool costs nothing.
	if (UHutongPresetProperties* AsPresets = Cast<UHutongPresetProperties>(PropertySet))
	{
		PresetSettings = AsPresets;
	}
	AddToolPropertySource(PropertySet);
}

void URectDragToolBase::UpdatePlacementReadout()
{
	if (!Placement) return;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);

	const double NewWidth = bIsDragging ? MaxX - MinX : 0.0;
	const double NewDepth = bIsDragging ? MaxY - MinY : 0.0;
	const double NewRotation = bIsDragging ? PlacementYawDeg : 0.0;
	const FString NewDetail = bIsDragging ? GetPlacementDetail() : FString();
	// Height is a tool setting rather than part of the drag, so it stays shown between placements.
	const double NewHeight = GetPreviewHeight();

	if (FMath::IsNearlyEqual(NewWidth, Placement->Width)
		&& FMath::IsNearlyEqual(NewDepth, Placement->Depth)
		&& FMath::IsNearlyEqual(NewHeight, Placement->Height)
		&& FMath::IsNearlyEqual(NewRotation, Placement->Rotation)
		&& NewDetail == Placement->Detail)
	{
		return;
	}

	// No NotifyOfPropertyChangeByTool here: that rebuilds the whole details panel, and this runs on every hover tick.
	Placement->Width = NewWidth;
	Placement->Depth = NewDepth;
	Placement->Height = NewHeight;
	Placement->Rotation = NewRotation;
	Placement->Detail = NewDetail;
}

void URectDragToolBase::Shutdown(EToolShutdownType ShutdownType)
{
	for (const TObjectPtr<UInteractiveToolPropertySet>& Set : RegisteredSettings)
	{
		if (Set) Set->SaveProperties(this);
	}
	UInteractiveTool::Shutdown(ShutdownType);
}

void URectDragToolBase::GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	const FVector2D Local = WorldXYToLocalRect(CurrentWorld);
	OutMinX = FMath::Min(0.0, Local.X);
	OutMinY = FMath::Min(0.0, Local.Y);
	OutMaxX = FMath::Max(0.0, Local.X);
	OutMaxY = FMath::Max(0.0, Local.Y);
}

void URectDragToolBase::HoldExtentAtCursor(double& Lo, double& Hi, double Size)
{
	// Whichever end the drag reached is where the cursor is; the other is the anchor at zero.
	const double Cursor = (Hi > 0.0) ? Hi : Lo;
	Lo = Cursor - 0.5 * Size;
	Hi = Cursor + 0.5 * Size;
}

FVector2D URectDragToolBase::WorldXYToLocalRect(const FVector& World) const
{
	const FRotator Rot(0.0, PlacementYawDeg, 0.0);
	const FVector Off = Rot.UnrotateVector(World - StartWorld);
	return FVector2D(Off.X, Off.Y);
}

FVector URectDragToolBase::LocalRectToWorld(double LocalX, double LocalY) const
{
	return LocalRectToWorldFrom(StartWorld, LocalX, LocalY);
}

FVector URectDragToolBase::LocalRectToWorldFrom(const FVector& Origin, double LocalX, double LocalY) const
{
	const FRotator Rot(0.0, PlacementYawDeg, 0.0);
	const FVector Off = Rot.RotateVector(FVector(LocalX, LocalY, 0.0));
	return FVector(Origin.X + Off.X, Origin.Y + Off.Y, Origin.Z);
}

bool URectDragToolBase::GetViewportCursorGround(FVector& OutGround) const
{
	if (GEditor == nullptr) return false;
	FViewport* Viewport = GEditor->GetActiveViewport();
	FEditorViewportClient* Client = Viewport
		? static_cast<FEditorViewportClient*>(Viewport->GetClient()) : nullptr;
	if (Client == nullptr) return false;

	// Only while the mouse is actually over the viewport.
	const TSharedPtr<SEditorViewport> Widget = Client->GetEditorViewportWidget();
	if (!Widget.IsValid() || !FSlateApplication::IsInitialized()) return false;
	if (!Widget->GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos())) return false;

	const FViewportCursorLocation Cursor = Client->GetCursorWorldLocationFromMousePos();
	FInputDeviceRay Ray;
	Ray.WorldRay = FRay((FVector3d)Cursor.GetOrigin(), (FVector3d)Cursor.GetDirection(), true);
	return TryRayHitGround(Ray, OutGround);
}

void URectDragToolBase::Render(IToolsContextRenderAPI* RenderAPI)
{
	if (RenderAPI == nullptr) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;

	if (!bIsDragging)
	{
		// Idle frames with the key up end the last placement's suppression; a placement or a plan
		// edit in flight keeps whatever it has, which is what lets the key be released to click.
		if (!IsEditingPlan() && !IsSnapKeyDown()) bSnapKeyLatched = false;
		UpdateHoverInspectionFromViewport();
		DrawHoverInspection(PDI);
		FVector Ground;
		const bool bGround = GetViewportCursorGround(Ground);
		// What the cursor is over, for the handle under it to light up before it is pressed.
		HoverPlanHandle = INDEX_NONE;
		if (bGround && !IsEditingPlan())
		{
			if (const UHutongBuildingComponent* Selected = GetSelectedBuilding())
			{
				HoverPlanHandle = HitTestPlan(Selected, Ground);
			}
		}
		DrawPlanHandles(PDI);
		if (PlanEdit == EPlanEdit::Marquee && bPlanDragMoved)
		{
			// The box on the ground, in the selection's own blue.
			const FLinearColor BoxColor(0.35f, 0.75f, 1.0f, 1.0f);
			const FVector Lift(0.0, 0.0, 3.0);
			const FVector A = MarqueeStart + Lift, C = MarqueeEnd + Lift;
			const FVector B(C.X, A.Y, A.Z), D(A.X, C.Y, A.Z);
			DrawDashedPreviewLine(PDI, A, B, BoxColor, 2.5f);
			DrawDashedPreviewLine(PDI, B, C, BoxColor, 2.5f);
			DrawDashedPreviewLine(PDI, C, D, BoxColor, 2.5f);
			DrawDashedPreviewLine(PDI, D, A, BoxColor, 2.5f);
		}
		if (bGround && !IsEditingPlan())
		{
			RenderIdlePreview(PDI, Ground);
		}
		return;
	}

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);

	const FVector C0 = LocalRectToWorld(MinX, MinY);
	const FVector C1 = LocalRectToWorld(MaxX, MinY);
	const FVector C2 = LocalRectToWorld(MaxX, MaxY);
	const FVector C3 = LocalRectToWorld(MinX, MaxY);

	const FLinearColor Color(1.0f, 0.9f, 0.15f, 1.0f);
	const float Thickness = 5.0f;
	DrawPreviewLine(PDI, C0, C1, Color, Thickness);
	DrawPreviewLine(PDI, C1, C2, Color, Thickness);
	DrawPreviewLine(PDI, C2, C3, Color, Thickness);
	DrawPreviewLine(PDI, C3, C0, Color, Thickness);

	// The snap marker: a small cross on the point the placement has locked onto.
	if (bSnapActive)
	{
		const FLinearColor SnapColor(0.25f, 1.0f, 0.45f, 1.0f);
		const double Arm = 26.0;
		DrawPreviewLine(PDI, SnapPoint - FVector(Arm, 0, 0), SnapPoint + FVector(Arm, 0, 0), SnapColor, 4.0f);
		DrawPreviewLine(PDI, SnapPoint - FVector(0, Arm, 0), SnapPoint + FVector(0, Arm, 0), SnapColor, 4.0f);
		DrawPreviewLine(PDI, SnapPoint, SnapPoint + FVector(0, 0, 1.6 * Arm), SnapColor, 3.0f);
	}

	// Corner posts and a top outline, so the height being edited is visible in perspective.
	const double PreviewHeight = GetPreviewHeight();
	if (PreviewHeight <= 0.0) return;

	const FVector Up(0.0, 0.0, PreviewHeight);
	const FLinearColor HeightColor(0.45f, 0.8f, 1.0f, 1.0f);

	const FVector Corners[4] = { C0, C1, C2, C3 };
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector& A = Corners[i];
		const FVector& B = Corners[(i + 1) % 4];
		DrawDashedPreviewLine(PDI, A, A + Up, HeightColor, 2.5f);
		DrawDashedPreviewLine(PDI, A + Up, B + Up, HeightColor, 3.0f);
	}
}

TArray<FText> URectDragToolBase::GetToolHelpLines() const
{
	return {
		LOCTEXT("HelpAnchor", "Click the ground to anchor a corner, move, then click to set the footprint."),
		LOCTEXT("HelpHeight", "- and = raise or lower the height in 20 cm steps (Shift 5 cm, Ctrl 100 cm). Corner posts preview it."),
		LOCTEXT("HelpRotate", "Hold R while placing to rotate around the anchor; hold Shift for 5° steps. The angle is left alone otherwise — tick Adopt Neighbour Angle under Snapping to have rotation pull onto nearby buildings' bearings and quarter turns off them."),
		FText::Format(LOCTEXT("HelpSnap", "Placements snap to the corners and edges of buildings already placed, and a run lands on a standard lane width; the angle is not snapped unless Adopt Neighbour Angle is ticked. Tap {0} to place freely for one placement, or untick Snap To Placed Buildings under Snapping and the key then does the opposite. Tap the key first and release it before clicking: a press that starts with Option down belongs to the camera."), SnapKeyName()),
		LOCTEXT("HelpPlan", "Drag a box on open ground to select every building it touches, Shift to add to the selection. Click inside a laid-out footprint to select it and drag to move it; drag its handles to resize, the ring to rotate. With any building selected and nothing being placed, [ and ] turn its facade to the next side. Click on a footprint's edge to start a new footprint against it; Ctrl+click to place inside it."),
		LOCTEXT("HelpDetail", "Detail Level sets how much of the next placement is built: Block (塊) is a massing block, Far (遠) the generator with the ornament off, Near (近) the full building. Promote, rebuild, export and import what is already down under Placed Buildings at the foot of the panel."),
		LOCTEXT("HelpCancel", "Esc cancels an in-progress placement; Ctrl+Z undoes a placed actor."),
	};
}

FText URectDragToolBase::GetKeyHintText() const
{
	return FText::Format(LOCTEXT("KeyHint", "- = height · R rotate · {0} {1} · Esc cancel · Ctrl+Z undo"),
		SnapKeyName(),
		SnappingActive() ? LOCTEXT("KeyHintNoSnap", "no snap") : LOCTEXT("KeyHintSnap", "snap"));
}

TArray<FText> URectDragToolBase::GetStageNames() const
{
	return { LOCTEXT("StageAnchor", "Anchor"), LOCTEXT("StageSize", "Size") };
}

int32 URectDragToolBase::GetStageIndex() const
{
	if (!bIsDragging) return 0;
	return bRectCommitted ? 2 : 1;
}

FText URectDragToolBase::SnapKeyClause() const
{
	// The key does the other thing, so what it is about to do is not fixed — named in one place so
	// the prompts cannot describe it three different ways.
	return SnappingActive()
		? FText::Format(LOCTEXT("SnapKeyOff", " Snapping on; tap {0} to place freely."), SnapKeyName())
		: FText::Format(LOCTEXT("SnapKeyOn", " Snapping off; tap {0} to snap to neighbours."), SnapKeyName());
}

FText URectDragToolBase::GetPlanEditPromptText() const
{
	const FText Free = SnapKeyClause();
	switch (PlanEdit)
	{
	case EPlanEdit::Move:
		return FText::Format(LOCTEXT("PromptPlanMove", "Moving: release to drop.{0} Esc puts it back."), Free);
	case EPlanEdit::Resize:
		return FText::Format(LOCTEXT("PromptPlanResize", "Resizing: release to set. The far edge stays put.{0} Esc puts it back."), Free);
	case EPlanEdit::Rotate:
		// The key's clause is about position snapping, and nothing is being positioned here.
		return LOCTEXT("PromptPlanRotate", "Rotating about the centre: release to set. Shift snaps to 5°. Esc puts it back.");
	case EPlanEdit::Opening:
		return LOCTEXT("PromptPlanOpening", "Sliding the doorway along the wall: release to set. A built wall rebuilds on release. Esc puts it back.");
	case EPlanEdit::Marquee:
		return LOCTEXT("PromptPlanMarquee", "Selecting: release to select the buildings the box touches. Shift adds them to the selection.");
	case EPlanEdit::RunVertex:
		return FText::Format(LOCTEXT("PromptPlanRunVertex", "Moving the end of the wall: it slides along a neighbour's face when it snaps to one, and every leg meeting here follows and stays melded. Shift keeps the leg's bearing. Release to set, a built wall rebuilds on release.{0} Esc puts it back."), Free);
	case EPlanEdit::Skew:
	{
		const UHutongBuildingComponent* B = EditedPlan.Get();
		const bool bEnds = !B || B->FootprintSkew.Mode == EHutongSkewMode::Ends;
		return bEnds
			? FText::Format(LOCTEXT("PromptPlanSkewEnds", "Moving the corner (端斜): it goes along the run or across it, whichever way you first pull, one way per drag; the body stays put. Release to set, rebuilds on release.{0} Esc puts it back. Whole-footprint mode is in the Details panel under Corner Offsets."), Free)
			: FText::Format(LOCTEXT("PromptPlanSkewWhole", "Angling the corner (斜角): release to set. The other three corners stay put; a built building rebuilds on release.{0} Esc puts it back."), Free);
	}
	default:
		break;
	}
	if (HoverPlanHandle >= PlanHandleBay)
	{
		return LOCTEXT("PromptDivideMarker", "Click to divide the building at this bay line (分間): the bays before it stay, the rest become a second building on the same line, each with its own gable and door.");
	}
	if (HoverPlanHandle == PlanHandleFuseStart || HoverPlanHandle == PlanHandleFuseEnd)
	{
		return LOCTEXT("PromptFuseMarker", "Click to fuse this building with the one standing end to end on this line (合併): one building, the bays summed, the wall between them gone.");
	}
	if (const UHutongBuildingComponent* Selected = GetSelectedBuilding())
	{
		TArray<FHutongPlanOpening> Openings;
		Selected->GetPlanOpenings(Openings);
		if (!Selected->bPlanOnly && Openings.Num() > 0)
		{
			return LOCTEXT("PromptWallSelected", "Wall selected: drag the doorway marker to slide it along the run; Shift-drag a corner to cut the end on the bias (斜角). [ and ] turn a facade. Click open ground to place.");
		}
		if (!Selected->bPlanOnly)
		{
			return LOCTEXT("PromptBuiltSelected", "Building selected: Shift-drag a corner to angle the footprint (斜角); it rebuilds on release. [ and ] turn the facade. Click open ground to place.");
		}
	}
	if (GetSelectedPlanBuilding())
	{
		return LOCTEXT("PromptPlanSelectedSkew", "Laid-out building selected: drag a handle to resize, Shift-drag a corner to angle it (斜角), the ring to rotate, the inside to move, a doorway marker to slide it; [ and ] turn the facade. Click open ground to place another.");
	}
	return FText::GetEmpty();
}

FText URectDragToolBase::GetStagePromptText() const
{
	if (!bIsDragging)
	{
		const FText Plan = GetPlanEditPromptText();
		if (!Plan.IsEmpty()) return Plan;
		return LOCTEXT("PromptAnchor", "Click the ground to anchor a corner of the footprint.");
	}
	if (bRotateModeActive)
	{
		return LOCTEXT("PromptRotate", "Rotating: move the mouse to swing the footprint, Shift snaps to 5°. Release R to keep the angle.");
	}
	if (!bRectCommitted)
	{
		return FText::Format(LOCTEXT("PromptSize", "Move to size the footprint, then click to place. - and = change height, hold R to rotate, Esc to cancel.{0}"), SnapKeyClause());
	}
	return LOCTEXT("PromptCommit", "Click to place. Esc to cancel.");
}

void URectDragToolBase::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	if (!bIsDragging)
	{
		DrawHoverInspectionHUD(Canvas, RenderAPI);
		return;
	}
	if (Canvas == nullptr || RenderAPI == nullptr) return;

	// **The readout sits in the viewport's top-left, not beside the rectangle.** Pinned to the
	// rect's far corner it lay over the thing being drawn at exactly the sizes where the numbers
	// matter, walked off the edge on a rect dragged toward one, and vanished outright whenever
	// WorldToPixel refused a corner behind the camera. A corner of the screen is always legible,
	// always in the same place, and never on top of the geometry. **Wrapped to the viewport**: a
	// prompt is a sentence or two, and on a narrow viewport it ran off the right edge unread.
	// The engine's bitmap font: a Slate font item draws nothing in this canvas pass, and this
	// block went missing entirely on it. The Chinese in the prompts is lost to it, as it always was.
	const FVector2D At = HudCorner(Canvas);
	const UFont* Font = UEngine::GetMediumFont();
	if (Font == nullptr) return;
	// Wrapped short of the measured width: the bitmap font draws a little wider than it measures.
	const double MaxWidth = FMath::Max(0.85 * (ViewportWidth(Canvas) - At.X - 24.0), 120.0);

	TArray<FString> Lines;
	TArray<FLinearColor> Colours;
	auto Add = [&](const FText& Text, const FLinearColor& Colour)
	{
		for (const FString& Line : WrapToWidth(Text.ToString(), Font, MaxWidth))
		{
			Lines.Add(Line);
			Colours.Add(Colour);
		}
	};
	Add(GetStagePromptText(), FLinearColor(1.0f, 0.9f, 0.15f));
	Add(GetPlacementSummaryText(), FLinearColor::White);
	// The keys this tool answers to, the same line the panel keeps on screen.
	Add(GetKeyHintText(), FLinearColor(0.78f, 0.78f, 0.78f));

	// A dark backing, so the block reads over a map as well as over the ground.
	double Widest = 0.0;
	for (const FString& Line : Lines) Widest = FMath::Max(Widest, (double)Font->GetStringSize(*Line));
	FCanvasTileItem Backing(At - FVector2D(8.0, 6.0), FVector2D(Widest * 1.15 + 16.0, Lines.Num() * 16.0 + 12.0), FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
	Backing.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Backing);

	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		FCanvasTextItem Item(FVector2D(At.X, At.Y + i * 16.0), FText::FromString(Lines[i]), Font, Colours[i]);
		Item.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Item);
	}
}

double URectDragToolBase::ViewportWidth(FCanvas* Canvas)
{
	if (Canvas == nullptr) return 1920.0;
	const float DPIScale = FMath::Max(Canvas->GetDPIScale(), UE_KINDA_SMALL_NUMBER);
	const FIntPoint Backbuffer = Canvas->GetRenderTarget() ? Canvas->GetRenderTarget()->GetSizeXY() : FIntPoint(1920, 1080);
	return Backbuffer.X / DPIScale;
}

TArray<FString> URectDragToolBase::WrapToWidth(const FString& Text, const UFont* Font, double MaxWidth)
{
	TArray<FString> Out;
	if (Font == nullptr)
	{
		Text.ParseIntoArrayLines(Out, /*bCullEmpty*/ false);
		return Out;
	}
	auto Width = [&](const FString& S) { return (double)Font->GetStringSize(*S); };

	TArray<FString> Paragraphs;
	Text.ParseIntoArrayLines(Paragraphs, /*bCullEmpty*/ false);
	for (const FString& Paragraph : Paragraphs)
	{
		TArray<FString> Words;
		Paragraph.ParseIntoArray(Words, TEXT(" "), /*bCullEmpty*/ true);
		FString Line;
		for (const FString& Word : Words)
		{
			const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			if (Width(Candidate) <= MaxWidth)
			{
				Line = Candidate;
				continue;
			}
			if (!Line.IsEmpty()) Out.Add(Line);
			Line.Reset();
			// A word wider than the line on its own — a run of Chinese has no spaces — breaks where it must.
			FString Piece;
			for (int32 i = 0; i < Word.Len(); ++i)
			{
				const FString Next = Piece + Word.Mid(i, 1);
				if (!Piece.IsEmpty() && Width(Next) > MaxWidth)
				{
					Out.Add(Piece);
					Piece = Word.Mid(i, 1);
				}
				else
				{
					Piece = Next;
				}
			}
			Line = Piece;
		}
		Out.Add(Line);
	}
	return Out;
}

FVector2D URectDragToolBase::HudCorner(FCanvas* Canvas)
{
	// Clear of the viewport's own toolbar, in DPI-independent units like everything drawn here.
	return FVector2D(24.0, 56.0);
}

FVector2D URectDragToolBase::ClampToViewport(FCanvas* Canvas, const FVector2D& At,
	const FVector2D& BlockSize)
{
	if (Canvas == nullptr) return At;
	const float DPIScale = FMath::Max(Canvas->GetDPIScale(), UE_KINDA_SMALL_NUMBER);
	const FIntPoint Backbuffer = Canvas->GetRenderTarget()
		? Canvas->GetRenderTarget()->GetSizeXY() : FIntPoint(1920, 1080);
	const FVector2D Screen(Backbuffer.X / DPIScale, Backbuffer.Y / DPIScale);

	// A margin, so text near an edge is inside it rather than half off it.
	constexpr double Margin = 8.0;
	return FVector2D(
		FMath::Clamp(At.X, Margin, FMath::Max(Screen.X - BlockSize.X - Margin, Margin)),
		FMath::Clamp(At.Y, Margin, FMath::Max(Screen.Y - BlockSize.Y - Margin, Margin)));
}

FText URectDragToolBase::GetPlacementSummaryText() const
{
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);

	FString Summary = FString::Printf(TEXT("%.0f x %.0f x %.0f cm  ·  %.0f°"),
		MaxX - MinX, MaxY - MinY, GetPreviewHeight(), PlacementYawDeg);

	const FString Detail = GetPlacementDetail();
	if (!Detail.IsEmpty())
	{
		Summary += TEXT("  ·  ") + Detail;
	}
	return FText::FromString(Summary);
}

void URectDragToolBase::DrawPreviewLine(FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B,
	const FLinearColor& Color, float Thickness)
{
	// Thickness is in screen pixels, not world centimetres.
	static const FLinearColor Backing(0.02f, 0.02f, 0.02f, 1.0f);
	PDI->DrawLine(A, B, Backing, SDPG_Foreground, Thickness + 4.0f, 0.0f, true);
	PDI->DrawLine(A, B, Color, SDPG_Foreground, Thickness, 0.0f, true);
}

void URectDragToolBase::DrawDashedPreviewLine(FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B,
	const FLinearColor& Color, float Thickness, double DashLength)
{
	const FVector Delta = B - A;
	const double Length = Delta.Size();
	if (Length < UE_KINDA_SMALL_NUMBER) return;

	// Round to an odd number of half-periods so a dash lands on both ends of the line.
	const int32 Steps = FMath::Max(3, 2 * FMath::RoundToInt32(Length / (2.0 * DashLength)) + 1);
	const FVector Step = Delta / Steps;
	for (int32 i = 0; i < Steps; i += 2)
	{
		DrawPreviewLine(PDI, A + Step * i, A + Step * (i + 1), Color, Thickness);
	}
}

bool URectDragToolBase::TryRayHitGround(const FInputDeviceRay& Ray, FVector& OutHit) const
{
	const FVector Origin = Ray.WorldRay.Origin;
	const FVector Dir = Ray.WorldRay.Direction;
	if (FMath::IsNearlyZero(Dir.Z)) return false;
	const double Plane = (bIsDragging) ? StartWorld.Z : 0.0;
	const double t = (Plane - Origin.Z) / Dir.Z;
	// A perspective ray that reaches the plane only behind its origin has it behind the camera and
	// must miss. An orthographic ray's origin sits on the view plane, which in a top-down viewport
	// is this very plane, so t there is zero or negative for every cursor position on it — a fact
	// about where the origin was put, not about what the cursor is over.
	if (t <= 0.0 && !IsActiveViewportOrtho()) return false;
	OutHit = Origin + Dir * t;
	return true;
}

FInputRayHit URectDragToolBase::GroundRayHit(const FInputDeviceRay& Ray) const
{
	FVector Hit;
	if (!TryRayHitGround(Ray, Hit)) return FInputRayHit();
	return FInputRayHit((Hit - Ray.WorldRay.Origin).Size());
}

FInputRayHit URectDragToolBase::IsHitByClick(const FInputDeviceRay& ClickPos)
{
	// A click on a built building is a selection — the editor's to make, after which the mode
	// brings up that building's tool — not the first corner of a new placement over its roof.
	// Only a placement already in hand keeps its finishing click wherever that lands.
	if (!bIsDragging)
	{
		FHitResult Hit;
		if (TraceHoveredBuilding(ClickPos, Hit)) return FInputRayHit();
	}
	return GroundRayHit(ClickPos);
}

void URectDragToolBase::OnClicked(const FInputDeviceRay& ClickPos)
{
	FVector Hit;
	if (!TryRayHitGround(ClickPos, Hit)) return;

	ProcessClick(Hit);
	UpdatePlacementReadout();
}

bool URectDragToolBase::IsSnapKeyDown()
{
	if (!FSlateApplication::IsInitialized()) return false;
	// Alt — which on a Mac is Option — or Command.
	const FModifierKeysState& Mods = FSlateApplication::Get().GetModifierKeys();
	return Mods.IsAltDown() || Mods.IsCommandDown();
}

bool URectDragToolBase::IsSnapKeyLatched() const
{
	if (IsSnapKeyDown())
	{
		// Latched here as well as read, so a placement that is mid-flight keeps it after the key
		// comes up — the click that finishes the placement has to arrive with the key released.
		bSnapKeyLatched = true;
		return true;
	}
	return bSnapKeyLatched;
}

bool URectDragToolBase::SnappingActive() const
{
	// One comparison, so the standing setting and the key cannot be read in different orders at
	// different sites. The key inverts whichever way the checkbox stands, which is why a prompt
	// only ever has to name the one thing the key is about to do.
	return Snap && (Snap->bEnabled != IsSnapKeyLatched());
}

FText SnapKeyName()
{
#if PLATFORM_MAC
	return NSLOCTEXT("HutongRectDragTool", "SnapKeyMac", "Option or \u2318");
#else
	return NSLOCTEXT("HutongRectDragTool", "SnapKeyPC", "Alt");
#endif
}

FVector URectDragToolBase::ApplySnap(const FVector& World, bool bIsAnchor)
{
	bSnapActive = false;

	// Each call answers for one end of the run, so that end's own last answer goes first. Left
	// standing, a miss keeps the previous placement's bearing and MiterExtend builds a corner for
	// a neighbour that is not there — on the run being drawn and on the next one placed.
	if (bIsAnchor)
	{
		AnchorSnapYawDeg = -1000.0;
		AnchorSnapYaw2Deg = -1000.0;
		AnchorSnapInward = FVector2D::ZeroVector;
		AnchorSnapBuilding.Reset();
	}
	else
	{
		CursorSnapYawDeg = -1000.0;
		CursorSnapYaw2Deg = -1000.0;
		CursorSnapInward = FVector2D::ZeroVector;
		CursorSnapBuilding.Reset();
	}

	if (!SnappingActive())
	{
		return World;
	}

	// Lane width first, and only for the anchor: a run's position across the street is set by where it starts.
	if (bIsAnchor && Snap->bSnapLaneWidth && WantsLaneWidthSnap())
	{
		// Queried without a bearing: at the first click nothing about the run's direction is known and PlacementYawDeg is whatever the last placement left behind.
		const HutongSnap::FGap Gap = HutongSnap::FindParallelGap(
			GetFootprints(), World, HutongSnap::AnyYaw, 2.0 * HutongGen::Urban::PitchCm);
		if (Gap.bFound)
		{
			// Measured face to face, the way the readout reports it and the way a street is actually wide.
			const double Offset = GetLaneFaceOffset();
			const double Clear = FMath::Max(Gap.DistanceCm - Offset, 0.0);
			const double Canonical = HutongGen::Urban::NearestCanonicalWidth(
				Clear, FMath::Max(Snap->LaneToleranceCm, 0.0));
			if (Canonical > 0.0)
			{
				// Move along the line to the neighbour, so only the distance changes.
				const double Shift = Clear - Canonical;
				const FVector Moved = World + FVector(Gap.Toward.X, Gap.Toward.Y, 0.0) * Shift;
				bSnapActive = true;
				SnapPoint = Moved;
				AnchorSnapYawDeg = Gap.EdgeYawDeg;
				if (Snap->bAdoptAngle)
				{
					PlacementYawDeg = Gap.EdgeYawDeg;
				}
				return Moved;
			}
		}
	}

	const double Radius = EffectiveSnapRadius(World);
	HutongSnap::FResult R = HutongSnap::FindSnap(GetFootprints(), World, Radius);

	// The cursor end keeps what it had while it stays near it — unless the corner at the end of a
	// held face is now in reach: a held edge is the line through it, and along that line the
	// corner where it meets the neighbour's next face is the point a run wants, with both
	// bearings, not the one face's. A corner off that line is some other building's, and a held
	// corner is kept as it is, or an end on a thin end face hops between its two corners per pixel.
	bool bFreshCorner = false;
	if (!bIsAnchor && bStickyCursorValid && R.bSnapped && R.EdgeYaw2Deg > -900.0)
	{
		const HutongSnap::FResult& S = StickyCursorSnap;
		if (S.EdgeYawDeg > -900.0 && S.EdgeYaw2Deg < -900.0)
		{
			const double Yaw = FMath::DegreesToRadians(S.EdgeYawDeg);
			const FVector2D E(FMath::Cos(Yaw), FMath::Sin(Yaw));
			const FVector2D D(R.Point.X - S.Point.X, R.Point.Y - S.Point.Y);
			bFreshCorner = FMath::Abs(D.X * E.Y - D.Y * E.X) <= 2.0;
		}
	}
	if (!bIsAnchor && bStickyCursorValid && !bFreshCorner)
	{
		const HutongSnap::FResult& S = StickyCursorSnap;
		bool bKeep = false;
		FVector Held = S.Point;
		if (S.EdgeYawDeg > -900.0 && S.EdgeYaw2Deg < -900.0)
		{
			// An edge: the line through the point, held within the radius across it and a few along.
			const double Yaw = FMath::DegreesToRadians(S.EdgeYawDeg);
			const FVector2D E(FMath::Cos(Yaw), FMath::Sin(Yaw));
			const FVector2D D(World.X - S.Point.X, World.Y - S.Point.Y);
			const double Along = FVector2D::DotProduct(D, E);
			const double Across = FMath::Abs(D.X * E.Y - D.Y * E.X);
			if (Across <= 1.5 * Radius && FMath::Abs(Along) <= 6.0 * Radius)
			{
				bKeep = true;
				Held = FVector(S.Point.X + E.X * Along, S.Point.Y + E.Y * Along, World.Z);
			}
		}
		else if (FVector::Dist2D(World, S.Point) <= 1.5 * Radius)
		{
			bKeep = true;
		}
		if (bKeep)
		{
			R = S;
			R.bSnapped = true;
			R.Point = Held;
		}
		else
		{
			bStickyCursorValid = false;
		}
	}

	if (!R.bSnapped)
	{
		return World;
	}
	if (!bIsAnchor)
	{
		StickyCursorSnap = R;
		bStickyCursorValid = true;
	}

	bSnapActive = true;
	SnapPoint = R.Point;

	// The anchor takes its neighbour's angle, and only the anchor's own end takes a miter from it.
	if (bIsAnchor)
	{
		AnchorSnapInward = R.Inward;
		AnchorSnapYawDeg = R.EdgeYawDeg;
		AnchorSnapYaw2Deg = R.EdgeYaw2Deg;
		AnchorSnapBuilding = R.Building;
		if (Snap->bAdoptAngle && R.EdgeYawDeg > -900.0)
		{
			PlacementYawDeg = HutongSnap::SnapYaw(
				PlacementYawDeg, { R.EdgeYawDeg }, FMath::Max(Snap->AngleToleranceDeg, 0.0) + 45.0);
		}
	}
	else
	{
		CursorSnapYawDeg = R.EdgeYawDeg;
		CursorSnapYaw2Deg = R.EdgeYaw2Deg;
		CursorSnapInward = R.Inward;
		CursorSnapBuilding = R.Building;
	}

	return R.Point;
}

double URectDragToolBase::MiterExtend(double RunYawDeg, double NeighbourYawDeg, double Thickness) const
{
	if (NeighbourYawDeg < -900.0 || Thickness <= 0.0)
	{
		return 0.0;
	}

	// Angle between the two lines, folded into [0, 90].
	double Delta = FMath::Fmod(FMath::Abs(RunYawDeg - NeighbourYawDeg), 180.0);
	if (Delta > 90.0) Delta = 180.0 - Delta;

	// Collinear needs nothing; the run already continues its neighbour.
	if (Delta < 1.0) return 0.0;

	const double HalfInterior = 0.5 * (180.0 - Delta);
	const double Tan = FMath::Tan(FMath::DegreesToRadians(HalfInterior));
	if (Tan < UE_KINDA_SMALL_NUMBER) return 0.0;

	return FMath::Clamp((0.5 * Thickness) / Tan, 0.0, 0.5 * Thickness);
}

void URectDragToolBase::ProcessClick(const FVector& Hit)
{
	if (!bIsDragging)
	{
		// A press inside a laid-out footprint never reaches here.
		AnchorSnapInward = FVector2D::ZeroVector;

		StartWorld = ApplySnap(Hit, true);
		CurrentWorld = StartWorld;
		bIsDragging = true;
		bRectCommitted = false;
		OnPlacementStarted(StartWorld);
		return;
	}

	if (!bRectCommitted)
	{
		if (!bRotateModeActive)
		{
			CurrentWorld = ApplySnap(Hit, false);
		}
		bRectCommitted = true;
		const bool bSpawn = OnRectCommitted(Hit);
		if (bSpawn)
		{
			SpawnFinalActor();
			// What was just placed has to be snappable on the next click.
			HutongSnap::Invalidate();
			ClearSnapBearings();
			bIsDragging = false;
			bRectCommitted = false;
		}
		return;
	}

	if (OnExtraStageClicked(Hit))
	{
		SpawnFinalActor();
		HutongSnap::Invalidate();
		ClearSnapBearings();
		bIsDragging = false;
		bRectCommitted = false;
	}
}

FInputRayHit URectDragToolBase::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
	const FInputRayHit Ground = GroundRayHit(PressPos);
	if (Ground.bHit) return Ground;

	// A ray aimed at a building from anywhere near eye level never reaches Z = 0.
	FHitResult Hit;
	if (TraceHoveredBuilding(PressPos, Hit))
	{
		return FInputRayHit((float)Hit.Distance);
	}
	return FInputRayHit();
}

void URectDragToolBase::OnBeginHover(const FInputDeviceRay& DevicePos)
{
}

bool URectDragToolBase::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	if (!bIsDragging) return true;
	FVector Hit;
	if (!TryRayHitGround(DevicePos, Hit)) return true;
	if (bRotateModeActive)
	{
		UpdateRotateFromCursor(Hit);
	}
	else if (!bRectCommitted)
	{
		CurrentWorld = ApplySnap(Hit, !bIsDragging);
	}
	else
	{
		bSnapActive = false;
	}
	OnPlacementHover(Hit);
	UpdatePlacementReadout();
	return true;
}

bool URectDragToolBase::TraceHoveredBuilding(const FInputDeviceRay& Ray, FHitResult& OutHit) const
{
	UWorld* World = GetToolManager()
		? GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld() : nullptr;
	if (World == nullptr) return false;

	const FVector Dir = (FVector)Ray.WorldRay.Direction;
	// The scene stands behind an orthographic ray's own origin, so the trace has to start further
	// back than the view plane the cursor was deprojected onto.
	FVector Origin = (FVector)Ray.WorldRay.Origin;
	if (IsActiveViewportOrtho()) Origin -= Dir * OrthoRayBackOff;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HutongHoverInspect), /*bTraceComplex*/ true);
	if (!World->LineTraceSingleByChannel(OutHit, Origin, Origin + Dir * 5000000.0,
			ECC_Visibility, Params))
	{
		return false;
	}

	const AActor* Actor = OutHit.GetActor();
	return Actor && Actor->FindComponentByClass<UHutongBuildingComponent>() != nullptr;
}

UHutongBuildingComponent* URectDragToolBase::FindPlanBuildingAt(const FVector& Ground, double* OutEdgeDistance) const
{
	UHutongBuildingComponent* Best = nullptr;
	double BestArea = TNumericLimits<double>::Max();
	double BestEdge = 0.0;
	for (const HutongSnap::FFootprint& F : GetFootprints())
	{
		UHutongBuildingComponent* B = F.Building.Get();
		if (!B || !F.bPlanOnly) continue;
		if (F.Size.X <= 0.0 || F.Size.Y <= 0.0) continue;
		// Every mesh here is built from the actor's origin, and the footprint is the placement's
		// own four corners, which with a skew are not the rectangle's.
		const FVector Local3 = F.ActorToWorld.InverseTransformPosition(Ground);
		const FVector2D Local(Local3.X, Local3.Y);
		FVector2D Quad[4];
		B->GetFootprintCorners(Quad);
		if (!HutongFootprint::PointInQuad(Quad, Local)) continue;
		const double Area = HutongFootprint::QuadArea(Quad);
		if (Area >= BestArea) continue;
		BestArea = Area;
		Best = B;
		BestEdge = HutongFootprint::DistanceToQuadEdge(Quad, Local);
	}
	if (OutEdgeDistance) *OutEdgeDistance = BestEdge;
	return Best;
}

double URectDragToolBase::WorldPerPixelAt(const FVector& P) const
{
	if (GEditor == nullptr) return 1.0;
	FViewport* Viewport = GEditor->GetActiveViewport();
	FEditorViewportClient* Client = Viewport
		? static_cast<FEditorViewportClient*>(Viewport->GetClient()) : nullptr;
	if (Client == nullptr) return 1.0;
	if (Client->IsOrtho())
	{
		return FMath::Max(Client->GetOrthoUnitsPerPixel(Viewport), UE_KINDA_SMALL_NUMBER);
	}
	const double Dist = FVector::Distance(Client->GetViewLocation(), P);
	const double Height = FMath::Max((double)Viewport->GetSizeXY().Y, 1.0);
	return FMath::Max(Dist * 2.0 * FMath::Tan(FMath::DegreesToRadians(Client->ViewFOV) * 0.5) / Height,
		UE_KINDA_SMALL_NUMBER);
}

const TArray<HutongSnap::FFootprint>& URectDragToolBase::GetFootprints() const
{
	// No age-out while the mouse is down: a drag cannot change what is in the level, and a
	// mid-drag refresh is a walk of the whole district for an answer that has not moved.
	const double MaxAge = (bIsDragging || IsEditingPlan()) ? -1.0 : 1.0;
	return HutongSnap::Cache().Get(GetWorld(), MaxAge);
}

EHutongBaySide URectDragToolBase::ComputeDefaultBaySide() const
{
	if (!GCurrentLevelEditingViewportClient) return EHutongBaySide::MinusY;
	const FVector2D CamLocal = WorldXYToLocalRect(GCurrentLevelEditingViewportClient->GetViewLocation());
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	return HutongGen::BaySide::DefaultFromCameraDelta(
		CamLocal.X - 0.5 * (MinX + MaxX), CamLocal.Y - 0.5 * (MinY + MaxY));
}

EHutongBaySide URectDragToolBase::ComputeClosestSide(double Hx, double Hy) const
{
	const FVector2D Local = WorldXYToLocalRect(FVector(Hx, Hy, 0.0));
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	return HutongGen::BaySide::ClosestToPoint(Local.X, Local.Y, MinX, MinY, MaxX, MaxY);
}

double URectDragToolBase::EffectiveSnapRadius(const FVector& P) const
{
	const double Configured = Snap ? FMath::Max(Snap->Radius, 1.0) : 55.0;
	return FMath::Max(Configured, SnapPixelFloor * WorldPerPixelAt(P));
}

void URectDragToolBase::UpdateHoverInspectionFromViewport()
{
	if (bIsDragging || GEditor == nullptr)
	{
		HoveredBuilding.Reset();
		return;
	}

	FViewport* Viewport = GEditor->GetActiveViewport();
	FEditorViewportClient* Client = Viewport
		? static_cast<FEditorViewportClient*>(Viewport->GetClient()) : nullptr;
	if (Client == nullptr) return;

	// A still cursor is over whatever it was over last frame, and HoveredBuilding still says what
	// that was — so the trace is skipped and the members stand as they are.
	const FIntPoint CursorPx(Viewport->GetMouseX(), Viewport->GetMouseY());
	if (bHoverTraceValid && CursorPx == LastHoverCursorPx) return;
	LastHoverCursorPx = CursorPx;
	bHoverTraceValid = true;
	HoveredBuilding.Reset();

	// The editor's own cursor, so this does not depend on a hover sequence being live.
	const FViewportCursorLocation Cursor = Client->GetCursorWorldLocationFromMousePos();

	FInputDeviceRay Ray;
	Ray.WorldRay = FRay((FVector3d)Cursor.GetOrigin(), (FVector3d)Cursor.GetDirection(), true);

	FHitResult Hit;
	if (TraceHoveredBuilding(Ray, Hit))
	{
		HoveredBuilding = Hit.GetActor()->FindComponentByClass<UHutongBuildingComponent>();
		HoveredWorldPoint = Hit.ImpactPoint;
		return;
	}
	FVector Ground;
	if (TryRayHitGround(Ray, Ground))
	{
		if (UHutongBuildingComponent* Plan = FindPlanBuildingAt(Ground))
		{
			HoveredBuilding = Plan;
			HoveredWorldPoint = Ground;
		}
	}
}

FText URectDragToolBase::GetHoverSummaryText() const
{
	// What the cursor is over, else what is selected: the readout should not go blank the moment
	// the cursor leaves the footprint for one of its own handles.
	const UHutongBuildingComponent* Building = HudBuilding.Get();
	if (Building == nullptr) Building = GetSelectedBuilding();
	const AActor* Actor = Building ? Building->GetOwner() : nullptr;
	if (Actor == nullptr) return FText::GetEmpty();

	const FVector2D Size = Building->GetFootprintSize();
	const FVector Loc = Actor->GetActorLocation();

	int32 Triangles = 0;
	if (const UStaticMeshComponent* MeshComp = Actor->FindComponentByClass<UStaticMeshComponent>())
	{
		if (const UStaticMesh* Static = MeshComp->GetStaticMesh())
		{
			Triangles = Static->GetNumTriangles(0);
		}
	}

	// What the piece is: the type it was placed with, and the preset that says which of that
	// type it is — a 正房 and a 耳房 are one generator and the preset is the whole difference.
	const FString What = Building->Preset.IsEmpty()
		? FString::Printf(TEXT("%s  ·  no preset"), *Building->GetTypeLabel().ToString())
		: FString::Printf(TEXT("%s  ·  %s"), *Building->GetTypeLabel().ToString(), *Building->Preset);

	FHutongPlanBays Bays;
	Building->GetPlanBays(Bays);
	const FString BayText = (Bays.Boundaries.Num() >= 2)
		? FString::Printf(TEXT("  ·  %d bays (間)"), Bays.Boundaries.Num() - 1)
		: FString();

	// What is on the ground: a laid-out rectangle has no mesh to count, and saying so is the
	// answer to why nothing is standing there.
	const FString Built = Building->bPlanOnly
		? FString(TEXT("plan only"))
		: FString::Printf(TEXT("%d tris"), Triangles);
	const FString DetailText =
		StaticEnum<EHutongDetail>()->GetDisplayNameTextByValue((int64)Building->DetailLevel).ToString();

	// What the polygon is worth as evidence, which is the half of the answer the geometry cannot
	// give: a traced building and a guessed one look the same from here. The note follows it when
	// there is one, cut to a line — the Details panel is where a long one is read.
	const FString Confidence = FString::Printf(TEXT("confidence %s"),
		*StaticEnum<EHutongConfidence>()->GetDisplayNameTextByValue((int64)Building->Confidence).ToString());
	FString NoteText = Building->Notes.TrimStartAndEnd().Replace(TEXT("\n"), TEXT(" "));
	if (NoteText.Len() > 64) NoteText = NoteText.Left(61) + TEXT("...");
	const FString NoteLine = NoteText.IsEmpty() ? FString() : FString::Printf(TEXT("\n%s"), *NoteText);

	const FString SkewText = Building->HasFootprintSkew() ? FString(TEXT("  ·  skewed (斜角)")) : FString();

	return FText::FromString(FString::Printf(
		TEXT("%s\n%.0f x %.0f cm%s%s  ·  %.0f°\n%s  ·  %s  ·  %s\n%s%s\nat %.0f, %.0f, %.0f cm"),
		*What, Size.X, Size.Y, *BayText, *SkewText, Actor->GetActorRotation().Yaw,
		*Built, *DetailText, *Actor->GetActorLabel(),
		*Confidence, *NoteLine,
		Loc.X, Loc.Y, Loc.Z));
}

void URectDragToolBase::DrawHoverInspection(FPrimitiveDrawInterface* PDI) const
{
	// Latched for DrawHUD, which runs after this in the same frame and must report what was outlined.
	HudBuilding = HoveredBuilding;
	HudWorldPoint = HoveredWorldPoint;

	const UHutongBuildingComponent* Building = HoveredBuilding.Get();
	if (Building == nullptr || PDI == nullptr) return;

	const AActor* Actor = Building->GetOwner();
	if (Actor == nullptr) return;

	// The footprint, not the actor's bounds. Bounds take in the eaves, the platform's overhang and the 下鹼's projection.
	const FVector2D Size = Building->GetFootprintSize();
	if (Size.X <= 0.0 || Size.Y <= 0.0) return;

	const FTransform Xform = Actor->GetActorTransform();
	FVector2D Quad[4];
	Building->GetFootprintCorners(Quad);
	auto At = [&](double X, double Y)
	{
		const FVector2D Q = HutongFootprint::Map(Size, Building->FootprintSkew, X, Y);
		return Xform.TransformPosition(FVector(Q.X, Q.Y, 0.0));
	};
	FVector Corners[4];
	for (int32 i = 0; i < 4; ++i) Corners[i] = Xform.TransformPosition(FVector(Quad[i].X, Quad[i].Y, 0.0));

	// Cool blue, which is the preview's colour for a thing that is not the footprint being dragged.
	const FLinearColor Mark(0.25f, 0.85f, 1.0f);
	for (int32 i = 0; i < 4; ++i)
	{
		PDI->DrawLine(Corners[i], Corners[(i + 1) % 4], Mark, SDPG_Foreground, 2.0f);
		// A short post at each corner.
		PDI->DrawLine(Corners[i], Corners[i] + FVector(0.0, 0.0, 70.0), Mark, SDPG_Foreground, 1.0f);
	}

	// The bays, drawn across the footprint the way the plan outline draws them, so how many 間 a
	// building is can be read off one that is already standing as well as one still on the ground.
	FHutongPlanBays Bays;
	Building->GetPlanBays(Bays);
	const bool bAlongX = Building->ArePlanBaysAlongX();
	const FLinearColor Division(Mark.R, Mark.G, Mark.B, 0.5f);
	for (int32 i = 1; i + 1 < Bays.Boundaries.Num(); ++i)
	{
		const double T = Bays.Boundaries[i];
		const FVector A = bAlongX ? At(T, 0.0) : At(0.0, T);
		const FVector B = bAlongX ? At(T, Size.Y) : At(Size.X, T);
		PDI->DrawLine(A, B, Division, SDPG_Foreground, 1.0f);
	}
}

void URectDragToolBase::DrawHoverInspectionHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) const
{
	const UHutongBuildingComponent* Building = HudBuilding.Get();
	if (Building == nullptr || Canvas == nullptr || RenderAPI == nullptr) return;

	const FSceneView* View = RenderAPI->GetSceneView();
	if (View == nullptr) return;

	// The same text the mode panel shows, split onto its own lines: the panel is the half that
	// can be verified from a headless run, so the two must not be written twice.
	TArray<FString> Lines;
	GetHoverSummaryText().ToString().ParseIntoArray(Lines, TEXT("\n"));
	if (Lines.Num() == 0) return;

	// Slate's font, not GetMediumFont. The engine's medium font is a bitmap with no CJK glyphs in it, and every one of these type labels is half Chinese.
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);

	// Projected from the point the ray struck. This one stays by the cursor — it names the thing
	// under it — but is held inside the viewport so a building hovered near an edge still reads.
	FVector2D Pixel;
	if (!View->WorldToPixel(HudWorldPoint, Pixel)) return;
	const float DPIScale = FMath::Max(Canvas->GetDPIScale(), UE_KINDA_SMALL_NUMBER);

	double Widest = 0.0;
	for (const FString& Line : Lines) Widest = FMath::Max(Widest, (double)Line.Len());
	const FVector2D Block(Widest * 6.5 + 24.0, Lines.Num() * 15.0 + 24.0);
	const FVector2D At = ClampToViewport(Canvas, Pixel / DPIScale, Block);

	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		FCanvasTextItem Item(FVector2D(At.X + 18.0, At.Y + 16.0 + i * 15.0),
			FText::FromString(Lines[i]), Font,
			(i == 0) ? FLinearColor(0.25f, 0.85f, 1.0f) : FLinearColor::White);
		Item.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Item);
	}
}

void URectDragToolBase::OnEndHover()
{
	// The cursor has left, so nothing is under it.
	HoveredBuilding.Reset();

}

void URectDragToolBase::ClearSnapBearings()
{
	AnchorSnapInward = FVector2D::ZeroVector;
	AnchorSnapYawDeg = -1000.0;
	AnchorSnapYaw2Deg = -1000.0;
	bStickyCursorValid = false;
	CursorSnapYawDeg = -1000.0;
	CursorSnapYaw2Deg = -1000.0;
	CursorSnapInward = FVector2D::ZeroVector;
	AnchorSnapBuilding.Reset();
	CursorSnapBuilding.Reset();
}

void URectDragToolBase::CancelPlacement()
{
	CancelPlanEdit();
	bSnapActive = false;
	ClearSnapBearings();
	bIsDragging = false;
	bRectCommitted = false;
	bRotateModeActive = false;
	UpdatePlacementReadout();
}

void URectDragToolBase::BeginRotateMode()
{
	if (!bIsDragging || bRotateModeActive) return;
	bRotateModeActive = true;
	RotateAnchorLocalRect = WorldXYToLocalRect(CurrentWorld);
	const double dx = CurrentWorld.X - StartWorld.X;
	const double dy = CurrentWorld.Y - StartWorld.Y;
	RotateAnchorCursorAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(dy, dx));
	RotateAnchorYawDeg = PlacementYawDeg;
}

void URectDragToolBase::EndRotateMode()
{
	bRotateModeActive = false;
}

void URectDragToolBase::UpdateRotateFromCursor(const FVector& CursorWorld)
{
	const double dx = CursorWorld.X - StartWorld.X;
	const double dy = CursorWorld.Y - StartWorld.Y;
	if (dx * dx + dy * dy < 1.0) return;

	const double NewCursorAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(dy, dx));
	double NewYaw = RotateAnchorYawDeg + (NewCursorAngleDeg - RotateAnchorCursorAngleDeg);

	const bool bFiveDegrees = FSlateApplication::IsInitialized()
		&& FSlateApplication::Get().GetModifierKeys().IsShiftDown();
	if (bFiveDegrees)
	{
		NewYaw = FMath::RoundToDouble(NewYaw / 5.0) * 5.0;
	}
	else if (SnappingActive() && Snap->bAdoptAngle)
	{
		// To the neighbours' bearings, and to quarter turns off them — not to a world grid.
		const TArray<double> Yaws = HutongSnap::GatherEdgeYaws(
			GetFootprints(), StartWorld, FMath::Max(Snap->Radius, 1.0) * 12.0);
		NewYaw = HutongSnap::SnapYaw(NewYaw, Yaws, FMath::Max(Snap->AngleToleranceDeg, 0.0));
	}

	NewYaw = FMath::Fmod(NewYaw, 360.0);
	if (NewYaw < 0.0) NewYaw += 360.0;
	PlacementYawDeg = NewYaw;

	// Keep rect rigid: re-project the captured local-frame extent into world.
	CurrentWorld = LocalRectToWorld(RotateAnchorLocalRect.X, RotateAnchorLocalRect.Y);
}

void URectDragToolBase::SpawnFinalActor()
{
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const double SizeX = MaxX - MinX;
	const double SizeY = MaxY - MinY;
	if (SizeX < 10.0 || SizeY < 10.0) return;

	// Delayed rather than shown outright.
	FScopedSlowTask Task(1.0f, LOCTEXT("PlacingActor", "Building geometry…"));
	Task.MakeDialogDelayed(0.4f);
	Task.EnterProgressFrame(1.0f);

	TArray<FDynamicMesh3> LODs;
	BuildLODsForRect(SizeX, SizeY, LODs);

	UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();
	const FVector ActorLocXY = LocalRectToWorld(MinX, MinY);
	const FTransform Xform(
		FRotator(0.0, PlacementYawDeg, 0.0),
		FVector(ActorLocXY.X, ActorLocXY.Y, StartWorld.Z));

	UInteractiveToolManager* ToolManager = GetToolManager();
	ToolManager->BeginUndoTransaction(NSLOCTEXT("HutongLayout", "PlaceActor", "Place Hutong Actor"));
	const FHutongPalette Palette = Appearance ? Appearance->Palette : FHutongPalette();
	// Plan-only: an actor with no mesh, drawn by the outline the component attaches.
	AStaticMeshActor* Actor = IsPlanOnly()
		? HutongGen::SpawnEmptyActor(World, Xform, GetActorNameBase())
		: HutongGen::SpawnStaticMeshActor(World, LODs, Xform, GetActorNameBase(), Palette);
	if (Actor)
	{
		AttachBuildingComponent(Actor, SizeX, SizeY);
		AdoptNeighbourBaseCourse(Actor);
	}
	ToolManager->EndUndoTransaction();
}

void URectDragToolBase::AdoptNeighbourBaseCourse(AStaticMeshActor* Actor)
{
	// The anchor's neighbour first, since that is the end drawn from.
	AdoptBaseCourseFrom(Actor, AnchorSnapBuilding.Get() ? AnchorSnapBuilding.Get() : CursorSnapBuilding.Get());
}

void URectDragToolBase::AdoptBaseCourseFrom(AStaticMeshActor* Actor, const UHutongBuildingComponent* Neighbour)
{
	// A piece placed against another is part of the same frontage, and its 下鹼 runs through at
	// the neighbour's line.
	UHutongBuildingComponent* Placed = Actor ? Actor->FindComponentByClass<UHutongBuildingComponent>() : nullptr;
	if (!Placed || Placed->GetBaseCourseTop() <= 0.0) return;
	if (!Neighbour || Neighbour == Placed) return;
	const double Top = Neighbour->GetBaseCourseTop();
	if (Top <= 0.0 || FMath::IsNearlyEqual(Top, Placed->GetBaseCourseTop(), 0.5)) return;
	Placed->Modify();
	Placed->SetBaseCourseTop(Top);
	if (!Placed->bPlanOnly) Placed->Rebuild();
}

// ---- Editing a laid-out building in place ----

namespace
{
	const FLinearColor PlanCornerColor(1.0f, 0.9f, 0.15f);
	const FLinearColor PlanEdgeColor(0.55f, 1.0f, 0.65f);
	const FLinearColor PlanRotateColor(0.45f, 0.8f, 1.0f);
	const FLinearColor PlanActiveColor(1.0f, 0.45f, 0.1f);
	// Shift held: the corners now angle the footprint rather than resize it.
	const FLinearColor PlanSkewColor(0.85f, 0.55f, 1.0f);
	const FLinearColor PlanDoorColor(1.0f, 0.3f, 0.25f);
	// The bay-line markers that divide, and the end markers that fuse.
	const FLinearColor PlanDivideColor(0.3f, 0.9f, 0.95f);
	const FLinearColor PlanFuseColor(0.45f, 1.0f, 0.55f);
	constexpr double PlanMinSize = 20.0;

	void NotifyEdit(const FText& Message, bool bSuccess)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = bSuccess ? 3.0f : 5.0f;
		Info.bUseSuccessFailIcons = true;
		if (TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	void PlanSides(int32 Handle, int32& OutX, int32& OutY)
	{
		static const int32 X[8] = { -1, +1, +1, -1, 0, +1, 0, -1 };
		static const int32 Y[8] = { -1, -1, +1, +1, -1, 0, +1, 0 };
		const int32 H = FMath::Clamp(Handle, 0, 7);
		OutX = X[H];
		OutY = Y[H];
	}

	FVector GroundOf(const FInputDeviceRay& Ray, bool& bOk)
	{
		const FVector Origin = Ray.WorldRay.Origin;
		const FVector Dir = Ray.WorldRay.Direction;
		bOk = !FMath::IsNearlyZero(Dir.Z);
		if (!bOk) return FVector::ZeroVector;
		const double T = -Origin.Z / Dir.Z;
		bOk = T >= 0.0;
		return Origin + Dir * T;
	}
}

bool URectDragToolBase::TurnSelectedFacing(int32 Delta)
{
	if (GEditor == nullptr || Delta == 0) return false;
	USelection* Selected = GEditor->GetSelectedActors();
	if (!Selected) return false;

	TArray<UHutongBuildingComponent*> Work;
	for (int32 i = 0; i < Selected->Num(); ++i)
	{
		AActor* Actor = Cast<AActor>(Selected->GetSelectedObject(i));
		UHutongBuildingComponent* B = Actor ? Actor->FindComponentByClass<UHutongBuildingComponent>() : nullptr;
		EHutongBaySide Side;
		if (B && B->GetFacade(Side)) Work.Add(B);
	}
	if (Work.Num() == 0) return false;

	const FScopedTransaction Transaction(LOCTEXT("TurnFacing", "Turn Building Facade"));
	for (UHutongBuildingComponent* B : Work)
	{
		EHutongBaySide Side = EHutongBaySide::MinusY;
		B->GetFacade(Side);
		// The sides are declared in turning order (−Y, +X, +Y, −X), so a step is an addition.
		const int32 Next = (((int32)Side + Delta * B->FacadeTurnStep()) % 4 + 4) % 4;
		if (B->GetOwner()) B->GetOwner()->Modify();
		B->Modify();
		if (!B->SetFacade((EHutongBaySide)Next)) continue;
		// The same seam a panel edit goes through: a built building re-bakes, a laid-out one redraws its hatching.
		B->Rebuild();
		B->ApplyPlacementAttachments();
	}
	return true;
}

UHutongBuildingComponent* URectDragToolBase::GetSelectedBuilding() const
{
	if (IsEditingPlan()) return EditedPlan.Get();
	if (GEditor == nullptr) return nullptr;
	USelection* Selected = GEditor->GetSelectedActors();
	if (!Selected || Selected->Num() != 1) return nullptr;
	AActor* Actor = Cast<AActor>(Selected->GetSelectedObject(0));
	return Actor ? Actor->FindComponentByClass<UHutongBuildingComponent>() : nullptr;
}

UHutongBuildingComponent* URectDragToolBase::GetSelectedPlanBuilding() const
{
	UHutongBuildingComponent* B = GetSelectedBuilding();
	return (B && B->bPlanOnly) ? B : nullptr;
}

FVector URectDragToolBase::PlanOpeningWorld(const UHutongBuildingComponent* Building, int32 Index, double& OutWidth) const
{
	TArray<FHutongPlanOpening> Openings;
	Building->GetPlanOpenings(Openings);
	OutWidth = 0.0;
	const AActor* Owner = Building->GetOwner();
	if (!Owner || !Openings.IsValidIndex(Index)) return FVector::ZeroVector;
	const FVector2D Size = Building->GetFootprintSize();
	OutWidth = Openings[Index].Width;
	const double Along = Openings[Index].Centre;
	const FVector Local = Building->IsRunAlongY()
		? FVector(0.5 * Size.X, Along, 0.0) : FVector(Along, 0.5 * Size.Y, 0.0);
	return Owner->GetActorTransform().TransformPosition(Local);
}

FVector URectDragToolBase::PlanHandleLocal(const FVector2D Corners[4], int32 Handle)
{
	if (Handle >= 0 && Handle < 4) return FVector(Corners[Handle].X, Corners[Handle].Y, 0.0);
	const int32 Edge = FMath::Clamp(Handle - 4, 0, 3);
	const FVector2D Mid = 0.5 * (Corners[Edge] + Corners[(Edge + 1) % 4]);
	return FVector(Mid.X, Mid.Y, 0.0);
}

bool URectDragToolBase::IsSkewKeyDown()
{
	return FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsShiftDown();
}

double URectDragToolBase::PlanHandleSize(const FVector2D& Size)
{
	// A tenth of the shorter side, held between a hand's breadth and a pace.
	const double Basis = IsLineLikePlan(Size) ? 0.03 * FMath::Max(Size.X, Size.Y) : 0.1 * FMath::Min(Size.X, Size.Y);
	return FMath::Clamp(Basis, 50.0, 160.0);
}

double URectDragToolBase::PlanCornerHandleSize(const FVector2D& Size)
{
	if (!IsLineLikePlan(Size)) return PlanHandleSize(Size);
	return FMath::Clamp(0.35 * FMath::Min(Size.X, Size.Y), 12.0, 160.0);
}

bool URectDragToolBase::IsLineLikePlan(const FVector2D& Size)
{
	return HutongFootprint::IsLineLike(Size);
}

bool URectDragToolBase::PlanHandleEnabled(const FVector2D& Size, int32 Handle)
{
	if (!IsLineLikePlan(Size)) return true;
	const bool bAlongX = Size.X >= Size.Y;
	return bAlongX ? (Handle == 5 || Handle == 7) : (Handle == 4 || Handle == 6);
}

bool URectDragToolBase::IsPlanGrabPoint(const FVector2D& Size, double EdgeDistance, const FVector& Ground) const
{
	if (IsLineLikePlan(Size)) return true;
	// The band a neighbouring placement starts in cannot be allowed to swallow the footprint: on a
	// 垂花門 at its smallest the snap radius reaches past the middle from both long edges, and a plan
	// nothing can be pressed on is one that can only be placed again.
	const double Band = FMath::Min(EffectiveSnapRadius(Ground), 0.3 * FMath::Min(Size.X, Size.Y));
	return EdgeDistance > Band;
}

FVector URectDragToolBase::PlanRotateHandleWorld(const UHutongBuildingComponent* Building, FVector& OutEdgeMid) const
{
	// Beyond the front, or beyond −Y for a type with no front, two handle-widths out so it never sits on a corner however the footprint is shaped.
	const AActor* Owner = Building->GetOwner();
	const FTransform Xf = Owner->GetActorTransform();
	const FVector2D Size = Building->GetFootprintSize();
	EHutongBaySide Side = EHutongBaySide::MinusY;
	Building->GetFacade(Side);
	const HutongGen::BaySide::FEdge E =
		HutongGen::BaySide::GetEdge((HutongGen::EBaySide)Side, 0.0, 0.0, Size.X, Size.Y);
	const FVector2D Mid = HutongFootprint::Map(Size, Building->FootprintSkew,
		E.bAlongX ? 0.5 * Size.X : E.FixedCoord, E.bAlongX ? E.FixedCoord : 0.5 * Size.Y);
	OutEdgeMid = Xf.TransformPosition(FVector(Mid.X, Mid.Y, 0.0));
	const FVector Out = Xf.TransformVector(FVector(E.OutDir.X, E.OutDir.Y, 0.0)).GetSafeNormal2D();
	return OutEdgeMid + Out * (2.0 * PlanHandleSize(Size));
}

int32 URectDragToolBase::HitTestPlan(const UHutongBuildingComponent* Building, const FVector& Ground) const
{
	const AActor* Owner = Building ? Building->GetOwner() : nullptr;
	if (!Owner) return INDEX_NONE;
	const FTransform Xf = Owner->GetActorTransform();
	const FVector2D Size = Building->GetFootprintSize();
	const double Pick = 0.75 * PlanHandleSize(Size);

	// The opening sliders come first and are offered on any building that has them, built or laid out; everything below is for a laid-out one.
	{
		TArray<FHutongPlanOpening> Openings;
		Building->GetPlanOpenings(Openings);
		for (int32 i = 0; i < Openings.Num(); ++i)
		{
			double Width;
			if (FVector::Dist2D(Ground, PlanOpeningWorld(Building, i, Width)) <= Pick) return PlanHandleOpening + i;
		}
	}
	// The divide markers on the bay lines and the fuse markers on a shared end, on any building
	// with bays, built or laid out. Picked small, so the inside of a laid-out plan stays a grab.
	if (HutongDetailOps::CanDivide(Building))
	{
		const double MarkerPick = 0.5 * PlanHandleSize(Size);
		FHutongPlanBays Bays;
		Building->GetPlanBays(Bays);
		Bays.Boundaries.Sort();
		for (int32 i = 1; i + 1 < Bays.Boundaries.Num(); ++i)
		{
			if (FVector::Dist2D(Ground, PlanBayMarkerWorld(Building, Bays.Boundaries[i])) <= MarkerPick) return PlanHandleBay + i;
		}
		const double Run = Building->ArePlanBaysAlongX() ? Size.X : Size.Y;
		if (FindFuseNeighbour(Building, true) && FVector::Dist2D(Ground, PlanBayMarkerWorld(Building, Run)) <= MarkerPick) return PlanHandleFuseEnd;
		if (FindFuseNeighbour(Building, false) && FVector::Dist2D(Ground, PlanBayMarkerWorld(Building, 0.0)) <= MarkerPick) return PlanHandleFuseStart;
	}
	FVector2D Quad[4];
	Building->GetFootprintCorners(Quad);

	// Under Shift the corners are offered to be angled, on every kind of footprint: a wall meets
	// an off-square neighbour with its end cut on the bias. Picked at the size they are drawn,
	// which on a run is well inside the thickness.
	const bool bSkew = IsSkewKeyDown();
	const double CornerPick = 0.75 * PlanCornerHandleSize(Size);
	if (bSkew)
	{
		int32 Nearest = INDEX_NONE;
		double NearestDist = CornerPick;
		for (int32 H = 0; H < 4; ++H)
		{
			const double Dist = FVector::Dist2D(Ground, Xf.TransformPosition(PlanHandleLocal(Quad, H)));
			if (Dist <= NearestDist) { NearestDist = Dist; Nearest = H; }
		}
		if (Nearest != INDEX_NONE) return Nearest;
	}
	// A built building offers nothing else: a plain click on it is a placement click and stays one.
	if (!Building->bPlanOnly) return INDEX_NONE;

	FVector EdgeMid;
	if (FVector::Dist2D(Ground, PlanRotateHandleWorld(Building, EdgeMid)) <= Pick) return PlanHandleRing;
	// Corners before edges, so a small footprint's corner wins over the midpoint beside it.
	for (int32 H = 0; H < 8; ++H)
	{
		if (!PlanHandleEnabled(Size, H)) continue;
		if (FVector::Dist2D(Ground, Xf.TransformPosition(PlanHandleLocal(Quad, H))) <= Pick) return H;
	}
	const FVector Local3 = Xf.InverseTransformPosition(Ground);
	const FVector2D Local(Local3.X, Local3.Y);
	if (!HutongFootprint::PointInQuad(Quad, Local)) return INDEX_NONE;
	const double Edge = HutongFootprint::DistanceToQuadEdge(Quad, Local);
	return IsPlanGrabPoint(Size, Edge, Ground) ? PlanHandleInside : INDEX_NONE;
}

void URectDragToolBase::BeginPlanEdit(UHutongBuildingComponent* Building, int32 Hit, const FVector& Ground)
{
	AActor* Owner = Building ? Building->GetOwner() : nullptr;
	if (!Owner || Hit == INDEX_NONE) return;
	const FTransform Xf = Owner->GetActorTransform();
	const FVector2D Size = Building->GetFootprintSize();

	EditedPlan = Building;
	EditStartTransform = Xf;
	EditStartSize = Size;
	EditStartSkew = Building->FootprintSkew;
	EditHandle = INDEX_NONE;
	bPlanDragMoved = false;

	if (Hit >= PlanHandleBay)
	{
		PlanEdit = EPlanEdit::Divide;
		EditHandle = Hit - PlanHandleBay;
	}
	else if (Hit == PlanHandleFuseStart || Hit == PlanHandleFuseEnd)
	{
		PlanEdit = EPlanEdit::Fuse;
		EditHandle = Hit;
	}
	else if (Hit >= PlanHandleOpening)
	{
		PlanEdit = EPlanEdit::Opening;
		EditHandle = Hit - PlanHandleOpening;
		TArray<FHutongPlanOpening> Openings;
		Building->GetPlanOpenings(Openings);
		EditStartOpeningCentre = Openings.IsValidIndex(EditHandle) ? Openings[EditHandle].Centre : 0.0;
	}
	else if (Hit == PlanHandleRing)
	{
		PlanEdit = EPlanEdit::Rotate;
		const FVector Centre = Xf.TransformPosition(FVector(0.5 * Size.X, 0.5 * Size.Y, 0.0));
		EditStartCursorAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Ground.Y - Centre.Y, Ground.X - Centre.X));
		EditStartYawDeg = Xf.Rotator().Yaw;
	}
	else if (Hit == PlanHandleInside)
	{
		PlanEdit = EPlanEdit::Move;
		const FVector Local = Xf.InverseTransformPosition(Ground);
		EditGrabLocal = FVector(Local.X, Local.Y, 0.0);
	}
	else if (Hit >= 4 && Hit < 8 && Cast<UHutongWallBuildingComponent>(Building)
		&& ((Building->IsRunAlongY() && (Hit == 4 || Hit == 6)) || (!Building->IsRunAlongY() && (Hit == 5 || Hit == 7))))
	{
		// The end of a wall leg: not the rectangle's edge but the run's vertex, and the legs
		// meeting there go with it.
		UHutongWallBuildingComponent* Wall = Cast<UHutongWallBuildingComponent>(Building);
		if (HutongWallRun::Gather(Wall, GatherWallLegs(), EditRun))
		{
			const int32 Leg = EditRun.Legs.IndexOfByPredicate([&](const TWeakObjectPtr<UHutongWallBuildingComponent>& L) { return L.Get() == Wall; });
			const bool bStartEnd = Building->IsRunAlongY() ? (Hit == 4) : (Hit == 7);
			EditRunVertex = bStartEnd ? Leg : Leg + 1;
			EditRunStart.Reset();
			for (const TWeakObjectPtr<UHutongWallBuildingComponent>& L : EditRun.Legs)
			{
				FRunLegState State;
				CaptureLegState(L.Get(), State);
				EditRunStart.Add(State);
			}
			PlanEdit = EPlanEdit::RunVertex;
			EditHandle = Hit;
			bEditRunSideOn = false;
		}
		else
		{
			PlanEdit = EPlanEdit::Resize;
			EditHandle = Hit;
		}
	}
	else if (Hit < 4 && IsSkewKeyDown())
	{
		PlanEdit = EPlanEdit::Skew;
		EditHandle = Hit;
		EditSkewAxis = INDEX_NONE;
		bSkewAxisHeld = false;
		// The corner keeps its distance from the cursor instead of jumping under it on the press.
		const FVector Local = Xf.InverseTransformPosition(Ground);
		EditGrabLocal = FVector(Local.X, Local.Y, 0.0);
	}
	else
	{
		PlanEdit = EPlanEdit::Resize;
		EditHandle = Hit;
	}
}

FInputRayHit URectDragToolBase::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	if (bIsDragging) return FInputRayHit();
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsControlDown())
	{
		return FInputRayHit();
	}
	bool bOk = false;
	const FVector Ground = GroundOf(PressPos, bOk);
	if (!bOk) return FInputRayHit();

	bool bOver = false;
	if (const UHutongBuildingComponent* Selected = GetSelectedBuilding())
	{
		bOver = HitTestPlan(Selected, Ground) != INDEX_NONE;
	}
	if (!bOver)
	{
		double Edge = 0.0;
		const UHutongBuildingComponent* Plan = FindPlanBuildingAt(Ground, &Edge);
		bOver = Plan && IsPlanGrabPoint(Plan->GetFootprintSize(), Edge, Ground);
	}
	if (!bOver)
	{
		// Open ground: a drag from here is a box selection, a click still a placement — both are
		// this behaviour's now and told apart on release. Built geometry is left to the editor's
		// own selection, as the click behaviour leaves it.
		FHitResult Hit;
		if (TraceHoveredBuilding(PressPos, Hit)) return FInputRayHit();
	}
	return FInputRayHit((float)FVector::Distance(PressPos.WorldRay.Origin, Ground));
}

void URectDragToolBase::OnClickPress(const FInputDeviceRay& PressPos)
{
	bool bOk = false;
	const FVector Ground = GroundOf(PressPos, bOk);
	if (!bOk) return;

	PlanPressPixel = PressPos.bHas2D ? PressPos.ScreenPosition : FVector2D::ZeroVector;
	bPlanPressHasPixel = PressPos.bHas2D;

	UHutongBuildingComponent* Building = GetSelectedBuilding();
	int32 Hit = Building ? HitTestPlan(Building, Ground) : INDEX_NONE;
	if (Hit == INDEX_NONE)
	{
		// The press is on another laid-out building: select it, and let the same press move it.
		Building = FindPlanBuildingAt(Ground);
		if (!Building || !Building->GetOwner() || GEditor == nullptr)
		{
			// Nothing under it: a box begins here.
			PlanEdit = EPlanEdit::Marquee;
			EditedPlan.Reset();
			EditHandle = INDEX_NONE;
			bPlanDragMoved = false;
			MarqueeStart = Ground;
			MarqueeEnd = Ground;
			return;
		}
		GEditor->SelectNone(false, true);
		GEditor->SelectActor(Building->GetOwner(), true, true, true);
		Hit = PlanHandleInside;
	}
	BeginPlanEdit(Building, Hit, Ground);
}

void URectDragToolBase::OnClickDrag(const FInputDeviceRay& DragPos)
{
	if (!IsEditingPlan()) return;
	bool bOk = false;
	const FVector Ground = GroundOf(DragPos, bOk);
	if (!bOk) return;

	// A drag event is not movement: the cursor has to leave the press's own pixel by a slop before
	// this is an edit rather than the selection click it started as.
	if (!bPlanDragMoved && bPlanPressHasPixel && DragPos.bHas2D
		&& FVector2D::Distance(DragPos.ScreenPosition, PlanPressPixel) <= (PlanEdit == EPlanEdit::Marquee ? MarqueeSlopPixels : PlanClickSlopPixels))
	{
		return;
	}
	bPlanDragMoved = true;
	if (PlanEdit == EPlanEdit::Marquee)
	{
		MarqueeEnd = Ground;
		return;
	}
	UpdatePlanEdit(Ground);
}

void URectDragToolBase::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	if (!IsEditingPlan()) return;
	if (PlanEdit == EPlanEdit::Marquee)
	{
		const bool bMoved = bPlanDragMoved;
		const FVector Start = MarqueeStart, End = MarqueeEnd;
		CancelPlanEdit();
		if (!bMoved)
		{
			// Never dragged: the click it was, which places.
			FVector Hit;
			if (TryRayHitGround(ReleasePos, Hit))
			{
				ProcessClick(Hit);
				UpdatePlacementReadout();
			}
			return;
		}
		// Every building the box touches: a corner or the centre of its footprint inside it.
		const FVector2D Lo(FMath::Min(Start.X, End.X), FMath::Min(Start.Y, End.Y));
		const FVector2D Hi(FMath::Max(Start.X, End.X), FMath::Max(Start.Y, End.Y));
		auto Inside = [&](const FVector& P) { return P.X >= Lo.X && P.X <= Hi.X && P.Y >= Lo.Y && P.Y <= Hi.Y; };
		TArray<AActor*> Picked;
		for (const HutongSnap::FFootprint& F : GetFootprints())
		{
			UHutongBuildingComponent* B = F.Building.Get();
			AActor* Actor = B ? B->GetOwner() : nullptr;
			if (!Actor) continue;
			FVector Centre = FVector::ZeroVector;
			bool bIn = false;
			for (int32 i = 0; i < 4; ++i)
			{
				bIn = bIn || Inside(F.Corners[i]);
				Centre += 0.25 * F.Corners[i];
			}
			if (bIn || Inside(Centre)) Picked.Add(Actor);
		}
		if (GEditor == nullptr) return;
		const FScopedTransaction Transaction(LOCTEXT("MarqueeSelect", "Select Hutong Buildings"));
		// Shift adds, as the editor's own box does.
		if (!IsSkewKeyDown()) GEditor->SelectNone(false, true);
		for (AActor* Actor : Picked) GEditor->SelectActor(Actor, true, false, true);
		GEditor->NoteSelectionChange();
		return;
	}
	if (PlanEdit == EPlanEdit::Divide || PlanEdit == EPlanEdit::Fuse)
	{
		// The marker is a button: released where it was pressed, it acts; dragged off, nothing.
		const bool bMoved = bPlanDragMoved;
		const EPlanEdit Kind = PlanEdit;
		const int32 Handle = EditHandle;
		UHutongBuildingComponent* B = EditedPlan.Get();
		CancelPlanEdit();
		if (bMoved || !B) return;
		if (Kind == EPlanEdit::Divide) DivideAtMarker(B, Handle);
		else FuseAtMarker(B, Handle == PlanHandleFuseEnd);
		return;
	}
	// A press that never moved was a selection, and there is nothing to commit.
	if (bPlanDragMoved) CommitPlanEdit();
	else CancelPlanEdit();
}

UHutongBuildingComponent* URectDragToolBase::FindFuseNeighbour(const UHutongBuildingComponent* Building, bool bAtEnd) const
{
	if (!Building) return nullptr;
	for (const HutongSnap::FFootprint& F : GetFootprints())
	{
		UHutongBuildingComponent* Other = F.Building.Get();
		if (!Other || Other == Building || Other->GetClass() != Building->GetClass()) continue;
		bool bEnd = false;
		if (HutongDetailOps::CanFuse(Building, Other, bEnd) && bEnd == bAtEnd) return Other;
	}
	return nullptr;
}

FVector URectDragToolBase::PlanBayMarkerWorld(const UHutongBuildingComponent* Building, double Along) const
{
	const AActor* Owner = Building ? Building->GetOwner() : nullptr;
	if (!Owner) return FVector::ZeroVector;
	const FVector2D Size = Building->GetFootprintSize();
	const bool bAlongX = Building->ArePlanBaysAlongX();
	const FVector2D Q = HutongFootprint::Map(Size, Building->FootprintSkew,
		bAlongX ? Along : 0.5 * Size.X, bAlongX ? 0.5 * Size.Y : Along);
	return Owner->GetActorTransform().TransformPosition(FVector(Q.X, Q.Y, 0.0));
}

void URectDragToolBase::DivideAtMarker(UHutongBuildingComponent* Building, int32 BayLine)
{
	FText WhyNot;
	UHutongBuildingComponent* New = nullptr;
	{
		const FScopedTransaction Transaction(LOCTEXT("DivideBuilding", "Divide Hutong Building"));
		FScopedSlowTask Task(1.0f, LOCTEXT("Dividing", "Dividing the building…"));
		Task.MakeDialogDelayed(0.4f);
		Task.EnterProgressFrame(1.0f);
		New = HutongDetailOps::DivideBuilding(Building, BayLine, WhyNot);
	}
	if (!New)
	{
		NotifyEdit(WhyNot, false);
		return;
	}
	FHutongPlanBays Kept, Taken;
	Building->GetPlanBays(Kept);
	New->GetPlanBays(Taken);
	NotifyEdit(FText::Format(LOCTEXT("DivideDone", "Divided (分間): {0} bays kept, {1} in the new building."),
		FText::AsNumber(Kept.Boundaries.Num() - 1), FText::AsNumber(Taken.Boundaries.Num() - 1)), true);
	bHoverTraceValid = false;
	UpdatePlacementReadout();
}

void URectDragToolBase::FuseAtMarker(UHutongBuildingComponent* Building, bool bAtEnd)
{
	UHutongBuildingComponent* Other = FindFuseNeighbour(Building, bAtEnd);
	if (!Other)
	{
		NotifyEdit(LOCTEXT("FuseNoNeighbour", "No like building stands end to end on this line."), false);
		return;
	}
	FText WhyNot;
	bool bFused = false;
	{
		const FScopedTransaction Transaction(LOCTEXT("FuseBuildings", "Fuse Hutong Buildings"));
		FScopedSlowTask Task(1.0f, LOCTEXT("Fusing", "Fusing the buildings…"));
		Task.MakeDialogDelayed(0.4f);
		Task.EnterProgressFrame(1.0f);
		bFused = HutongDetailOps::FuseBuildings(Building, Other, WhyNot);
	}
	if (!bFused)
	{
		NotifyEdit(WhyNot, false);
		return;
	}
	FHutongPlanBays Bays;
	Building->GetPlanBays(Bays);
	NotifyEdit(FText::Format(LOCTEXT("FuseDone", "Fused (合併) into one building of {0} bays."),
		FText::AsNumber(Bays.Boundaries.Num() - 1)), true);
	bHoverTraceValid = false;
	UpdatePlacementReadout();
}

void URectDragToolBase::OnTerminateDragSequence()
{
	CancelPlanEdit();
}

void URectDragToolBase::UpdatePlanEdit(const FVector& Ground)
{
	UHutongBuildingComponent* B = EditedPlan.Get();
	AActor* Owner = B ? B->GetOwner() : nullptr;
	if (!B || !Owner)
	{
		PlanEdit = EPlanEdit::None;
		return;
	}
	const double Z = EditStartTransform.GetLocation().Z;

	switch (PlanEdit)
	{
	case EPlanEdit::Divide:
	case EPlanEdit::Fuse:
		return;
	case EPlanEdit::Move:
	{
		// The grabbed point follows the cursor; the origin corner then snaps to the neighbours, ignoring the building itself.
		FVector Origin = Ground - EditStartTransform.GetRotation().RotateVector(EditGrabLocal);
		Origin.Z = Z;
		if (SnappingActive())
		{
			const HutongSnap::FResult R = HutongSnap::FindSnap(GetFootprints(), Origin, EffectiveSnapRadius(Origin), Owner);
			if (R.bSnapped) Origin = FVector(R.Point.X, R.Point.Y, Z);
		}
		Owner->SetActorLocation(Origin);
		break;
	}
	case EPlanEdit::Resize:
	{
		FVector G = Ground;
		if (SnappingActive())
		{
			const HutongSnap::FResult R = HutongSnap::FindSnap(GetFootprints(), G, EffectiveSnapRadius(G), Owner);
			if (R.bSnapped) G = FVector(R.Point.X, R.Point.Y, G.Z);
		}
		const FVector Local = EditStartTransform.InverseTransformPosition(G);

		int32 SideX, SideY;
		PlanSides(EditHandle, SideX, SideY);
		const FVector2D S = EditStartSize;
		double MinX = 0.0, MinY = 0.0, MaxX = S.X, MaxY = S.Y;
		if (SideX < 0) MinX = FMath::Min(Local.X, MaxX - PlanMinSize);
		if (SideX > 0) MaxX = FMath::Max(Local.X, PlanMinSize);
		if (SideY < 0) MinY = FMath::Min(Local.Y, MaxY - PlanMinSize);
		if (SideY > 0) MaxY = FMath::Max(Local.Y, PlanMinSize);

		B->SetFootprintSize(FVector2D(MaxX - MinX, MaxY - MinY));
		// A minus-side drag keeps the far edge where it is.
		const FVector2D Accepted = B->GetFootprintSize();
		const FVector Shift(SideX < 0 ? S.X - Accepted.X : 0.0, SideY < 0 ? S.Y - Accepted.Y : 0.0, 0.0);
		Owner->SetActorTransform(EditStartTransform);
		Owner->SetActorLocation(EditStartTransform.GetLocation() + EditStartTransform.TransformVector(Shift));
		break;
	}
	case EPlanEdit::Skew:
	{
		// The corner follows the cursor and the other three stay where they are. It stops where
		// the footprint would fold: the last corner set that still builds is kept.
		const FVector2D S = EditStartSize;
		const FVector2D Rect[4] = { FVector2D(0.0, 0.0), FVector2D(S.X, 0.0), FVector2D(S.X, S.Y), FVector2D(0.0, S.Y) };
		const FVector2D StartOffset = EditStartSkew.Get(EditHandle);
		const FVector2D StartLocal = Rect[EditHandle] + StartOffset;
		FHutongFootprintSkew Candidate = B->FootprintSkew;
		const bool bEnds = Candidate.Mode == EHutongSkewMode::Ends;

		// Under Ends the corner goes one way per drag, along the run or across it, whichever the
		// cursor pulls towards. Read off the whole pull, not its first centimetres — a hand that
		// wobbles across before it sets off along the run must not lock the wrong axis — and held
		// once the pull is two handles long, so a diagonal never flips the corner between two
		// lines after that.
		const FVector RawLocal = EditStartTransform.InverseTransformPosition(Ground);
		const FVector2D Pull(RawLocal.X - EditGrabLocal.X, RawLocal.Y - EditGrabLocal.Y);
		if (bEnds && !bSkewAxisHeld)
		{
			const double Handle = PlanCornerHandleSize(S);
			if (Pull.Size() < 0.5 * Handle) break;
			EditSkewAxis = FMath::Abs(Pull.X) >= FMath::Abs(Pull.Y) ? 0 : 1;
			bSkewAxisHeld = Pull.Size() >= 2.0 * Handle;
		}

		// Where the corner sits unsnapped: under the cursor as it was grabbed, held to its axis under Ends.
		const FVector2D Grab = FVector2D(EditGrabLocal.X, EditGrabLocal.Y) - StartLocal;
		FVector2D CornerLocal = FVector2D(RawLocal.X, RawLocal.Y) - Grab;
		if (bEnds)
		{
			if (EditSkewAxis == 0) CornerLocal.Y = StartLocal.Y;
			else CornerLocal.X = StartLocal.X;
		}

		if (SnappingActive())
		{
			const FVector CornerStart = EditStartTransform.TransformPosition(FVector(StartLocal.X, StartLocal.Y, 0.0));
			const FVector CornerFree = EditStartTransform.TransformPosition(FVector(CornerLocal.X, CornerLocal.Y, 0.0));
			const double SnapR = EffectiveSnapRadius(CornerFree);
			HutongSnap::FResult R;
			if (bEnds)
			{
				// Held to one line, the corner joins a neighbour where that line crosses the
				// neighbour's face — a mitre. Found from the corner's own position along the line,
				// so the cursor need not be anywhere near the face it is aiming for.
				const FVector AxisDir = EditStartTransform.TransformVector(EditSkewAxis == 0 ? FVector(1, 0, 0) : FVector(0, 1, 0)).GetSafeNormal2D();
				R = HutongSnap::FindSnapAlongLine(GetFootprints(), CornerStart, FVector2D(AxisDir.X, AxisDir.Y), CornerFree, 2.0 * SnapR, Owner);
			}
			else
			{
				// To a neighbour's corner or edge — but never to the very spot this corner started
				// on: a wall butted against another shares that corner with it, and snapping there
				// held the corner still however far the cursor went.
				R = HutongSnap::FindSnap(GetFootprints(), CornerFree, SnapR, Owner);
				if (R.bSnapped && FVector::Dist2D(R.Point, CornerStart) <= 2.0) R.bSnapped = false;
			}
			if (R.bSnapped)
			{
				const FVector Local = EditStartTransform.InverseTransformPosition(FVector(R.Point.X, R.Point.Y, CornerFree.Z));
				CornerLocal = FVector2D(Local.X, Local.Y);
			}
		}
		Candidate.Set(EditHandle, CornerLocal - Rect[EditHandle]);
		if (HutongFootprint::IsSkewValid(S, Candidate)) B->FootprintSkew = Candidate;
		break;
	}
	case EPlanEdit::RunVertex:
	{
		if (!EditRun.Vertices.IsValidIndex(EditRunVertex)) break;
		FVector G = Ground;
		HutongWallChain::FEndFace Face;
		HutongSnap::FResult SnapForSide;
		bool bPointSnapped = false;
		if (SnappingActive())
		{
			// To anything but the run's own legs, which move with the vertex.
			TArray<HutongSnap::FFootprint> Others;
			for (const HutongSnap::FFootprint& F : GetFootprints())
			{
				if (!EditRun.Contains(Cast<UHutongWallBuildingComponent>(F.Building.Get()))) Others.Add(F);
			}
			const HutongSnap::FResult R = HutongSnap::FindSnap(Others, G, EffectiveSnapRadius(G));
			if (R.bSnapped)
			{
				G = FVector(R.Point.X, R.Point.Y, G.Z);
				bPointSnapped = true;
				if (R.EdgeYawDeg > -900.0)
				{
					// Resting on that face: the end is cut flush along it, and slides along it.
					Face.bSet = true;
					Face.Point = FVector2D(G.X, G.Y);
					const double Yaw = FMath::DegreesToRadians(R.EdgeYawDeg);
					Face.Dir = FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw));
				}
			}
			SnapForSide = R;
		}
		// The leg's bearing is kept under Shift, or when the pull sits within a few pixels of
		// the line it was on: a wall lengthened along itself stays straight, and everything else
		// is free. An angular band was not free — on a long leg it was tens of centimetres wide.
		const int32 K = EditRunVertex;
		const int32 N = EditRun.Vertices.Num() - 1;
		if (!bPointSnapped)
		{
			const FVector2D Fixed = K > 0 ? EditRun.Vertices[K - 1] : EditRun.Vertices[1];
			const FVector2D Was = (K > 0 ? EditRun.Vertices[K] - Fixed : EditRun.Vertices[0] - Fixed).GetSafeNormal();
			const FVector2D Now = FVector2D(G.X, G.Y) - Fixed;
			if (!Was.IsNearlyZero())
			{
				const double OffLine = FMath::Abs(Now.X * Was.Y - Now.Y * Was.X);
				const double Band = FMath::Max(8.0, 4.0 * WorldPerPixelAt(G));
				if (IsSkewKeyDown() || OffLine <= Band)
				{
					const FVector2D P = Fixed + Was * FVector2D::DotProduct(Now, Was);
					G = FVector(P.X, P.Y, G.Z);
				}
			}
		}
		TArray<FVector2D> Vertices = EditRun.Vertices;
		Vertices[K] = FVector2D(G.X, G.Y);
		// A face running along the leg: the leg's outer face goes on it, the body on the
		// neighbour's side. A lone wall shifts whole and stays parallel; a leg in a run shifts
		// this end and keeps its far vertex with the run.
		if (SnapForSide.bSnapped)
		{
			const FVector2D Dir = (K > 0 ? Vertices[K] - Vertices[K - 1] : Vertices[1] - Vertices[0]).GetSafeNormal();
			double DrawnY = 0.0;
			const double Tolerance = bEditRunSideOn ? HutongWallChain::AlongFaceDegAfter : HutongWallChain::AlongFaceDeg;
			bEditRunSideOn = HutongWallChain::SideAlongFace(Dir, SnapForSide.EdgeYawDeg, SnapForSide.EdgeYaw2Deg, SnapForSide.Inward, EditRun.Thickness, DrawnY, Tolerance);
			if (bEditRunSideOn)
			{
				const FVector2D Shift = FVector2D(-Dir.Y, Dir.X) * (0.5 * EditRun.Thickness - DrawnY);
				Vertices[K] += Shift;
				if (N == 1) Vertices[K == 0 ? 1 : 0] += Shift;
			}
		}
		const HutongWallChain::FEndFace StartFace = (K == 0) ? Face : EditRun.StartFace;
		const HutongWallChain::FEndFace EndFace = (K == N) ? Face : EditRun.EndFace;
		TArray<HutongWallChain::FSegment> Segments;
		if (!HutongWallRun::Rebuild(EditRun, Vertices, StartFace, EndFace, Segments)) break;
		HutongWallRun::Apply(EditRun, Segments);
		for (const TWeakObjectPtr<UHutongWallBuildingComponent>& L : EditRun.Legs)
		{
			if (L.IsValid() && L->bPlanOnly) L->ApplyPlanOutline();
		}
		break;
	}
	case EPlanEdit::Opening:
	{
		// The cursor's distance along the run, in the actor's frame; the component clamps it so the opening stays inside the wall.
		const FVector Local = EditStartTransform.InverseTransformPosition(Ground);
		B->SetPlanOpeningCentre(EditHandle, B->IsRunAlongY() ? Local.Y : Local.X);
		break;
	}
	case EPlanEdit::Rotate:
	{
		const FVector2D S = EditStartSize;
		const FVector Centre = EditStartTransform.TransformPosition(FVector(0.5 * S.X, 0.5 * S.Y, 0.0));
		const double dx = Ground.X - Centre.X, dy = Ground.Y - Centre.Y;
		if (dx * dx + dy * dy < 1.0) return;
		double Yaw = EditStartYawDeg + (FMath::RadiansToDegrees(FMath::Atan2(dy, dx)) - EditStartCursorAngleDeg);

		const bool bFiveDegrees = FSlateApplication::IsInitialized()
			&& FSlateApplication::Get().GetModifierKeys().IsShiftDown();
		if (bFiveDegrees)
		{
			Yaw = FMath::RoundToDouble(Yaw / 5.0) * 5.0;
		}
		else if (SnappingActive() && Snap->bAdoptAngle)
		{
			// To the neighbours' bearings and quarter turns off them, as hold-R does.
			const TArray<double> Yaws = HutongSnap::GatherEdgeYaws(
				GetFootprints(), Centre, FMath::Max(Snap->Radius, 1.0) * 12.0, Owner);
			Yaw = HutongSnap::SnapYaw(Yaw, Yaws, FMath::Max(Snap->AngleToleranceDeg, 0.0));
		}
		const FRotator Rot(0.0, Yaw, 0.0);
		FVector Loc = Centre - Rot.RotateVector(FVector(0.5 * S.X, 0.5 * S.Y, 0.0));
		Loc.Z = Z;
		Owner->SetActorTransform(FTransform(Rot, Loc, EditStartTransform.GetScale3D()));
		break;
	}
	default:
		return;
	}
	// A built wall is not re-baked per frame: the slider is drawn from the params, and the mesh catches up on release.
	if (B->bPlanOnly) B->ApplyPlanOutline();
	UpdatePlacementReadout();
}

void URectDragToolBase::CaptureLegState(UHutongBuildingComponent* Building, FRunLegState& Out)
{
	Out.Building = Building;
	if (!Building || !Building->GetOwner()) return;
	Out.Transform = Building->GetOwner()->GetActorTransform();
	Out.Size = Building->GetFootprintSize();
	Out.Skew = Building->FootprintSkew;
	Out.bAlongY = Building->IsRunAlongY();
	if (const UHutongWallBuildingComponent* Wall = Cast<UHutongWallBuildingComponent>(Building))
	{
		Out.StartExtend = Wall->StartExtend;
		Out.EndExtend = Wall->EndExtend;
	}
}

void URectDragToolBase::RestoreLegState(const FRunLegState& State)
{
	UHutongBuildingComponent* Building = State.Building.Get();
	if (!Building || !Building->GetOwner()) return;
	Building->GetOwner()->SetActorTransform(State.Transform);
	Building->SetRunAlongY(State.bAlongY);
	Building->SetFootprintSize(State.Size);
	Building->FootprintSkew = State.Skew;
	if (UHutongWallBuildingComponent* Wall = Cast<UHutongWallBuildingComponent>(Building))
	{
		Wall->StartExtend = State.StartExtend;
		Wall->EndExtend = State.EndExtend;
	}
}

TArray<UHutongWallBuildingComponent*> URectDragToolBase::GatherWallLegs() const
{
	TArray<UHutongWallBuildingComponent*> Legs;
	for (const HutongSnap::FFootprint& F : GetFootprints())
	{
		if (UHutongWallBuildingComponent* Wall = Cast<UHutongWallBuildingComponent>(F.Building.Get())) Legs.Add(Wall);
	}
	return Legs;
}

void URectDragToolBase::CommitPlanEdit()
{
	// The building has moved, so every cached corner of it is stale.
	ON_SCOPE_EXIT { HutongSnap::Invalidate(); bHoverTraceValid = false; };

	if (PlanEdit == EPlanEdit::RunVertex)
	{
		// Every leg rewound and reapplied inside one transaction, as the single-building edit is.
		TArray<FRunLegState> Final;
		bool bChanged = false;
		for (const FRunLegState& Start : EditRunStart)
		{
			FRunLegState Now;
			CaptureLegState(Start.Building.Get(), Now);
			Final.Add(Now);
			bChanged = bChanged || !Now.Transform.Equals(Start.Transform, 1.0e-6) || !Now.Size.Equals(Start.Size, 1.0e-6)
				|| Now.Skew != Start.Skew || Now.bAlongY != Start.bAlongY;
		}
		if (!bChanged)
		{
			CancelPlanEdit();
			return;
		}
		for (const FRunLegState& Start : EditRunStart) RestoreLegState(Start);
		{
			const FScopedTransaction Transaction(LOCTEXT("EditWallRun", "Move Wall Run Vertex"));
			for (int32 i = 0; i < Final.Num(); ++i)
			{
				UHutongBuildingComponent* B = Final[i].Building.Get();
				AActor* Owner = B ? B->GetOwner() : nullptr;
				if (!Owner) continue;
				Owner->Modify();
				B->Modify();
				if (UActorComponent* Outline = Owner->FindComponentByClass<UHutongPlanOutlineComponent>()) Outline->Modify();
				RestoreLegState(Final[i]);
				B->Rebuild();
			}
		}
		PlanEdit = EPlanEdit::None;
		EditedPlan.Reset();
		EditHandle = INDEX_NONE;
		EditRun = HutongWallRun::FRun();
		EditRunStart.Reset();
		EditRunVertex = INDEX_NONE;
		UpdatePlacementReadout();
		return;
	}

	UHutongBuildingComponent* B = EditedPlan.Get();
	AActor* Owner = B ? B->GetOwner() : nullptr;
	if (B && Owner)
	{
		const FTransform Final = Owner->GetActorTransform();
		const FVector2D FinalSize = B->GetFootprintSize();
		const FHutongFootprintSkew FinalSkew = B->FootprintSkew;
		TArray<FHutongPlanOpening> FinalOpenings;
		B->GetPlanOpenings(FinalOpenings);
		const bool bOpening = PlanEdit == EPlanEdit::Opening && FinalOpenings.IsValidIndex(EditHandle);
		const double FinalCentre = bOpening ? FinalOpenings[EditHandle].Centre : 0.0;

		// A drag that moved the cursor and nothing else — a corner never pulled clear of its handle —
		// is a selection click, not an edit, and leaves no undo entry.
		const bool bUnchanged = Final.Equals(EditStartTransform, 1.0e-6) && FinalSize.Equals(EditStartSize, 1.0e-6)
			&& FinalSkew == EditStartSkew && (!bOpening || FMath::IsNearlyEqual(FinalCentre, EditStartOpeningCentre));
		if (bUnchanged)
		{
			CancelPlanEdit();
			return;
		}

		// Rewound and reapplied inside one transaction.
		Owner->SetActorTransform(EditStartTransform);
		B->SetFootprintSize(EditStartSize);
		B->FootprintSkew = EditStartSkew;
		if (bOpening) B->SetPlanOpeningCentre(EditHandle, EditStartOpeningCentre);
		{
			const FScopedTransaction Transaction(LOCTEXT("EditPlan", "Edit Laid-Out Building"));
			Owner->Modify();
			B->Modify();
			if (UActorComponent* Outline = Owner->FindComponentByClass<UHutongPlanOutlineComponent>())
			{
				Outline->Modify();
			}
			Owner->SetActorTransform(Final);
			B->SetFootprintSize(FinalSize);
			B->FootprintSkew = FinalSkew;
			if (bOpening) B->SetPlanOpeningCentre(EditHandle, FinalCentre);
			// The same seam a panel edit goes through.
			B->Rebuild();
		}
	}
	PlanEdit = EPlanEdit::None;
	EditedPlan.Reset();
	EditHandle = INDEX_NONE;
	UpdatePlacementReadout();
}

void URectDragToolBase::CancelPlanEdit()
{
	if (!IsEditingPlan()) return;
	if (PlanEdit == EPlanEdit::Marquee)
	{
		PlanEdit = EPlanEdit::None;
		EditedPlan.Reset();
		EditHandle = INDEX_NONE;
		return;
	}
	if (PlanEdit == EPlanEdit::RunVertex)
	{
		for (const FRunLegState& Start : EditRunStart)
		{
			RestoreLegState(Start);
			if (Start.Building.IsValid()) Start.Building->ApplyPlanOutline();
		}
		PlanEdit = EPlanEdit::None;
		EditedPlan.Reset();
		EditHandle = INDEX_NONE;
		EditRun = HutongWallRun::FRun();
		EditRunStart.Reset();
		EditRunVertex = INDEX_NONE;
		return;
	}
	UHutongBuildingComponent* B = EditedPlan.Get();
	AActor* Owner = B ? B->GetOwner() : nullptr;
	if (B && Owner)
	{
		Owner->SetActorTransform(EditStartTransform);
		B->SetFootprintSize(EditStartSize);
		B->FootprintSkew = EditStartSkew;
		if (PlanEdit == EPlanEdit::Opening) B->SetPlanOpeningCentre(EditHandle, EditStartOpeningCentre);
		B->ApplyPlanOutline();
	}
	PlanEdit = EPlanEdit::None;
	EditedPlan.Reset();
	EditHandle = INDEX_NONE;
}

void URectDragToolBase::DrawPlanHandles(FPrimitiveDrawInterface* PDI) const
{
	const UHutongBuildingComponent* B = GetSelectedBuilding();
	const AActor* Owner = B ? B->GetOwner() : nullptr;
	if (!B || !Owner || !PDI) return;

	const FTransform Xf = Owner->GetActorTransform();
	const FVector2D Size = B->GetFootprintSize();
	if (Size.X <= 0.0 || Size.Y <= 0.0) return;
	const double S = PlanHandleSize(Size);
	const FVector Lift(0.0, 0.0, 3.0);

	auto ColorFor = [&](int32 H, const FLinearColor& Base)
	{
		const bool bActive = IsEditingPlan()
			&& (((PlanEdit == EPlanEdit::Resize || PlanEdit == EPlanEdit::Skew || PlanEdit == EPlanEdit::RunVertex) && EditHandle == H)
				|| (PlanEdit == EPlanEdit::Rotate && H == PlanHandleRing)
				|| (PlanEdit == EPlanEdit::Opening && H == PlanHandleOpening + EditHandle));
		return (bActive || HoverPlanHandle == H) ? PlanActiveColor : Base;
	};
	auto Thick = [&](int32 H) { return (HoverPlanHandle == H || IsEditingPlan()) ? 3.5f : 2.5f; };

	const FVector AX = Xf.TransformVector(FVector(1, 0, 0)).GetSafeNormal2D();
	const FVector AY = Xf.TransformVector(FVector(0, 1, 0)).GetSafeNormal2D();

	// The opening sliders, on any building that has them.
	{
		TArray<FHutongPlanOpening> Openings;
		B->GetPlanOpenings(Openings);
		const FVector Run = B->IsRunAlongY() ? AY : AX;
		const FVector Across = B->IsRunAlongY() ? AX : AY;
		const double Thickness = B->IsRunAlongY() ? Size.X : Size.Y;
		for (int32 i = 0; i < Openings.Num(); ++i)
		{
			double Width;
			const FVector C = PlanOpeningWorld(B, i, Width) + Lift;
			const int32 H = PlanHandleOpening + i;
			const FLinearColor Col = ColorFor(H, PlanDoorColor);
			const float T = Thick(H);
			const FVector A = C - Run * (0.5 * Width), Bp = C + Run * (0.5 * Width);
			const FVector Half = Across * (0.5 * Thickness + 0.3 * S);
			DrawPreviewLine(PDI, A - Half, A + Half, Col, T);
			DrawPreviewLine(PDI, Bp - Half, Bp + Half, Col, T);
			DrawPreviewLine(PDI, A, Bp, Col, T + 1.0f);
			const double D = 0.5 * S;
			const FVector Q[4] = { C - Run * D, C - Across * D, C + Run * D, C + Across * D };
			for (int32 k = 0; k < 4; ++k) DrawPreviewLine(PDI, Q[k], Q[(k + 1) % 4], Col, T);
		}
	}
	// The divide markers, a diamond on each interior bay line, and the fuse markers, a bowtie on
	// an end a like neighbour stands against. The line under the hovered marker is drawn across.
	if (HutongDetailOps::CanDivide(B))
	{
		const bool bAlongX = B->ArePlanBaysAlongX();
		const FVector Run = bAlongX ? AX : AY;
		const FVector Across = bAlongX ? AY : AX;
		const double Depth = bAlongX ? Size.Y : Size.X;
		const double RunLen = bAlongX ? Size.X : Size.Y;
		const double D = 0.4 * S;
		FHutongPlanBays Bays;
		B->GetPlanBays(Bays);
		Bays.Boundaries.Sort();
		for (int32 i = 1; i + 1 < Bays.Boundaries.Num(); ++i)
		{
			const int32 H = PlanHandleBay + i;
			const FVector C = PlanBayMarkerWorld(B, Bays.Boundaries[i]) + Lift;
			const FLinearColor Col = ColorFor(H, PlanDivideColor);
			const float T = Thick(H);
			const FVector Q[4] = { C - Run * D, C - Across * D, C + Run * D, C + Across * D };
			for (int32 k = 0; k < 4; ++k) DrawPreviewLine(PDI, Q[k], Q[(k + 1) % 4], Col, T);
			if (HoverPlanHandle == H)
			{
				DrawPreviewLine(PDI, C - Across * (0.5 * Depth + 0.5 * S), C + Across * (0.5 * Depth + 0.5 * S), Col, T);
			}
		}
		for (int32 End = 0; End < 2; ++End)
		{
			const bool bAtEnd = End == 1;
			if (!FindFuseNeighbour(B, bAtEnd)) continue;
			const int32 H = bAtEnd ? PlanHandleFuseEnd : PlanHandleFuseStart;
			const FVector C = PlanBayMarkerWorld(B, bAtEnd ? RunLen : 0.0) + Lift;
			const FLinearColor Col = ColorFor(H, PlanFuseColor);
			const float T = Thick(H);
			// Two triangles meeting at the join, pointing into each building.
			const FVector L1 = C - Run * D - Across * D, L2 = C - Run * D + Across * D;
			const FVector R1 = C + Run * D - Across * D, R2 = C + Run * D + Across * D;
			DrawPreviewLine(PDI, C, L1, Col, T); DrawPreviewLine(PDI, L1, L2, Col, T); DrawPreviewLine(PDI, L2, C, Col, T);
			DrawPreviewLine(PDI, C, R1, Col, T); DrawPreviewLine(PDI, R1, R2, Col, T); DrawPreviewLine(PDI, R2, C, Col, T);
			if (HoverPlanHandle == H)
			{
				DrawPreviewLine(PDI, C - Across * (0.5 * Depth + 0.5 * S), C + Across * (0.5 * Depth + 0.5 * S), Col, T);
			}
		}
	}
	FVector2D Quad[4];
	B->GetFootprintCorners(Quad);
	// Shift: the corner handles mean "angle this corner", and say so by colour. On a built
	// building they appear only then, since that is the only edit it offers; on a run they appear
	// only then too, small enough to sit inside its thickness, since a run's plain handles are its ends.
	const bool bSkewMode = IsSkewKeyDown() || PlanEdit == EPlanEdit::Skew;
	if (!B->bPlanOnly && !bSkewMode) return;
	const double CornerR = 0.5 * PlanCornerHandleSize(Size);

	// Corners: a square with a cross in it, turned with the footprint.
	for (int32 H = 0; H < 8; ++H)
	{
		const bool bCorner = H < 4;
		if (!PlanHandleEnabled(Size, H) && !(bCorner && bSkewMode)) continue;
		if (!B->bPlanOnly && !bCorner) continue;
		const FVector P = Xf.TransformPosition(PlanHandleLocal(Quad, H)) + Lift;
		const FLinearColor C = ColorFor(H, bCorner ? (bSkewMode ? PlanSkewColor : PlanCornerColor) : PlanEdgeColor);
		const float T = Thick(H);
		const double R = bCorner && bSkewMode ? CornerR : 0.5 * S;
		if (bCorner)
		{
			const FVector Q[4] = { P - AX * R - AY * R, P + AX * R - AY * R, P + AX * R + AY * R, P - AX * R + AY * R };
			for (int32 i = 0; i < 4; ++i) DrawPreviewLine(PDI, Q[i], Q[(i + 1) % 4], C, T);
			DrawPreviewLine(PDI, P - AX * R, P + AX * R, C, T);
			DrawPreviewLine(PDI, P - AY * R, P + AY * R, C, T);
		}
		else
		{
			const double D = 0.8 * R;
			const FVector Q[4] = { P - AX * D, P - AY * D, P + AX * D, P + AY * D };
			for (int32 i = 0; i < 4; ++i) DrawPreviewLine(PDI, Q[i], Q[(i + 1) % 4], C, T);
		}
	}

	// The line the dragged corner is held to, through it and past the footprint both ways.
	if (PlanEdit == EPlanEdit::Skew && EditedPlan.Get() == B && EditSkewAxis != INDEX_NONE && EditHandle >= 0 && EditHandle < 4)
	{
		const FVector P = Xf.TransformPosition(PlanHandleLocal(Quad, EditHandle)) + Lift;
		const FVector Dir = EditSkewAxis == 0 ? AX : AY;
		const double Reach = 0.5 * (EditSkewAxis == 0 ? Size.X : Size.Y) + 4.0 * S;
		DrawPreviewLine(PDI, P - Dir * Reach, P + Dir * Reach, PlanSkewColor, 1.5f);
	}

	if (!B->bPlanOnly) return;

	// The rotate ring: a circle standing off the front, tied to the edge by a line, with a short arc arrow inside it so it reads as a turn and not a fifth kind of handle.
	FVector EdgeMid;
	const FVector Ring = PlanRotateHandleWorld(B, EdgeMid) + Lift;
	const FLinearColor RC = ColorFor(PlanHandleRing, PlanRotateColor);
	const float RT = Thick(PlanHandleRing);
	DrawPreviewLine(PDI, EdgeMid + Lift, Ring, RC, 2.0f);
	const double R = 0.6 * S;
	constexpr int32 Segments = 24;
	for (int32 i = 0; i < Segments; ++i)
	{
		const double A0 = 2.0 * PI * i / Segments, A1 = 2.0 * PI * (i + 1) / Segments;
		DrawPreviewLine(PDI,
			Ring + FVector(R * FMath::Cos(A0), R * FMath::Sin(A0), 0.0),
			Ring + FVector(R * FMath::Cos(A1), R * FMath::Sin(A1), 0.0), RC, RT);
	}
	// Three quarters of an inner arc, ending in an arrowhead.
	const double Ri = 0.55 * R;
	constexpr int32 ArcSegments = 12;
	for (int32 i = 0; i < ArcSegments; ++i)
	{
		const double A0 = 1.5 * PI * i / ArcSegments, A1 = 1.5 * PI * (i + 1) / ArcSegments;
		DrawPreviewLine(PDI,
			Ring + FVector(Ri * FMath::Cos(A0), Ri * FMath::Sin(A0), 0.0),
			Ring + FVector(Ri * FMath::Cos(A1), Ri * FMath::Sin(A1), 0.0), RC, RT);
	}
	const FVector Tip = Ring + FVector(Ri * FMath::Cos(1.5 * PI), Ri * FMath::Sin(1.5 * PI), 0.0);
	DrawPreviewLine(PDI, Tip, Tip + FVector(-0.35 * Ri, -0.35 * Ri, 0.0), RC, RT);
	DrawPreviewLine(PDI, Tip, Tip + FVector(-0.35 * Ri, 0.35 * Ri, 0.0), RC, RT);
}

#undef LOCTEXT_NAMESPACE
