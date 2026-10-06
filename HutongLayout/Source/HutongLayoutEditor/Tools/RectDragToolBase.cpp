#include "Tools/RectDragToolBase.h"
#include "Tools/HutongContextMenu.h"
#include "EngineUtils.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
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
	// Cursor travel before a plan press becomes a drag, not a click.
	constexpr double PlanClickSlopPixels = 4.0;
	// Open-ground box needs a real drag: trackpad clicks wander a few pixels and made empty boxes.
	constexpr double MarqueeSlopPixels = 12.0;

	// Ortho cursor ray starts on the view plane (Z = 0 in top view) with the scene behind it; pull
	// the origin back 10 km, inside the trace length.
	constexpr double OrthoRayBackOff = 1000000.0;

	bool IsActiveViewportOrtho()
	{
		FViewport* Viewport = GEditor ? GEditor->GetActiveViewport() : nullptr;
		const FEditorViewportClient* Client = Viewport
			? static_cast<const FEditorViewportClient*>(Viewport->GetClient()) : nullptr;
		return Client && Client->IsOrtho();
	}
}

// Snap-bypass key name, for prompts.
FText SnapKeyName();

void URectDragToolBase::Setup()
{
	UInteractiveTool::Setup();

	ClickBehavior = NewObject<USingleClickInputBehavior>(this);
	ClickBehavior->Initialize(this);
	AddInputBehavior(ClickBehavior);

	// Ahead of the click: claims presses over a laid-out building, declines the rest so the click
	// still places.
	DragBehavior = NewObject<UClickDragInputBehavior>(this);
	DragBehavior->Initialize(this);
	DragBehavior->SetDefaultPriority(FInputCapturePriority(FInputCapturePriority::DEFAULT_TOOL_PRIORITY - 1));
	// Shift registered, not just read at press: the editor's click-selection lists Shift as
	// optional and is promoted past a plain drag when it is held, stealing the press. A registered
	// modifier promotes this behaviour equally; it only claims presses on selected-building
	// handles.
	DragBehavior->Modifiers.RegisterModifier(SkewModifierID, FInputDeviceState::IsShiftKeyDown);
	AddInputBehavior(DragBehavior);

	HoverBehavior = NewObject<UMouseHoverBehavior>(this);
	HoverBehavior->Initialize(this);
	AddInputBehavior(HoverBehavior);

	// Panel order is registration order.

	// Metadata first: confidence and note belong to the sitting and are set before drawing.
	MetadataSettings = NewObject<UHutongMetadataProperties>(this);
	RegisterSettings(MetadataSettings);

	// Then the tool's own sets, preset picker first.
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

int32 URectDragToolBase::BuildLODsForRect(double SizeX, double SizeY, TArray<FDynamicMesh3>& OutLODs)
{
	return HutongGen::Detail::BuildPlacementLODs(IsPlanOnly() || !BuildsGeometry(), SizeX, SizeY,
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
	Building->bPlanOnly = IsPlanOnly() || !Building->HasGeometry();
	// Record which preset laid the building: 正房 and 耳房 share a generator, so params alone cannot
	// name it. Empty when none was picked.
	// Only when the picker picks this type: compound and gallery stamp pieces through here with a
	// compound picker. The tool's save key matching the component's class name confirms the name
	// fits.
	if (PresetSettings && PresetSettings->GetToolKey() == Building->GetPresetKey())
	{
		Building->Preset = PresetSettings->Preset;
	}
}

// Property cache keys by set class and every preset picker is one class, so each needs its own id
// or one tool's choice restores onto another's. Other sets (detail level, palette) are shared on
// purpose.
FString URectDragToolBase::CacheIdentifierFor(const UInteractiveToolPropertySet* PropertySet) const
{
	return PropertySet->IsA<UHutongPresetProperties>() ? GetClass()->GetName() : FString();
}

void URectDragToolBase::ApplyDefaultPreset(UHutongPresetProperties* Picker, const FString& Name)
{
	if (!Picker || !Picker->Preset.IsEmpty()) return;
	Picker->Preset = Name;
	Picker->LoadSelectedPreset();
}

void URectDragToolBase::RegisterSettings(UInteractiveToolPropertySet* PropertySet, bool bPersist)
{
	if (!PropertySet) return;
	if (bPersist)
	{
		PropertySet->RestoreProperties(this, CacheIdentifierFor(PropertySet));
		RegisteredSettings.Add(PropertySet);
	}
	// Detected here so every tool with a picker gets it through this one call.
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
	// Height is a tool setting, so it stays shown between placements.
	const double NewHeight = GetPreviewHeight();

	if (FMath::IsNearlyEqual(NewWidth, Placement->Width)
		&& FMath::IsNearlyEqual(NewDepth, Placement->Depth)
		&& FMath::IsNearlyEqual(NewHeight, Placement->Height)
		&& FMath::IsNearlyEqual(NewRotation, Placement->Rotation)
		&& NewDetail == Placement->Detail)
	{
		return;
	}

	// No NotifyOfPropertyChangeByTool: it rebuilds the whole details panel, and this runs per hover
	// tick.
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
		if (Set) Set->SaveProperties(this, CacheIdentifierFor(Set));
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
	// Cursor is at whichever end the drag reached; the anchor is at zero.
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

	// Only while the mouse is over the viewport.
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
		// Idle with the key up ends the last placement's suppression; an in-flight placement or
		// edit keeps it, so the key can be released before clicking.
		if (!IsEditingPlan() && !IsSnapKeyDown()) bSnapKeyLatched = false;
		UpdateHoverInspectionFromViewport();
		DrawSelectedFootprints(PDI);
		DrawHoverInspection(PDI);
		FVector Ground;
		const bool bGround = GetViewportCursorGround(Ground);
		// Hovered handle, highlighted before the press.
		HoverPlanHandle = INDEX_NONE;
		if (bGround && !IsEditingPlan())
		{
			if (const UHutongBuildingComponent* Selected = GetSelectedBuilding())
			{
				HoverPlanHandle = HitTestPlan(Selected, Ground);
				if (PressStartsPlacement(Ground, Selected, HoverPlanHandle)) HoverPlanHandle = INDEX_NONE;
			}
		}
		DrawPlanHandles(PDI);
		if (PlanEdit == EPlanEdit::Marquee && bPlanDragMoved)
		{
			// Ground box, in the selection's blue.
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
	if (DrawsRectFootprint())
	{
		DrawPreviewLine(PDI, C0, C1, Color, Thickness);
		DrawPreviewLine(PDI, C1, C2, Color, Thickness);
		DrawPreviewLine(PDI, C2, C3, Color, Thickness);
		DrawPreviewLine(PDI, C3, C0, Color, Thickness);
	}

	// Snap marker: small cross on the locked point.
	if (bSnapActive)
	{
		const FLinearColor SnapColor(0.25f, 1.0f, 0.45f, 1.0f);
		const double Arm = 26.0;
		DrawPreviewLine(PDI, SnapPoint - FVector(Arm, 0, 0), SnapPoint + FVector(Arm, 0, 0), SnapColor, 4.0f);
		DrawPreviewLine(PDI, SnapPoint - FVector(0, Arm, 0), SnapPoint + FVector(0, Arm, 0), SnapColor, 4.0f);
		DrawPreviewLine(PDI, SnapPoint, SnapPoint + FVector(0, 0, 1.6 * Arm), SnapColor, 3.0f);
	}

	// Corner posts and top outline show the height in perspective.
	const double PreviewHeight = GetPreviewHeight();
	if (PreviewHeight <= 0.0 || !DrawsRectFootprint()) return;

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

// One task per line, naming only settings the current view shows: students skimmed the paragraphs.
// Tools replace line 0 and insert their own from line 1.
TArray<FText> URectDragToolBase::GetToolHelpLines() const
{
	TArray<FText> Lines = {
		LOCTEXT("HelpAnchor", "Click the ground to anchor a corner, move, then click to set the footprint."),
		LOCTEXT("HelpCancel", "Esc cancels the placement; Esc again puts the tool down. Ctrl+Z undoes the last placement."),
		LOCTEXT("HelpHeight", "- and = raise or lower the height (Shift finer, Ctrl coarser)."),
		FText::Format(LOCTEXT("HelpSnap", "Placements snap to placed buildings. Tap {0} before clicking to place freely."), SnapKeyName()),
		LOCTEXT("HelpEdge", "Click on a footprint's edge to start a new one against it; Ctrl+click to place inside it."),
		LOCTEXT("HelpSelect", "Drag a box on open ground to select buildings (Shift adds). Drag a selected footprint to move it, its handles to resize, its ring to rotate."),
		LOCTEXT("HelpFacade", "With buildings selected: [ and ] remove or add a bay, Shift+[ and ] turn the facade, F flips it, G toggles a wall's gate."),
		LOCTEXT("HelpCycle", "With buildings selected: P opens a preset menu, T a type menu. Shift+P and Shift+T step to the next; Enter applies."),
		LOCTEXT("HelpScene", "The Scene tab generates, converts, exports, imports and changes the detail of placed buildings."),
	};
	if (HasRotateKey())
	{
		Lines.Insert(LOCTEXT("HelpRotate", "Hold R while placing and move the mouse to rotate; Shift snaps to 5°."), 3);
	}
	const UHutongLayoutModeSettings* Settings = UHutongLayoutEdMode::GetActiveSettings();
	if (Settings && Settings->bShowAdvancedSettings)
	{
		Lines.Add(LOCTEXT("HelpDetail", "Detail Level: Block (塊) massing only, Far (遠) no ornament, Near (近) full building."));
		Lines.Add(LOCTEXT("HelpAngle", "Adopt Neighbour Angle turns a snapped placement onto its neighbour's line."));
	}
	return Lines;
}

// Only the keys that act at this stage; the prompt above says what to click.
FText URectDragToolBase::GetKeyHintText() const
{
	const FText SnapHint = FText::Format(LOCTEXT("KeyHintSnapKey", "{0} {1}"), SnapKeyName(),
		SnappingActive() ? LOCTEXT("KeyHintNoSnap", "no snap") : LOCTEXT("KeyHintSnap", "snap"));
	if (IsPlacingActive() || IsEditingPlan())
	{
		return FText::Format(LOCTEXT("KeyHintPlacing", "- = height · R rotate · {0} · Esc cancel"), SnapHint);
	}
	if (HutongDetailOps::CollectSelected().Num() > 0)
	{
		return FText::Format(LOCTEXT("KeyHintIdleSelected", "- = height · {0} · P preset · T type · Esc put the tool down"), SnapHint);
	}
	return FText::Format(LOCTEXT("KeyHintIdle", "- = height · {0} · Esc put the tool down"), SnapHint);
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
	// The key inverts the snap setting; named once so the prompts agree.
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
		return FText::Format(LOCTEXT("PromptPlanResize", "Resizing: release to set.{0} Esc puts it back."), Free);
	case EPlanEdit::Rotate:
		// The key's clause is about position snapping; nothing is positioned here.
		return LOCTEXT("PromptPlanRotate", "Rotating: release to set. Shift snaps to 5°. Esc puts it back.");
	case EPlanEdit::Opening:
		return LOCTEXT("PromptPlanOpening", "Sliding the doorway: release to set. Esc puts it back.");
	case EPlanEdit::Marquee:
		return LOCTEXT("PromptPlanMarquee", "Selecting: release to select. Shift adds.");
	case EPlanEdit::RunVertex:
		return FText::Format(LOCTEXT("PromptPlanRunVertex", "Moving the wall end: release to set. Shift keeps the leg's bearing.{0} Esc puts it back."), Free);
	case EPlanEdit::Skew:
	{
		const UHutongBuildingComponent* B = EditedPlan.Get();
		const bool bEnds = !B || B->FootprintSkew.Mode == EHutongSkewMode::Ends;
		return bEnds
			? FText::Format(LOCTEXT("PromptPlanSkewEnds", "Moving the corner (端斜) along or across the run: release to set.{0} Esc puts it back."), Free)
			: FText::Format(LOCTEXT("PromptPlanSkewWhole", "Angling the corner (斜角): release to set.{0} Esc puts it back."), Free);
	}
	default:
		break;
	}
	if (HoverPlanHandle >= PlanHandleBay)
	{
		return LOCTEXT("PromptDivideMarker", "Click to divide the building at this bay line (分間).");
	}
	if (HoverPlanHandle == PlanHandleFuseStart || HoverPlanHandle == PlanHandleFuseEnd)
	{
		return LOCTEXT("PromptFuseMarker", "Click to fuse with the building end to end on this line (合併).");
	}
	if (const UHutongBuildingComponent* Selected = GetSelectedBuilding())
	{
		TArray<FHutongPlanOpening> Openings;
		Selected->GetPlanOpenings(Openings);
		if (!Selected->bPlanOnly && Openings.Num() > 0)
		{
			return LOCTEXT("PromptWallSelected", "Wall selected: drag the doorway marker to slide it, Shift-drag a corner to cut the end (斜角), G toggles the gate. Click open ground to place.");
		}
		if (!Selected->bPlanOnly)
		{
			return LOCTEXT("PromptBuiltSelected", "Building selected: Shift-drag a corner to angle it (斜角), [ ] change bays, Shift+[ ] turns the facade. Click open ground to place.");
		}
	}
	if (GetSelectedPlanBuilding())
	{
		return LOCTEXT("PromptPlanSelectedSkew", "Building selected: drag a handle to resize (Ctrl moves this one alone), Shift-drag a corner to angle it (斜角), the ring to rotate, the inside to move, a doorway marker to slide it; [ ] change bays, Shift+[ ] turns the facade, G toggles a wall's gate. Click open ground to place.");
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
		return LOCTEXT("PromptRotate", "Rotating: move the mouse to turn the footprint, Shift snaps to 5°. Release R to keep it.");
	}
	if (!bRectCommitted)
	{
		return LOCTEXT("PromptSize", "Move to size the footprint, then click to place.");
	}
	return LOCTEXT("PromptCommit", "Click to place. Esc to cancel.");
}

void URectDragToolBase::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	// Under every readout drawn after it.
	DrawBuildingLabels(Canvas, RenderAPI);
	if (!bIsDragging)
	{
		DrawHoverInspectionHUD(Canvas, RenderAPI);
		DrawPendingCycleBanner(Canvas);
		return;
	}
	if (Canvas == nullptr || RenderAPI == nullptr) return;

	// Readout pinned top-left, not beside the rect: there it covered the geometry, ran off-screen
	// and vanished when WorldToPixel failed. Wrapped to the viewport width.
	// Engine bitmap font: a Slate font draws nothing in this canvas pass. The prompts' Chinese is
	// lost.
	const FVector2D At = HudCorner(Canvas);
	const UFont* Font = UEngine::GetMediumFont();
	if (Font == nullptr) return;
	// Wrap short of the measured width: the bitmap font draws wider than it measures.
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
	// Key hints, same line as the panel.
	Add(GetKeyHintText(), FLinearColor(0.78f, 0.78f, 0.78f));

	// Dark backing, legible over a map.
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
			// A word wider than the line (Chinese has no spaces) breaks mid-word.
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
	// Clear of the viewport toolbar; DPI-independent units.
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

	// Margin keeps text off the edge.
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

void URectDragToolBase::DrawBaysAndFacing(FPrimitiveDrawInterface* PDI, TFunctionRef<FVector(double, double)> At,
	double Span, double Across, const TArray<double>& Bounds, int32 DoorBay)
{
	if (!PDI || Span < 1.0 || Across < 1.0) return;
	TArray<double> B = Bounds.Num() >= 2 ? Bounds : TArray<double>{ 0.0, Span };
	B.Sort();
	const FLinearColor Green(0.25f, 1.0f, 0.45f);
	const FLinearColor Orange(1.0f, 0.55f, 0.15f);
	DrawPreviewLine(PDI, At(0.0, 0.0), At(Span, 0.0), Green, 7.0f);
	for (int32 i = 1; i + 1 < B.Num(); ++i) DrawPreviewLine(PDI, At(B[i], 0.0), At(B[i], Across), Green, 3.0f);
	const double TipIn = FMath::Min(0.15 * Across, 60.0);
	for (int32 i = 0; i + 1 < B.Num(); ++i)
	{
		const double Width = B[i + 1] - B[i];
		const double Half = FMath::Min(FMath::Clamp(0.3 * Width, 20.0, 150.0), 0.4 * (Across - TipIn));
		if (Half <= 0.0) continue;
		const double Mid = 0.5 * (B[i] + B[i + 1]);
		DrawPreviewLine(PDI, At(Mid - Half, TipIn + Half), At(Mid, TipIn), Green, 5.0f);
		DrawPreviewLine(PDI, At(Mid, TipIn), At(Mid + Half, TipIn + Half), Green, 5.0f);
	}
	if (B.IsValidIndex(DoorBay) && B.IsValidIndex(DoorBay + 1))
	{
		DrawPreviewLine(PDI, At(B[DoorBay], 0.0), At(B[DoorBay + 1], 0.0), Orange, 11.0f);
	}
}

void URectDragToolBase::DrawRectBaysAndFacing(FPrimitiveDrawInterface* PDI, EHutongBaySide Side, double MinX, double MinY,
	double MaxX, double MaxY, const TArray<double>& Bounds, int32 DoorBay) const
{
	const HutongGen::BaySide::FEdge Edge = HutongGen::BaySide::GetEdge((HutongGen::EBaySide)Side, MinX, MinY, MaxX, MaxY);
	const double SpanMin = Edge.bAlongX ? MinX : MinY;
	const double Span = Edge.bAlongX ? MaxX - MinX : MaxY - MinY;
	const double Across = Edge.bAlongX ? MaxY - MinY : MaxX - MinX;
	const double InSign = Edge.bAlongX ? -Edge.OutDir.Y : -Edge.OutDir.X;
	DrawBaysAndFacing(PDI, [&](double Along, double Depth)
	{
		const double Fixed = Edge.FixedCoord + InSign * Depth;
		return Edge.bAlongX ? LocalRectToWorld(SpanMin + Along, Fixed) : LocalRectToWorld(Fixed, SpanMin + Along);
	}, Span, Across, Bounds, DoorBay);
}

void URectDragToolBase::DrawPreviewLine(FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B,
	const FLinearColor& Color, float Thickness)
{
	// Thickness in screen pixels, not cm.
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

	// Odd number of half-periods: a dash lands on both ends.
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
	// Perspective: plane behind the origin is behind the camera, a miss. Ortho top view: the origin
	// lies on this plane so t <= 0 everywhere, not a miss.
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
	// A click on a built building is the editor's selection, not a new placement's first corner. A
	// placement in hand keeps its finishing click anywhere. A tool that places nothing leaves every
	// click to the editor's selection.
	if (!HasPlacement()) return FInputRayHit();
	if (!bIsDragging)
	{
		FHitResult Hit;
		FVector Ground;
		if (TraceHoveredBuilding(ClickPos, Hit)
			&& !(TryRayHitGround(ClickPos, Ground) && PressStartsPlacement(Ground, GetSelectedBuilding(), INDEX_NONE)))
		{
			return FInputRayHit();
		}
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
	// Alt (Option on Mac) or Command.
	const FModifierKeysState& Mods = FSlateApplication::Get().GetModifierKeys();
	return Mods.IsAltDown() || Mods.IsCommandDown();
}

bool URectDragToolBase::IsSnapKeyLatched() const
{
	if (IsSnapKeyDown())
	{
		// Latched so an in-flight placement keeps it after release; the finishing click arrives
		// with the key up.
		bSnapKeyLatched = true;
		return true;
	}
	return bSnapKeyLatched;
}

bool URectDragToolBase::SnappingActive() const
{
	// One comparison, so setting and key are read consistently. The key inverts the checkbox, so a
	// prompt names one action.
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

	// Reset this end's last answer first: a stale bearing makes MiterExtend miter to a missing
	// neighbour.
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

	// Lane width first, anchor only: the start sets the run's position across the street.
	if (bIsAnchor && Snap->bSnapLaneWidth && WantsLaneWidthSnap() && !PointSnapsOnly())
	{
		// No bearing: at the first click the run's direction is unknown and PlacementYawDeg is
		// stale.
		const HutongSnap::FGap Gap = HutongSnap::FindParallelGap(
			GetFootprints(), World, HutongSnap::AnyYaw, 2.0 * HutongGen::Urban::PitchCm);
		if (Gap.bFound)
		{
			// Face to face, as the readout reports and a street is measured.
			const double Offset = GetLaneFaceOffset();
			const double Clear = FMath::Max(Gap.DistanceCm - Offset, 0.0);
			const double Canonical = HutongGen::Urban::NearestCanonicalWidth(
				Clear, FMath::Max(Snap->LaneToleranceCm, 0.0));
			if (Canonical > 0.0)
			{
				// Slide along the line to the neighbour so only the distance changes.
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

	// The cursor end keeps its held snap while near, unless the corner at the end of a held face
	// comes in reach: that corner (both bearings) beats the face. A held corner is kept, or an end
	// on a thin end face flips between its corners per pixel.
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
			// Edge: line through the point, held within the radius across and a little along.
			const double Yaw = FMath::DegreesToRadians(S.EdgeYawDeg);
			const FVector2D E(FMath::Cos(Yaw), FMath::Sin(Yaw));
			const FVector2D D(World.X - S.Point.X, World.Y - S.Point.Y);
			const double Along = FVector2D::DotProduct(D, E);
			const double Across = FMath::Abs(D.X * E.Y - D.Y * E.X);
			const FVector OnLine(S.Point.X + E.X * Along, S.Point.Y + E.Y * Along, World.Z);
			// Held only while on the face itself: past its corner the line is nothing anyone can see.
			if (Across <= 1.5 * Radius && FMath::Abs(Along) <= 6.0 * Radius && IsOnOutline(OnLine, S.Building.Get()))
			{
				bKeep = true;
				Held = OnLine;
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

	// Anchor takes the neighbour's angle; only the anchor end gets a miter from it.
	if (bIsAnchor)
	{
		AnchorSnapInward = R.Inward;
		AnchorSnapYawDeg = R.EdgeYawDeg;
		AnchorSnapYaw2Deg = R.EdgeYaw2Deg;
		AnchorSnapBuilding = R.Building;
		// Starting on a neighbour starts in its frame: its rotation, the quarter turn nearest the
		// current one, so the drag's axes run with it. Only here, at the click: hold R turns it after.
		const AActor* Neighbour = R.Building.IsValid() ? R.Building->GetOwner() : nullptr;
		if (Neighbour)
		{
			PlacementYawDeg = HutongSnap::SnapYaw(PlacementYawDeg, { Neighbour->GetActorRotation().Yaw }, 46.0);
		}
		else if (Snap->bAdoptAngle && R.EdgeYawDeg > -900.0)
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

	// Collinear: no miter.
	if (Delta < 1.0) return 0.0;

	const double HalfInterior = 0.5 * (180.0 - Delta);
	const double Tan = FMath::Tan(FMath::DegreesToRadians(HalfInterior));
	if (Tan < UE_KINDA_SMALL_NUMBER) return 0.0;

	return FMath::Clamp((0.5 * Thickness) / Tan, 0.0, 0.5 * Thickness);
}

void URectDragToolBase::ProcessClick(const FVector& Hit)
{
	if (!HasPlacement()) return;
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
			// The just-placed building must be snappable on the next click.
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

	// A ray aimed at a building from near eye level never reaches Z = 0.
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
	// The scene lies behind an ortho ray's origin; start the trace further back.
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
		// Meshes are built from the actor origin; the footprint is the placement's own corners (not
		// the rect under skew).
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
	// No age-out while the mouse is down: a drag cannot change the level, and a refresh walks the
	// whole district.
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

	// Still cursor: skip the trace, keep HoveredBuilding.
	const FIntPoint CursorPx(Viewport->GetMouseX(), Viewport->GetMouseY());
	if (bHoverTraceValid && CursorPx == LastHoverCursorPx) return;
	LastHoverCursorPx = CursorPx;
	bHoverTraceValid = true;
	HoveredBuilding.Reset();

	// Editor cursor, independent of a live hover sequence.
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

FText URectDragToolBase::GetHoverSummaryText(bool bWithPending) const
{
	// Hovered, else selected: the readout stays when the cursor moves onto a handle.
	const UHutongBuildingComponent* Building = HudBuilding.Get();
	if (Building == nullptr) Building = GetSelectedBuilding();
	const AActor* Actor = Building ? Building->GetOwner() : nullptr;
	if (Actor == nullptr) return FText::GetEmpty();
	// A pending P / T change heads the readout, so the panel and HUD both carry it.
	const FText PendingText = bWithPending ? GetPendingCycleText() : FText::GetEmpty();
	const FString PendingLine = PendingText.IsEmpty() ? FString() : PendingText.ToString() + TEXT("\n");

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

	// Type plus preset: 正房 and 耳房 share a generator; the preset tells them apart.
	const FString What = Building->Preset.IsEmpty()
		? FString::Printf(TEXT("%s  ·  no preset"), *Building->GetTypeLabel().ToString())
		: FString::Printf(TEXT("%s  ·  %s"), *Building->GetTypeLabel().ToString(), *Building->Preset);

	FHutongPlanBays Bays;
	Building->GetPlanBays(Bays);
	const FString BayText = (Bays.Boundaries.Num() >= 2)
		? FString::Printf(TEXT("  ·  %d bays (間)"), Bays.Boundaries.Num() - 1)
		: FString();

	// Plan-only has no mesh; saying so explains the empty ground.
	const FString Built = Building->bPlanOnly
		? FString(TEXT("plan only"))
		: FString::Printf(TEXT("%d tris"), Triangles);
	const FString DetailText =
		StaticEnum<EHutongDetail>()->GetDisplayNameTextByValue((int64)Building->DetailLevel).ToString();

	// Confidence: traced and guessed look alike. The note follows, cut to one line; Details shows
	// it whole.
	const FString Confidence = FString::Printf(TEXT("confidence %s"),
		*StaticEnum<EHutongConfidence>()->GetDisplayNameTextByValue((int64)Building->Confidence).ToString());
	FString NoteText = Building->Notes.TrimStartAndEnd().Replace(TEXT("\n"), TEXT(" "));
	if (NoteText.Len() > 64) NoteText = NoteText.Left(61) + TEXT("...");
	const FString NoteLine = NoteText.IsEmpty() ? FString() : FString::Printf(TEXT("\n%s"), *NoteText);

	const FString SkewText = Building->HasFootprintSkew() ? FString(TEXT("  ·  skewed (斜角)")) : FString();

	return FText::FromString(FString::Printf(
		TEXT("%s%s\n%.0f x %.0f cm%s%s  ·  %.0f°\n%s  ·  %s  ·  %s\n%s%s\nat %.0f, %.0f, %.0f cm"),
		*PendingLine, *What, Size.X, Size.Y, *BayText, *SkewText, Actor->GetActorRotation().Yaw,
		*Built, *DetailText, *Actor->GetActorLabel(),
		*Confidence, *NoteLine,
		Loc.X, Loc.Y, Loc.Z));
}

void URectDragToolBase::DrawSelectedFootprints(FPrimitiveDrawInterface* PDI) const
{
	// A laid-out building's outline draws its own selection; a built one's footprint is under its
	// eaves and the editor's outline follows the roof, so it is drawn here, over the geometry.
	if (PDI == nullptr) return;
	for (const UHutongBuildingComponent* B : HutongDetailOps::CollectSelected())
	{
		const AActor* Actor = B->GetOwner();
		if (!Actor || B->bPlanOnly) continue;
		FVector2D Quad[4];
		B->GetFootprintCorners(Quad);
		const FTransform Xform = Actor->GetActorTransform();
		FVector W[4];
		for (int32 i = 0; i < 4; ++i) W[i] = Xform.TransformPosition(FVector(Quad[i].X, Quad[i].Y, 2.0));
		for (int32 i = 0; i < 4; ++i)
		{
			PDI->DrawLine(W[i], W[(i + 1) % 4], HutongPlanColours::Selected, SDPG_Foreground, 7.0f, 0.0f, true);
			PDI->DrawLine(W[i], W[(i + 1) % 4], B->GetPlanColour(), SDPG_Foreground, 3.0f, 0.0f, true);
		}
	}
}

void URectDragToolBase::DrawHoverInspection(FPrimitiveDrawInterface* PDI) const
{
	// Latched for DrawHUD, later this frame.
	HudBuilding = HoveredBuilding;
	HudWorldPoint = HoveredWorldPoint;

	const UHutongBuildingComponent* Building = HoveredBuilding.Get();
	if (Building == nullptr || PDI == nullptr) return;

	const AActor* Actor = Building->GetOwner();
	if (Actor == nullptr) return;

	// Footprint, not bounds: bounds include eaves, platform overhang, 下鹼.
	const FVector2D Size = Building->GetFootprintSize();
	if (Size.X <= 0.0 || Size.Y <= 0.0) return;

	const FTransform Xform = Actor->GetActorTransform();
	FVector2D Quad[4];
	Building->GetFootprintCorners(Quad);
	const FHutongFootprintSkew Skew = Building->GetFootprintSkew();
	auto At = [&](double X, double Y)
	{
		const FVector2D Q = HutongFootprint::Map(Size, Skew, X, Y);
		return Xform.TransformPosition(FVector(Q.X, Q.Y, 0.0));
	};
	FVector Corners[4];
	for (int32 i = 0; i < 4; ++i) Corners[i] = Xform.TransformPosition(FVector(Quad[i].X, Quad[i].Y, 0.0));

	// Cool blue: the preview's colour for anything but the dragged footprint.
	const FLinearColor Mark(0.25f, 0.85f, 1.0f);
	for (int32 i = 0; i < 4; ++i)
	{
		PDI->DrawLine(Corners[i], Corners[(i + 1) % 4], Mark, SDPG_Foreground, 2.0f);
		// Short corner posts.
		PDI->DrawLine(Corners[i], Corners[i] + FVector(0.0, 0.0, 70.0), Mark, SDPG_Foreground, 1.0f);
	}

	// Bay lines as in the plan outline, so the 間 count reads on built buildings too.
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

	// Same text as the mode panel (headless-testable), split into lines; written once.
	TArray<FString> Lines;
	GetHoverSummaryText(/*bWithPending*/ false).ToString().ParseIntoArray(Lines, TEXT("\n"));
	if (Lines.Num() == 0) return;

	// Slate font, not GetMediumFont: the bitmap medium font has no CJK glyphs.
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);

	// Projected from the hit point; stays by the cursor, clamped inside the viewport.
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
	// Cursor left; nothing hovered.
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
		// Snap to neighbours' bearings and quarter turns off them, not a world grid.
		const TArray<double> Yaws = HutongSnap::GatherEdgeYaws(
			GetFootprints(), StartWorld, FMath::Max(Snap->Radius, 1.0) * 12.0);
		NewYaw = HutongSnap::SnapYaw(NewYaw, Yaws, FMath::Max(Snap->AngleToleranceDeg, 0.0));
	}

	NewYaw = FMath::Fmod(NewYaw, 360.0);
	if (NewYaw < 0.0) NewYaw += 360.0;
	PlacementYawDeg = NewYaw;

	// Keep rect rigid: re-project the captured local extent into world.
	CurrentWorld = LocalRectToWorld(RotateAnchorLocalRect.X, RotateAnchorLocalRect.Y);
}

void URectDragToolBase::SpawnFinalActor()
{
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const double SizeX = MaxX - MinX;
	const double SizeY = MaxY - MinY;
	if (SizeX < 10.0 || SizeY < 10.0) return;

	// Delayed, not shown outright.
	FScopedSlowTask Task(1.0f, LOCTEXT("PlacingActor", "Building geometry…"));
	Task.MakeDialogDelayed(0.4f);
	Task.EnterProgressFrame(1.0f);

	TArray<FDynamicMesh3> LODs;
	const int32 CollisionLOD = BuildLODsForRect(SizeX, SizeY, LODs);

	UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();
	const FVector ActorLocXY = LocalRectToWorld(MinX, MinY);
	const FTransform Xform(
		FRotator(0.0, PlacementYawDeg, 0.0),
		FVector(ActorLocXY.X, ActorLocXY.Y, StartWorld.Z));

	UInteractiveToolManager* ToolManager = GetToolManager();
	ToolManager->BeginUndoTransaction(NSLOCTEXT("HutongLayout", "PlaceActor", "Place Hutong Actor"));
	const FHutongPalette Palette = Appearance ? Appearance->Palette : FHutongPalette();
	// Plan-only: meshless actor, drawn by the component's outline.
	AStaticMeshActor* Actor = IsPlanOnly() || !BuildsGeometry()
		? HutongGen::SpawnEmptyActor(World, Xform, GetActorNameBase())
		: HutongGen::SpawnStaticMeshActor(World, LODs, Xform, GetActorNameBase(), Palette, CollisionLOD);
	if (Actor)
	{
		AttachBuildingComponent(Actor, SizeX, SizeY);
		AdoptNeighbourBaseCourse(Actor);
	}
	ToolManager->EndUndoTransaction();
}

void URectDragToolBase::AdoptNeighbourBaseCourse(AStaticMeshActor* Actor)
{
	// Anchor's neighbour first: that end is drawn from.
	AdoptBaseCourseFrom(Actor, AnchorSnapBuilding.Get() ? AnchorSnapBuilding.Get() : CursorSnapBuilding.Get());
}

void URectDragToolBase::AdoptBaseCourseFrom(AStaticMeshActor* Actor, const UHutongBuildingComponent* Neighbour)
{
	// Placed against another: same frontage, 下鹼 continues at the neighbour's line.
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
	// Shift: corners angle the footprint instead of resizing it.
	const FLinearColor PlanSkewColor(0.85f, 0.55f, 1.0f);
	const FLinearColor PlanDoorColor(1.0f, 0.3f, 0.25f);
	// Bay-line divide markers and end fuse markers.
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
	return HutongDetailOps::TurnFacing(HutongDetailOps::CollectSelected(), Delta);
}

bool URectDragToolBase::FlipSelectedFacing()
{
	return HutongDetailOps::FlipFacing(HutongDetailOps::CollectSelected());
}

bool URectDragToolBase::AdjustSelectedBays(int32 Delta)
{
	return HutongDetailOps::AdjustBays(HutongDetailOps::CollectSelected(), Delta);
}

bool URectDragToolBase::ToggleSelectedGate()
{
	return HutongDetailOps::ToggleGate(HutongDetailOps::CollectSelected());
}

bool URectDragToolBase::IsPendingFor(const TArray<UHutongBuildingComponent*>& Selected) const
{
	if (Selected.Num() == 0 || Selected.Num() != Pending.Buildings.Num()) return false;
	return !Selected.ContainsByPredicate([this](const UHutongBuildingComponent* B) { return !Pending.Buildings.Contains(B); });
}

void URectDragToolBase::HoldPendingFor(const TArray<UHutongBuildingComponent*>& Selected)
{
	if (IsPendingFor(Selected)) return;
	Pending = FPendingCycle();
	for (UHutongBuildingComponent* B : Selected) Pending.Buildings.Add(B);
}

TArray<UHutongBuildingComponent*> URectDragToolBase::PendingWork(const TArray<UHutongBuildingComponent*>& Selected) const
{
	return HutongDetailOps::NeedingChange(Selected, Pending.TypeIndex, Pending.Preset);
}

bool URectDragToolBase::CycleSelectedPreset(int32 Delta)
{
	const TArray<UHutongBuildingComponent*> Selected = HutongDetailOps::CollectSelected();
	if (Selected.Num() == 0 || Delta == 0) return false;
	HoldPendingFor(Selected);
	Pending.bArmed = false;
	Pending.Note = FText::GetEmpty();

	// Presets of the chosen type, else of the selection's own, which must then be one type.
	FName Key = NAME_None;
	if (Pending.TypeIndex != INDEX_NONE)
	{
		Key = HutongDetailOps::PresetKeyOf(HutongDetailOps::ConvertTargets()[Pending.TypeIndex]);
	}
	else
	{
		Key = Selected[0]->GetPresetKey();
		if (Selected.ContainsByPredicate([Key](const UHutongBuildingComponent* B) { return B->GetPresetKey() != Key; }))
		{
			Pending.Note = LOCTEXT("CycleMixed", "The selection mixes building types: press T to pick one type first.");
			return true;
		}
	}
	const TArray<FString> Names = UHutongPresetLibrary::Get()->GetPresetNames(Key);
	if (Names.Num() == 0)
	{
		Pending.Note = LOCTEXT("CycleNoPresets", "This building type has no presets.");
		return true;
	}
	const FString& Current = Pending.bChosen ? Pending.Preset : Selected[0]->Preset;
	const int32 Index = Names.IndexOfByKey(Current);
	const int32 Next = Index == INDEX_NONE ? (Delta > 0 ? 0 : Names.Num() - 1) : ((Index + Delta) % Names.Num() + Names.Num()) % Names.Num();
	Pending.Preset = Names[Next];
	Pending.bChosen = true;
	return true;
}

bool URectDragToolBase::CycleSelectedType(int32 Delta)
{
	const TArray<UHutongBuildingComponent*> Selected = HutongDetailOps::CollectSelected();
	const TArray<HutongDetailOps::FConvertTarget>& Targets = HutongDetailOps::ConvertTargets();
	if (Selected.Num() == 0 || Delta == 0 || Targets.Num() == 0) return false;
	HoldPendingFor(Selected);
	Pending.bArmed = false;
	Pending.Note = FText::GetEmpty();

	const int32 Own = HutongDetailOps::FindConvertTargetIndex(Selected[0]);
	if (Own == INDEX_NONE) return true;
	// Only a type every selected building may become, as the menu offers.
	if (!HutongDetailOps::CanAllBecome(Selected, Targets[Own]))
	{
		Pending.Note = LOCTEXT("CycleNoCommonType", "The selected buildings cannot become one type.");
		return true;
	}
	int32 Next = Pending.TypeIndex != INDEX_NONE ? Pending.TypeIndex : Own;
	do { Next = ((Next + Delta) % Targets.Num() + Targets.Num()) % Targets.Num(); }
	while (Next != Own && !HutongDetailOps::CanAllBecome(Selected, Targets[Next]));
	Pending.TypeIndex = Next;
	Pending.bChosen = true;
	// Back on its own type: its own preset. Else the type's first, so P steps on from there.
	if (Pending.TypeIndex == Own)
	{
		Pending.Preset = Selected[0]->Preset;
		return true;
	}
	const TArray<FString> Names = UHutongPresetLibrary::Get()->GetPresetNames(HutongDetailOps::PresetKeyOf(Targets[Pending.TypeIndex]));
	Pending.Preset = Names.Num() > 0 ? Names[0] : FString();
	return true;
}

void URectDragToolBase::ChooseForSelection(int32 TypeIndex, const FString& Preset)
{
	const TArray<UHutongBuildingComponent*> Selected = HutongDetailOps::CollectSelected();
	if (Selected.Num() == 0) return;
	Pending = FPendingCycle();
	for (UHutongBuildingComponent* B : Selected) Pending.Buildings.Add(B);
	Pending.TypeIndex = TypeIndex;
	Pending.Preset = Preset;
	Pending.bChosen = true;
	// Applies now, or holds the warning in the banner for Enter.
	ConfirmSelectedCycle();
}

bool URectDragToolBase::OpenSelectedMenu(bool bTypes, const FVector2D& ScreenPosition)
{
	const TArray<UHutongBuildingComponent*> Selected = HutongDetailOps::CollectSelected();
	if (Selected.Num() == 0 || !FSlateApplication::IsInitialized()) return false;
	const TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (!Window.IsValid()) return false;
	CancelSelectedCycle();

	const TWeakObjectPtr<URectDragToolBase> WeakThis(this);
	auto OnPick = [WeakThis](int32 TypeIndex, const FString& Preset)
	{
		if (WeakThis.IsValid()) WeakThis->ChooseForSelection(TypeIndex, Preset);
	};
	// Not searchable: the P / T that opened it would arrive as the first letter of a search.
	FMenuBuilder Menu(/*bShouldCloseWindowAfterMenuSelection*/ true, nullptr, nullptr, /*bCloseSelfOnly*/ false,
		&FCoreStyle::Get(), /*bSearchable*/ false, NAME_None, /*bRecursivelySearchable*/ false);
	if (bTypes) HutongContextMenu::FillTypeMenu(Menu, Selected, OnPick);
	else HutongContextMenu::FillPresetMenu(Menu, Selected, OnPick);

	FSlateApplication::Get().PushMenu(Window.ToSharedRef(), FWidgetPath(), Menu.MakeWidget(), ScreenPosition,
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
	return true;
}

bool URectDragToolBase::ConfirmSelectedCycle()
{
	const TArray<UHutongBuildingComponent*> Selected = HutongDetailOps::CollectSelected();
	if (!IsPendingFor(Selected) || !Pending.bChosen) return false;
	const TArray<UHutongBuildingComponent*> Work = PendingWork(Selected);
	if (Work.Num() == 0)
	{
		Pending = FPendingCycle();
		return true;
	}

	// First Enter: what would be lost. Each building against its own preset, so generated values on a
	// building placed without one count too.
	if (!Pending.bArmed)
	{
		Pending.Customized = HutongDetailOps::CustomizedAcross(Work);
		if (Pending.Customized.Num() > 0)
		{
			Pending.bArmed = true;
			return true;
		}
	}

	HutongDetailOps::ChangeTypeOrPreset(Work, Pending.TypeIndex, Pending.Preset);
	Pending = FPendingCycle();
	return true;
}

bool URectDragToolBase::CancelSelectedCycle()
{
	const bool bHeld = IsPendingFor(HutongDetailOps::CollectSelected()) && (Pending.bChosen || !Pending.Note.IsEmpty());
	Pending = FPendingCycle();
	return bHeld;
}

bool URectDragToolBase::GetPendingCycleLines(FText& OutTitle, TArray<FText>& OutDetails, bool& bOutWarning) const
{
	OutDetails.Reset();
	bOutWarning = false;
	const TArray<UHutongBuildingComponent*> Selected = HutongDetailOps::CollectSelected();
	if (!IsPendingFor(Selected)) return false;
	if (!Pending.Note.IsEmpty())
	{
		OutTitle = Pending.Note;
		OutDetails.Add(LOCTEXT("CycleNoteKeys", "Esc dismisses"));
		return true;
	}
	if (!Pending.bChosen) return false;

	const FText Type = Pending.TypeIndex != INDEX_NONE
		? FText::FromString(HutongDetailOps::ConvertTargets()[Pending.TypeIndex].Label)
		: Selected[0]->GetTypeLabel();
	const FText Preset = Pending.Preset.IsEmpty()
		? LOCTEXT("CycleNoPreset", "type defaults, no preset") : FText::FromString(Pending.Preset);
	OutTitle = FText::Format(LOCTEXT("CycleTitle", "{0}  ·  {1}"), Type, Preset);
	if (!Pending.bArmed)
	{
		OutDetails.Add(LOCTEXT("CycleKeys", "Enter applies  ·  Esc drops it  ·  Shift+P next preset  ·  Shift+T next type"));
		return true;
	}
	bOutWarning = true;
	constexpr int32 Shown = 4;
	TArray<FString> Names(Pending.Customized.GetData(), FMath::Min(Pending.Customized.Num(), Shown));
	FString List = FString::Join(Names, TEXT(", "));
	if (Pending.Customized.Num() > Shown) List += FString::Printf(TEXT(" and %d more"), Pending.Customized.Num() - Shown);
	OutDetails.Add(FText::Format(LOCTEXT("CycleWarnCount",
		"Warning: replaces {0} adjusted value(s): {1}"),
		FText::AsNumber(Pending.Customized.Num()), FText::FromString(List)));
	OutDetails.Add(LOCTEXT("CycleWarnKeys", "Enter again applies  ·  Esc keeps the building as it is"));
	return true;
}

FText URectDragToolBase::GetPendingCycleText() const
{
	FText Title;
	TArray<FText> Details;
	bool bWarning = false;
	if (!GetPendingCycleLines(Title, Details, bWarning)) return FText::GetEmpty();
	FString Out = FString::Printf(TEXT("Change to: %s"), *Title.ToString());
	for (const FText& Line : Details) Out += TEXT("\n") + Line.ToString();
	return FText::FromString(Out);
}

void URectDragToolBase::DrawBuildingLabels(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) const
{
	const UHutongLayoutModeSettings* Settings = UHutongLayoutEdMode::GetActiveSettings();
	if (Canvas == nullptr || RenderAPI == nullptr || (Settings && !Settings->bShowBuildingLabels)) return;
	const FSceneView* View = RenderAPI->GetSceneView();
	UWorld* World = GetWorld();
	const UFont* Font = UEngine::GetMediumFont();
	if (View == nullptr || World == nullptr || Font == nullptr) return;
	const float DPIScale = FMath::Max(Canvas->GetDPIScale(), UE_KINDA_SMALL_NUMBER);
	// Written once the footprint's narrower side spans this much of the screen: close zoom only.
	constexpr double MinSpanPixels = 70.0;
	const double LineHeight = Font->GetMaxCharHeight() + 2.0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const UHutongBuildingComponent* B = It->FindComponentByClass<UHutongBuildingComponent>();
		if (B == nullptr) continue;
		FVector2D Quad[4];
		B->GetFootprintCorners(Quad);
		const FTransform Xform = It->GetActorTransform();
		FVector2D Pixel[4];
		bool bOnScreen = true;
		for (int32 i = 0; i < 4 && bOnScreen; ++i)
		{
			bOnScreen = View->WorldToPixel(Xform.TransformPosition(FVector(Quad[i].X, Quad[i].Y, 0.0)), Pixel[i]);
			Pixel[i] /= DPIScale;
		}
		if (!bOnScreen) continue;
		// Narrower span of the projected quad: its two pairs of opposite edge midpoints.
		const double SpanA = FVector2D::Distance(0.5 * (Pixel[0] + Pixel[1]), 0.5 * (Pixel[2] + Pixel[3]));
		const double SpanB = FVector2D::Distance(0.5 * (Pixel[1] + Pixel[2]), 0.5 * (Pixel[3] + Pixel[0]));
		const double Span = FMath::Min(SpanA, SpanB);
		if (Span < MinSpanPixels) continue;

		TArray<FString> Lines = { AsciiForBitmapFont(B->GetTypeLabel().ToString()) };
		if (!B->Preset.IsEmpty()) Lines.Add(AsciiForBitmapFont(B->Preset));
		const FVector2D Centre = 0.25 * (Pixel[0] + Pixel[1] + Pixel[2] + Pixel[3]);
		const double Top = Centre.Y - 0.5 * Lines.Num() * LineHeight;
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			// The bitmap font draws wider than it measures.
			const double W = Font->GetStringSize(*Lines[i]) * 1.15;
			FCanvasTextItem Item(FVector2D(Centre.X - 0.5 * W, Top + i * LineHeight), FText::FromString(Lines[i]), Font,
				i == 0 ? FLinearColor::White : FLinearColor(1.0f, 0.9f, 0.55f));
			Item.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(Item);
		}
	}
}

void URectDragToolBase::DrawPendingCycleBanner(FCanvas* Canvas) const
{
	if (Canvas == nullptr) return;
	FText Title;
	TArray<FText> Details;
	bool bWarning = false;
	if (!GetPendingCycleLines(Title, Details, bWarning)) return;

	// The engine's bitmap fonts, scaled: a Slate-font text item draws nothing in this pass (the tile
	// drew, the text did not). They have no CJK glyphs, so names keep their English.
	const UFont* TitleFont = UEngine::GetLargeFont();
	const UFont* DetailFont = UEngine::GetMediumFont();
	if (TitleFont == nullptr || DetailFont == nullptr) return;
	constexpr double TitleScale = 1.6;
	constexpr double DetailScale = 1.3;
	// The bitmap font draws wider than it measures.
	constexpr double Overdraw = 1.15;
	const double MaxWidth = 0.9 * ViewportWidth(Canvas) - 48.0;

	struct FRow { FString Text; const UFont* Font; double Scale; FLinearColor Colour; };
	TArray<FRow> Rows;
	auto AddWrapped = [&](const FText& Text, const UFont* Font, double Scale, const FLinearColor& Colour)
	{
		for (const FString& Line : WrapToWidth(AsciiForBitmapFont(Text.ToString()), Font, MaxWidth / (Scale * Overdraw)))
		{
			Rows.Add({ Line, Font, Scale, Colour });
		}
	};
	AddWrapped(Title, TitleFont, TitleScale, bWarning ? FLinearColor(1.0f, 0.45f, 0.15f) : FLinearColor(1.0f, 0.9f, 0.15f));
	for (int32 i = 0; i < Details.Num(); ++i)
	{
		AddWrapped(Details[i], DetailFont, DetailScale,
			(bWarning && i == 0) ? FLinearColor(1.0f, 0.7f, 0.5f) : FLinearColor(0.85f, 0.85f, 0.85f));
	}

	auto RowWidth = [&](const FRow& R) { return R.Font->GetStringSize(*R.Text) * R.Scale * Overdraw; };
	auto RowHeight = [](const FRow& R) { return R.Font->GetMaxCharHeight() * R.Scale + 6.0; };
	double Widest = 0.0, Height = 0.0;
	for (const FRow& R : Rows)
	{
		Widest = FMath::Max(Widest, RowWidth(R));
		Height += RowHeight(R);
	}
	// Centred across the top, clear of the viewport toolbar.
	const double Pad = 18.0;
	const FVector2D At(FMath::Max(0.5 * (ViewportWidth(Canvas) - Widest) - Pad, 8.0), 56.0);
	FCanvasTileItem Backing(At, FVector2D(Widest + 2.0 * Pad, Height + 2.0 * Pad),
		bWarning ? FLinearColor(0.25f, 0.04f, 0.0f, 0.8f) : FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
	Backing.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Backing);

	double Y = At.Y + Pad;
	for (const FRow& R : Rows)
	{
		FCanvasTextItem Item(FVector2D(At.X + Pad + 0.5 * (Widest - RowWidth(R)), Y), FText::FromString(R.Text), R.Font, R.Colour);
		Item.Scale = FVector2D(R.Scale, R.Scale);
		Item.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Item);
		Y += RowHeight(R);
	}
}

FString URectDragToolBase::AsciiForBitmapFont(const FString& Text)
{
	// "Side House (廂房)" -> "Side House": a group of CJK in brackets goes whole, a stray glyph alone.
	auto IsWide = [](TCHAR C) { return C >= 0x2E80; };
	FString Out;
	for (int32 i = 0; i < Text.Len(); ++i)
	{
		const TCHAR C = Text[i];
		if (C == TEXT('('))
		{
			const int32 Close = Text.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, i);
			bool bAllWide = Close != INDEX_NONE;
			for (int32 k = i + 1; bAllWide && k < Close; ++k) bAllWide = IsWide(Text[k]) || Text[k] == TEXT(' ') || Text[k] == TEXT(',');
			if (bAllWide)
			{
				i = Close;
				continue;
			}
		}
		if (C == 0x00B7) Out += TEXT("-");
		else if (!IsWide(C)) Out.AppendChar(C);
	}
	while (Out.ReplaceInline(TEXT("  "), TEXT(" ")) > 0) {}
	Out.ReplaceInline(TEXT(" )"), TEXT(")"));
	return Out.TrimStartAndEnd();
}

bool URectDragToolBase::SetSelectedFacing(const FText& Title,
	TFunctionRef<EHutongBaySide(const UHutongBuildingComponent&, EHutongBaySide)> NextSide)
{
	return HutongDetailOps::SetFacing(HutongDetailOps::CollectSelected(), Title, NextSide);
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
	// A tenth of the shorter side, clamped between a hand's breadth and a pace.
	const double Basis = IsLineLikePlan(Size) ? 0.03 * FMath::Max(Size.X, Size.Y) : 0.1 * FMath::Min(Size.X, Size.Y);
	return FMath::Clamp(Basis, 50.0, 160.0);
}

double URectDragToolBase::PlanCornerHandleSize(const FVector2D& Size)
{
	if (!IsLineLikePlan(Size)) return PlanHandleSize(Size);
	return FMath::Clamp(0.35 * FMath::Min(Size.X, Size.Y), 12.0, 160.0);
}

double URectDragToolBase::SkewCornerRadius(const FVector2D& Size, const FVector& At) const
{
	// A run's corners are sized off its thickness, a few centimetres, and were a pixel or two to hit
	// zoomed out. Two at one end may then overlap; the nearer takes the press.
	constexpr double MinPixels = 8.0;
	return FMath::Max(0.5 * PlanCornerHandleSize(Size), MinPixels * WorldPerPixelAt(At));
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

bool URectDragToolBase::PressStartsPlacement(const FVector& Ground, const UHutongBuildingComponent* Selected, int32 SelectedHit) const
{
	if (!StartsOnFootprintEdges()) return false;
	// Every handle of the selected building beats a wall start: its resize handles reach inside
	// the footprint, and a press there to resize was taken as a wall anchor on the edge.
	if (Selected && SelectedHit != INDEX_NONE && SelectedHit != PlanHandleInside)
	{
		return false;
	}
	// Within snap reach of an outline, where the anchor lands on it; further in is the building's. On a
	// run (a wall) all of it is within reach of its outline, so a press on one is always the run's —
	// select it to slide its gate; a press just off it still starts a wall on its face.
	const double Band = EffectiveSnapRadius(Ground);
	bool bNearEdge = false;
	for (const HutongSnap::FFootprint& F : GetFootprints())
	{
		const UHutongBuildingComponent* B = F.Building.Get();
		if (!B) continue;
		const FVector Local3 = F.ActorToWorld.InverseTransformPosition(Ground);
		const FVector2D Local(Local3.X, Local3.Y);
		FVector2D Quad[4];
		B->GetFootprintCorners(Quad);
		if (IsLineLikePlan(F.Size) && HutongFootprint::PointInQuad(Quad, Local)) return false;
		bNearEdge = bNearEdge || HutongFootprint::DistanceToQuadEdge(Quad, Local) <= Band;
	}
	return bNearEdge;
}

bool URectDragToolBase::IsPlanGrabPoint(const FVector2D& Size, double EdgeDistance, const FVector& Ground) const
{
	if (IsLineLikePlan(Size)) return true;
	// Cap the neighbour-snap band: on the smallest 垂花門 it reaches past the middle from both long
	// edges, leaving nothing pressable.
	const double Band = FMath::Min(EffectiveSnapRadius(Ground), 0.3 * FMath::Min(Size.X, Size.Y));
	return EdgeDistance > Band;
}

FVector URectDragToolBase::PlanRotateHandleWorld(const UHutongBuildingComponent* Building, FVector& OutEdgeMid) const
{
	// Past the front (−Y with no front), two handle-widths out, clear of any corner.
	const AActor* Owner = Building->GetOwner();
	const FTransform Xf = Owner->GetActorTransform();
	const FVector2D Size = Building->GetFootprintSize();
	EHutongBaySide Side = EHutongBaySide::MinusY;
	Building->GetFacade(Side);
	const HutongGen::BaySide::FEdge E =
		HutongGen::BaySide::GetEdge((HutongGen::EBaySide)Side, 0.0, 0.0, Size.X, Size.Y);
	const FVector2D Mid = HutongFootprint::Map(Size, Building->GetFootprintSkew(),
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

	// Opening sliders first, on any building with them; the rest is laid-out only.
	{
		TArray<FHutongPlanOpening> Openings;
		Building->GetPlanOpenings(Openings);
		for (int32 i = 0; i < Openings.Num(); ++i)
		{
			double Width;
			if (FVector::Dist2D(Ground, PlanOpeningWorld(Building, i, Width)) <= Pick) return PlanHandleOpening + i;
		}
	}
	// Divide markers on bay lines, fuse markers on shared ends, built or laid out. Small picks keep
	// the plan interior grabbable.
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
		if (FindFuseNeighbour(Building, true) && FVector::Dist2D(Ground, FuseMarkerWorld(Building, true)) <= MarkerPick) return PlanHandleFuseEnd;
		if (FindFuseNeighbour(Building, false) && FVector::Dist2D(Ground, FuseMarkerWorld(Building, false)) <= MarkerPick) return PlanHandleFuseStart;
	}
	FVector2D Quad[4];
	Building->GetFootprintCorners(Quad);

	// Shift: corners angle on any footprint (a wall end cut on the bias to meet an off-square
	// neighbour). Picked at drawn size, inside a run's thickness.
	const bool bSkew = IsSkewKeyDown();
	const double CornerPick = 1.5 * SkewCornerRadius(Size, Ground);
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
	// A built building offers nothing else: a plain click places.
	if (!Building->bPlanOnly) return INDEX_NONE;

	FVector EdgeMid;
	if (FVector::Dist2D(Ground, PlanRotateHandleWorld(Building, EdgeMid)) <= Pick) return PlanHandleRing;
	// Corners before edges: a small footprint's corner beats the adjacent midpoint.
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
		// Wall leg end: the run's vertex, not the rect edge; the legs meeting there move with it.
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
		// Corner keeps its offset from the cursor instead of jumping on the press.
		const FVector Local = Xf.InverseTransformPosition(Ground);
		EditGrabLocal = FVector(Local.X, Local.Y, 0.0);
	}
	else
	{
		PlanEdit = EPlanEdit::Resize;
		EditHandle = Hit;
		CaptureJoint(Building, Hit);
	}
}

bool URectDragToolBase::IsOnOutline(const FVector& Point, const UHutongBuildingComponent* Building)
{
	const AActor* Owner = Building ? Building->GetOwner() : nullptr;
	if (!Owner) return false;
	FVector2D Quad[4];
	Building->GetFootprintCorners(Quad);
	const FVector Local = Owner->GetActorTransform().InverseTransformPosition(Point);
	return HutongFootprint::DistanceToQuadEdge(Quad, FVector2D(Local.X, Local.Y)) <= 2.0;
}

UHutongBuildingComponent* URectDragToolBase::ShiftToggleTarget(const FInputDeviceRay& Ray, const FVector& Ground, int32 SelectedHit) const
{
	if (GEditor == nullptr || !FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetModifierKeys().IsShiftDown()) return nullptr;
	// A handle Shift means something on (a corner's skew, the ring, a slider) keeps the press.
	if (SelectedHit != INDEX_NONE && SelectedHit != PlanHandleInside) return nullptr;
	if (UHutongBuildingComponent* Plan = FindPlanBuildingAt(Ground)) return Plan->GetOwner() ? Plan : nullptr;
	FHitResult Hit;
	if (!TraceHoveredBuilding(Ray, Hit)) return nullptr;
	AActor* Actor = Hit.GetActor();
	return Actor ? Actor->FindComponentByClass<UHutongBuildingComponent>() : nullptr;
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

	const UHutongBuildingComponent* Selected = GetSelectedBuilding();
	const int32 SelectedHit = Selected ? HitTestPlan(Selected, Ground) : INDEX_NONE;
	// Shift on a building, built or laid out: the press toggles it in the selection.
	if (ShiftToggleTarget(PressPos, Ground, SelectedHit))
	{
		return FInputRayHit((float)FVector::Distance(PressPos.WorldRay.Origin, Ground));
	}
	// On a footprint edge in the wall tool: open ground, so the release places.
	if (PressStartsPlacement(Ground, Selected, SelectedHit))
	{
		return FInputRayHit((float)FVector::Distance(PressPos.WorldRay.Origin, Ground));
	}
	bool bOver = SelectedHit != INDEX_NONE;
	if (!bOver)
	{
		double Edge = 0.0;
		const UHutongBuildingComponent* Plan = FindPlanBuildingAt(Ground, &Edge);
		bOver = Plan && IsPlanGrabPoint(Plan->GetFootprintSize(), Edge, Ground);
	}
	if (!bOver)
	{
		// Open ground: drag = box select, click = placement, told apart on release. Built geometry
		// is left to the editor's selection.
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
	if (UHutongBuildingComponent* Toggle = ShiftToggleTarget(PressPos, Ground, Hit))
	{
		// Added if out, taken out if in; no edit follows, so the release does nothing.
		AActor* Actor = Toggle->GetOwner();
		const FScopedTransaction Transaction(LOCTEXT("ToggleSelect", "Select Hutong Building"));
		GEditor->SelectActor(Actor, !Actor->IsSelected(), true, true);
		return;
	}
	const bool bPlace = PressStartsPlacement(Ground, Building, Hit);
	if (bPlace) Hit = INDEX_NONE;
	if (Hit == INDEX_NONE)
	{
		// On another laid-out building: select it; the same press moves it.
		Building = bPlace ? nullptr : FindPlanBuildingAt(Ground);
		if (!Building || !Building->GetOwner() || GEditor == nullptr)
		{
			// Nothing under it: start a box.
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

	// Drag events fire without movement: require slop past the press pixel before it is an edit,
	// not a click.
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
			// A tool that places nothing: a click on open ground clears the selection, as the editor's would.
			if (!HasPlacement())
			{
				if (GEditor && !FSlateApplication::Get().GetModifierKeys().IsShiftDown()) GEditor->SelectNone(true, true);
				return;
			}
			// Never dragged: a click, which places.
			FVector Hit;
			if (TryRayHitGround(ReleasePos, Hit))
			{
				ProcessClick(Hit);
				UpdatePlacementReadout();
			}
			return;
		}
		// The box takes every footprint it touches: a narrow box across a building held none of its
		// corners and missed it.
		const FVector2D Lo(FMath::Min(Start.X, End.X), FMath::Min(Start.Y, End.Y));
		const FVector2D Hi(FMath::Max(Start.X, End.X), FMath::Max(Start.Y, End.Y));
		TArray<AActor*> Picked;
		for (const HutongSnap::FFootprint& F : GetFootprints())
		{
			UHutongBuildingComponent* B = F.Building.Get();
			AActor* Actor = B ? B->GetOwner() : nullptr;
			if (!Actor) continue;
			const FVector2D Quad[4] = { FVector2D(F.Corners[0]), FVector2D(F.Corners[1]), FVector2D(F.Corners[2]), FVector2D(F.Corners[3]) };
			if (HutongFootprint::QuadOverlapsBox(Quad, Lo, Hi)) Picked.Add(Actor);
		}
		if (GEditor == nullptr) return;
		const FScopedTransaction Transaction(LOCTEXT("MarqueeSelect", "Select Hutong Buildings"));
		// Shift adds, like the editor's box.
		if (!IsSkewKeyDown()) GEditor->SelectNone(false, true);
		for (AActor* Actor : Picked) GEditor->SelectActor(Actor, true, false, true);
		GEditor->NoteSelectionChange();
		return;
	}
	if (PlanEdit == EPlanEdit::Divide || PlanEdit == EPlanEdit::Fuse)
	{
		// The marker is a button: released on it, it acts; dragged off, nothing.
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
	// An unmoved press was a selection; nothing to commit.
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
	const FVector2D Q = HutongFootprint::Map(Size, Building->GetFootprintSkew(),
		bAlongX ? Along : 0.5 * Size.X, bAlongX ? 0.5 * Size.Y : Along);
	return Owner->GetActorTransform().TransformPosition(FVector(Q.X, Q.Y, 0.0));
}

FVector URectDragToolBase::FuseMarkerWorld(const UHutongBuildingComponent* Building, bool bAtEnd) const
{
	const AActor* Owner = Building ? Building->GetOwner() : nullptr;
	if (!Owner) return FVector::ZeroVector;
	const FVector2D Size = Building->GetFootprintSize();
	const bool bAlongX = Building->ArePlanBaysAlongX();
	const double Along = bAtEnd ? (bAlongX ? Size.X : Size.Y) : 0.0;
	const double Across = 0.25 * (bAlongX ? Size.Y : Size.X);
	const FVector2D Q = HutongFootprint::Map(Size, Building->GetFootprintSkew(),
		bAlongX ? Along : Across, bAlongX ? Across : Along);
	return Owner->GetActorTransform().TransformPosition(FVector(Q.X, Q.Y, 0.0));
}

namespace
{
	// Edge 4..7 (−Y, +X, +Y, −X) of a footprint quad, corners anticlockwise from the origin.
	void QuadEdge(const FVector2D Quad[4], int32 Edge, FVector2D& A, FVector2D& B)
	{
		const int32 First = (Edge - 4 + 0) % 4;
		A = Quad[First];
		B = Quad[(First + 1) % 4];
	}
}

void URectDragToolBase::CaptureJoint(UHutongBuildingComponent* Building, int32 Hit)
{
	Joints.Reset();
	const AActor* Owner = Building ? Building->GetOwner() : nullptr;
	if (!Owner || Hit < 0 || Hit > 7 || Cast<UHutongWallBuildingComponent>(Building)) return;
	// A corner moves both its edges: neighbours on either follow (the corner lies on both lines).
	if (Hit < 4)
	{
		CaptureJointOnEdge(Building, 4 + Hit);
		CaptureJointOnEdge(Building, 4 + (Hit + 3) % 4);
	}
	else
	{
		CaptureJointOnEdge(Building, Hit);
	}
}

void URectDragToolBase::CaptureJointOnEdge(UHutongBuildingComponent* Building, int32 Hit)
{
	const AActor* Owner = Building->GetOwner();
	FVector2D Quad[4], A0, A1;
	Building->GetFootprintCorners(Quad);
	QuadEdge(Quad, Hit, A0, A1);
	const FTransform Xf = Owner->GetActorTransform();
	const FVector2D WA0(Xf.TransformPosition(FVector(A0, 0.0))), WA1(Xf.TransformPosition(FVector(A1, 0.0)));
	const FVector2D Dir = (WA1 - WA0).GetSafeNormal();
	const FVector2D Centre(Xf.TransformPosition(FVector(0.5 * (Quad[0] + Quad[2]), 0.0)));
	const double Len = FVector2D::Distance(WA0, WA1);
	// Hand-placed neighbours read as touching a few centimetres off; the join brings them onto the line.
	const double Tolerance = FMath::Max(2.0, 3.0 * WorldPerPixelAt(FVector(0.5 * (WA0 + WA1), 0.0)));

	for (const HutongSnap::FFootprint& F : GetFootprints())
	{
		UHutongBuildingComponent* Other = F.Building.Get();
		if (!Other || Other == Building || !Other->bPlanOnly || Other->HasFootprintSkew()
			|| Cast<UHutongWallBuildingComponent>(Other) || !Other->GetOwner()) continue;
		if (Joints.ContainsByPredicate([&](const FJoint& J) { return J.Start.Building.Get() == Other; })) continue;
		FVector2D OQuad[4];
		Other->GetFootprintCorners(OQuad);
		const FTransform OXf = Other->GetOwner()->GetActorTransform();
		const FVector2D OCentre(OXf.TransformPosition(FVector(0.5 * (OQuad[0] + OQuad[2]), 0.0)));
		for (int32 E = 4; E < 8; ++E)
		{
			FVector2D B0, B1;
			QuadEdge(OQuad, E, B0, B1);
			const FVector2D WB0(OXf.TransformPosition(FVector(B0, 0.0))), WB1(OXf.TransformPosition(FVector(B1, 0.0)));
			// On the dragged edge's line…
			if (FMath::Abs(FVector2D::CrossProduct(Dir, WB0 - WA0)) > Tolerance
				|| FMath::Abs(FVector2D::CrossProduct(Dir, WB1 - WA0)) > Tolerance) continue;
			// …overlapping a good part of it…
			const double T0 = FVector2D::DotProduct(Dir, WB0 - WA0), T1 = FVector2D::DotProduct(Dir, WB1 - WA0);
			const double Overlap = FMath::Min(Len, FMath::Max(T0, T1)) - FMath::Max(0.0, FMath::Min(T0, T1));
			if (Overlap < 0.3 * FMath::Min(Len, FVector2D::Distance(WB0, WB1))) continue;
			// …and on its far side, not lying over it.
			const FVector2D Normal(-Dir.Y, Dir.X);
			if (FVector2D::DotProduct(Normal, Centre - WA0) * FVector2D::DotProduct(Normal, OCentre - WA0) >= 0.0) continue;
			FJoint& J = Joints.AddDefaulted_GetRef();
			CaptureLegState(Other, J.Start);
			J.Edge = E;
			break;
		}
	}
}

void URectDragToolBase::UpdateJoint()
{
	UHutongBuildingComponent* B = EditedPlan.Get();
	if (!B || !B->GetOwner()) return;
	const bool bAlone = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsControlDown();
	FVector2D Quad[4];
	B->GetFootprintCorners(Quad);
	const FVector Edge = B->GetOwner()->GetActorTransform().TransformPosition(PlanHandleLocal(Quad, EditHandle));
	for (const FJoint& J : Joints)
	{
		UHutongBuildingComponent* Partner = J.Start.Building.Get();
		if (!Partner || !Partner->GetOwner()) continue;
		RestoreLegState(J.Start);
		if (!bAlone)
		{
			// The neighbour's joined edge goes to the dragged edge's line; its opposite edge stays.
			const bool bX = J.Edge == 5 || J.Edge == 7;
			const bool bMax = J.Edge == 5 || J.Edge == 6;
			const FVector Local = J.Start.Transform.InverseTransformPosition(Edge);
			const double T = bX ? Local.X : Local.Y;
			const double Len0 = bX ? J.Start.Size.X : J.Start.Size.Y;
			const double Len = FMath::Max(PlanMinSize, bMax ? T : Len0 - T);
			Partner->SetFootprintSize(bX ? FVector2D(Len, J.Start.Size.Y) : FVector2D(J.Start.Size.X, Len));
			if (!bMax)
			{
				const double Accepted = bX ? Partner->GetFootprintSize().X : Partner->GetFootprintSize().Y;
				const FVector Shift = bX ? FVector(Len0 - Accepted, 0.0, 0.0) : FVector(0.0, Len0 - Accepted, 0.0);
				Partner->GetOwner()->SetActorLocation(J.Start.Transform.GetLocation() + J.Start.Transform.TransformVector(Shift));
			}
		}
		Partner->ApplyPlanOutline();
	}
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
		// Grab point follows the cursor; the origin corner snaps to neighbours, excluding self.
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
		// Joined, the nearest snap is the partner's own edge, which would hold the join still.
		if (SnappingActive() && !HasJoint())
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
		// A minus-side drag keeps the far edge fixed.
		const FVector2D Accepted = B->GetFootprintSize();
		const FVector Shift(SideX < 0 ? S.X - Accepted.X : 0.0, SideY < 0 ? S.Y - Accepted.Y : 0.0, 0.0);
		Owner->SetActorTransform(EditStartTransform);
		Owner->SetActorLocation(EditStartTransform.GetLocation() + EditStartTransform.TransformVector(Shift));
		if (HasJoint()) UpdateJoint();
		break;
	}
	case EPlanEdit::Skew:
	{
		// Corner follows the cursor, the other three fixed; stops at the last set that still builds
		// (no fold).
		const FVector2D S = EditStartSize;
		const FVector2D Rect[4] = { FVector2D(0.0, 0.0), FVector2D(S.X, 0.0), FVector2D(S.X, S.Y), FVector2D(0.0, S.Y) };
		const FVector2D StartOffset = EditStartSkew.Get(EditHandle);
		const FVector2D StartLocal = Rect[EditHandle] + StartOffset;
		FHutongFootprintSkew Candidate = B->FootprintSkew;
		const bool bEnds = Candidate.Mode == EHutongSkewMode::Ends;

		// Ends mode: one axis per drag (along or across), read from the whole pull so an early
		// wobble cannot pick it; locked once the pull is two handles long.
		const FVector RawLocal = EditStartTransform.InverseTransformPosition(Ground);
		const FVector2D Pull(RawLocal.X - EditGrabLocal.X, RawLocal.Y - EditGrabLocal.Y);
		if (bEnds && !bSkewAxisHeld)
		{
			const double Handle = PlanCornerHandleSize(S);
			if (Pull.Size() < 0.5 * Handle) break;
			EditSkewAxis = FMath::Abs(Pull.X) >= FMath::Abs(Pull.Y) ? 0 : 1;
			bSkewAxisHeld = Pull.Size() >= 2.0 * Handle;
		}

		// Unsnapped corner: at the grab offset, held to its axis under Ends.
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
				// An axis-held corner mitres where its line crosses a neighbour's face, found along
				// the line, so the cursor need not be near the face.
				const FVector AxisDir = EditStartTransform.TransformVector(EditSkewAxis == 0 ? FVector(1, 0, 0) : FVector(0, 1, 0)).GetSafeNormal2D();
				R = HutongSnap::FindSnapAlongLine(GetFootprints(), CornerStart, FVector2D(AxisDir.X, AxisDir.Y), CornerFree, 2.0 * SnapR, Owner);
			}
			else
			{
				// To a neighbour's corner or edge, never the start spot: a butted wall shares that
				// corner and snapping there held it still.
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
		if (HutongFootprint::IsSkewValid(S, B->WithEndBays(Candidate))) B->FootprintSkew = Candidate;
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
			// Snap to anything but the run's own legs, which move with the vertex.
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
					// On that face: end cut flush along it, slides along it.
					Face.bSet = true;
					Face.Point = FVector2D(G.X, G.Y);
					const double Yaw = FMath::DegreesToRadians(R.EdgeYawDeg);
					Face.Dir = FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw));
				}
			}
			SnapForSide = R;
		}
		// Leg bearing kept under Shift or while the pull stays within a few pixels of its line. Not
		// an angular band: on a long leg that was tens of cm wide.
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
				// Shift always; the few-pixel hold only while snapping, so the snap key frees the vertex.
				if (IsSkewKeyDown() || (SnappingActive() && OffLine <= Band))
				{
					const FVector2D P = Fixed + Was * FVector2D::DotProduct(Now, Was);
					G = FVector(P.X, P.Y, G.Z);
				}
			}
		}
		TArray<FVector2D> Vertices = EditRun.Vertices;
		Vertices[K] = FVector2D(G.X, G.Y);
		// Face along the leg: outer face on it, body on the neighbour's side. A lone wall shifts
		// whole and parallel; a run leg shifts this end, far vertex stays.
		if (SnapForSide.bSnapped)
		{
			const FVector2D Dir = (K > 0 ? Vertices[K] - Vertices[K - 1] : Vertices[1] - Vertices[0]).GetSafeNormal();
			double DrawnY = 0.0;
			const double Tolerance = bEditRunSideOn ? HutongWallChain::AlongFaceDegAfter : HutongWallChain::AlongFaceDeg;
			bEditRunSideOn = HutongWallChain::SideAlongFace(Dir, SnapForSide.EdgeYawDeg, SnapForSide.EdgeYaw2Deg, SnapForSide.Inward, EditRun.Thickness, DrawnY, Tolerance, EditRun.Setback);
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
		// Cursor distance along the run, actor frame; the component clamps the opening inside the
		// wall.
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
			// Snap to neighbour bearings and quarter turns, as hold-R does.
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
	// A built wall is not re-baked per frame: slider drawn from params, mesh updates on release.
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
	// Building moved: cached corners are stale.
	ON_SCOPE_EXIT { HutongSnap::Invalidate(); bHoverTraceValid = false; };

	if (PlanEdit == EPlanEdit::RunVertex)
	{
		// All legs rewound and reapplied in one transaction, like a single-building edit.
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

		// Cursor moved but nothing changed: a selection click, no undo entry.
		const bool bUnchanged = Final.Equals(EditStartTransform, 1.0e-6) && FinalSize.Equals(EditStartSize, 1.0e-6)
			&& FinalSkew == EditStartSkew && (!bOpening || FMath::IsNearlyEqual(FinalCentre, EditStartOpeningCentre));
		if (bUnchanged)
		{
			CancelPlanEdit();
			return;
		}

		// Rewound and reapplied inside one transaction, the joined neighbour with it.
		TArray<FRunLegState> PartnerFinal;
		for (const FJoint& J : Joints)
		{
			CaptureLegState(J.Start.Building.Get(), PartnerFinal.AddDefaulted_GetRef());
			RestoreLegState(J.Start);
		}
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
			for (const FRunLegState& PartnerState : PartnerFinal)
			{
				UHutongBuildingComponent* Partner = PartnerState.Building.Get();
				AActor* PartnerOwner = Partner ? Partner->GetOwner() : nullptr;
				if (!PartnerOwner) continue;
				PartnerOwner->Modify();
				Partner->Modify();
				if (UActorComponent* Outline = PartnerOwner->FindComponentByClass<UHutongPlanOutlineComponent>()) Outline->Modify();
				RestoreLegState(PartnerState);
				Partner->Rebuild();
			}
		}
	}
	PlanEdit = EPlanEdit::None;
	EditedPlan.Reset();
	EditHandle = INDEX_NONE;
	Joints.Reset();
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
	for (const FJoint& J : Joints)
	{
		if (UHutongBuildingComponent* Partner = J.Start.Building.Get())
		{
			RestoreLegState(J.Start);
			Partner->ApplyPlanOutline();
		}
	}
	Joints.Reset();
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

	// Opening sliders, on any building with them.
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
	// Divide diamonds on interior bay lines, fuse bowties on ends a like neighbour abuts. The
	// hovered marker's line is drawn across.
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
			const FVector C = FuseMarkerWorld(B, bAtEnd) + Lift;
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
	// Shift: corner handles mean angle, coloured so. Built buildings and runs show them only under
	// Shift: built offers no other edit, a run's plain handles are its ends. Run handles sit inside
	// its thickness.
	const bool bSkewMode = IsSkewKeyDown() || PlanEdit == EPlanEdit::Skew;
	if (!B->bPlanOnly && !bSkewMode) return;
	const double CornerR = SkewCornerRadius(Size, Xf.TransformPosition(PlanHandleLocal(Quad, 0)));

	// Corners: crossed square, turned with the footprint.
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

	// Line the skewed corner is held to, extended past the footprint both ways.
	if (PlanEdit == EPlanEdit::Skew && EditedPlan.Get() == B && EditSkewAxis != INDEX_NONE && EditHandle >= 0 && EditHandle < 4)
	{
		const FVector P = Xf.TransformPosition(PlanHandleLocal(Quad, EditHandle)) + Lift;
		const FVector Dir = EditSkewAxis == 0 ? AX : AY;
		const double Reach = 0.5 * (EditSkewAxis == 0 ? Size.X : Size.Y) + 4.0 * S;
		DrawPreviewLine(PDI, P - Dir * Reach, P + Dir * Reach, PlanSkewColor, 1.5f);
	}

	if (!B->bPlanOnly) return;

	// Rotate ring off the front, tied to the edge, with an arc arrow so it reads as a turn.
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
	// Three-quarter inner arc ending in an arrowhead.
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
