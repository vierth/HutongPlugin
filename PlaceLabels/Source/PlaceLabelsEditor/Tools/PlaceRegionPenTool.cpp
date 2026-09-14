#include "Tools/PlaceRegionPenTool.h"

#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "InteractiveToolManager.h"
#include "ToolContextInterfaces.h"
#include "PlaceLabelGeometry.h"
#include "PlaceLabelsFootprint.h"
#include "PlaceRegionActor.h"
#include "PlaceRegionComponent.h"
#include "Algo/Reverse.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Framework/Application/SlateApplication.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "PlaceRegionPenTool"

namespace
{
	// Sized and picked in pixels, then converted per point.
	constexpr double HandlePickPixels = 13.0;
	constexpr double HandleDrawPixels = 8.0;

	// How far the cursor may travel between press and release and still count as a click.
	constexpr double ClickSlopPixels = PlaceLabelsEdit::ClickSlopPixels;

	// Footprint tracing reaches this far past a building's edge, so you need only click near one.
	constexpr double FootprintSearchRadiusCm = 400.0;
}

void UPlaceRegionPenTool::Setup()
{
	UInteractiveTool::Setup();

	ClickDragBehavior = NewObject<UClickDragInputBehavior>(this);
	ClickDragBehavior->Initialize(this);
	ClickDragBehavior->bUpdateModifiersDuringDrag = true;
	AddInputBehavior(ClickDragBehavior);

	HoverBehavior = NewObject<UMouseHoverBehavior>(this);
	HoverBehavior->Initialize(this);
	AddInputBehavior(HoverBehavior);

	// Registration order is panel order: the settings you edit, then the readout you watch.
	Settings = NewObject<UPlaceRegionPenToolProperties>(this);
	RegisterSettings(Settings);
	Settings->OwningTool = this;

	// RestoreProperties brings back the snapping preferences.
	Settings->Name = FPlaceName();
	Settings->Type = nullptr;

	Readout = NewObject<UPlaceRegionPenToolReadout>(this);
	RegisterSettings(Readout, false);

	RefreshCachedRegions();
}

void UPlaceRegionPenToolProperties::CreateRegion()
{
	if (UPlaceRegionPenTool* Tool = OwningTool.Get())
	{
		Tool->ConfirmRegion();
	}
}

void UPlaceRegionPenToolProperties::DiscardOutline()
{
	if (UPlaceRegionPenTool* Tool = OwningTool.Get())
	{
		Tool->CancelDrawing();
	}
}

void UPlaceRegionPenTool::Shutdown(EToolShutdownType ShutdownType)
{
	for (const TObjectPtr<UInteractiveToolPropertySet>& Set : RegisteredSettings)
	{
		if (Set)
		{
			Set->SaveProperties(this);
		}
	}
	UInteractiveTool::Shutdown(ShutdownType);
}

void UPlaceRegionPenTool::RegisterSettings(UInteractiveToolPropertySet* PropertySet, bool bPersist)
{
	if (!PropertySet)
	{
		return;
	}
	if (bPersist)
	{
		PropertySet->RestoreProperties(this);
		RegisteredSettings.Add(PropertySet);
	}
	AddToolPropertySource(PropertySet);
}

UWorld* UPlaceRegionPenTool::GetTargetWorld() const
{
	return GetToolManager() && GetToolManager()->GetContextQueriesAPI()
		? GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld()
		: nullptr;
}

void UPlaceRegionPenTool::OnTick(float DeltaTime)
{
	TimeSinceCacheRefresh += DeltaTime;
	if (TimeSinceCacheRefresh < 1.5)
	{
		return;
	}
	TimeSinceCacheRefresh = 0.0;

	// Never mid-drag: rebuilding the snap set while a corner follows the cursor would change what it snaps to underneath the hand holding it.
	if (!bDraggingPoint)
	{
		RefreshCachedRegions();
	}
}

void UPlaceRegionPenTool::RefreshCachedRegions()
{
	// Snapping wants something to snap to.
	PlaceLabelsEdit::GatherRegions(GetTargetWorld(), CachedRegions, /*bOnlyWithUsableOutline*/ true);
}

bool UPlaceRegionPenTool::TryHitGround(const FInputDeviceRay& Ray, FVector& OutHit) const
{
	// Fall back to the height of the last point placed, or Z=0 before there is one.
	const double FallbackZ = PendingPoints.Num() > 0 ? PendingPoints.Last().Z : 0.0;
	return PlaceLabelsEdit::TraceGround(GetTargetWorld(), Ray.WorldRay.Origin,
		Ray.WorldRay.Direction, FallbackZ, OutHit);
}

FInputRayHit UPlaceRegionPenTool::GroundRayHit(const FInputDeviceRay& Ray) const
{
	FVector Hit;
	if (!TryHitGround(Ray, Hit))
	{
		return FInputRayHit();
	}
	return FInputRayHit(static_cast<float>((Hit - Ray.WorldRay.Origin).Size()));
}

double UPlaceRegionPenTool::PickRadiusAt(const FVector& WorldPoint) const
{
	const double WorldPerPixel = bCameraIsOrthographic
		? WorldPerPixelOrtho
		: FVector::Dist(WorldPoint, CameraPosition) * WorldPerPixelPerUnitDistance;
	return FMath::Max(WorldPerPixel * HandlePickPixels, 1.0);
}

double UPlaceRegionPenTool::HandleSizeAt(const FVector& WorldPoint) const
{
	const double WorldPerPixel = bCameraIsOrthographic
		? WorldPerPixelOrtho
		: FVector::Dist(WorldPoint, CameraPosition) * WorldPerPixelPerUnitDistance;
	return FMath::Max(WorldPerPixel * HandleDrawPixels * 2.0, 2.0);
}

FVector UPlaceRegionPenTool::ResolveCandidatePoint(const FVector& TracedHit, int32 IgnorePointIndex)
{
	FVector Result = TracedHit;
	FString Detail;

	const FModifierKeysState Mods = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().GetModifierKeys()
		: FModifierKeysState();

	// Shift constrains the segment being drawn to 15 degree steps off the previous point.
	if (Mods.IsShiftDown() && PendingPoints.Num() > 0 && IgnorePointIndex == INDEX_NONE)
	{
		const FVector& Prev = PendingPoints.Last();
		const FVector2D Delta(TracedHit.X - Prev.X, TracedHit.Y - Prev.Y);
		const double Length = Delta.Size();
		if (Length > UE_KINDA_SMALL_NUMBER)
		{
			const double AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
			const double SnappedDeg = FMath::RoundToDouble(AngleDeg / 15.0) * 15.0;
			const double Rad = FMath::DegreesToRadians(SnappedDeg);
			Result.X = Prev.X + FMath::Cos(Rad) * Length;
			Result.Y = Prev.Y + FMath::Sin(Rad) * Length;
			Detail = FString::Printf(TEXT("%.0f°"), SnappedDeg);
		}
	}

	if (Mods.IsAltDown())
	{
		if (Readout)
		{
			Readout->Detail = TEXT("snapping off (Alt)");
		}
		return Result;
	}

	PlaceLabelsEdit::FSnapSettings SnapSettings;
	SnapSettings.bToVertices = Settings ? Settings->bSnapToExistingVertices : true;
	SnapSettings.bToEdges = Settings ? Settings->bSnapToExistingEdges : true;
	SnapSettings.Radius = Settings ? Settings->SnapRadius : 0.0;

	PlaceLabelsEdit::FSnapQuery Query;
	Query.Regions = &CachedRegions;
	Query.OwnPoints = &PendingPoints;
	Query.IgnoreOwnIndex = IgnorePointIndex;

	const PlaceLabelsEdit::FSnapResult Snap =
		PlaceLabelsEdit::ResolveSnap(Result, SnapSettings, Query);

	if (Readout)
	{
		Readout->Detail = Snap.bSnapped
			? FString::Printf(TEXT("→ %s"), *Snap.Detail)
			: Detail;
	}
	return Snap.bSnapped ? Snap.Point : Result;
}

void UPlaceRegionPenTool::UpdateHoverState(const FVector& TracedHit)
{
	HoverKind = EHover::Ground;
	HoverPointIndex = INDEX_NONE;
	HoverSegmentIndex = INDEX_NONE;
	HoveredFootprint.Reset();
	HoveredFootprintLabel.Reset();

	const FVector2D XY(TracedHit.X, TracedHit.Y);
	const double PickRadius = PickRadiusAt(TracedHit);
	const double PickRadiusSq = PickRadius * PickRadius;

	TArray<FVector2D> Flat;
	Flat.Reserve(PendingPoints.Num());
	for (const FVector& P : PendingPoints)
	{
		Flat.Emplace(P.X, P.Y);
	}

	// A corner beats everything: it lies on two segments and, for the first corner, on the close target too.
	if (Flat.Num() > 0)
	{
		double CornerDistSq = TNumericLimits<double>::Max();
		const int32 Nearest = PlaceLabelsEdit::ClosestVertex(Flat, XY, CornerDistSq);
		if (Nearest != INDEX_NONE && CornerDistSq <= PickRadiusSq)
		{
			// Only the first corner closes the outline, and only once the polygon would be valid.
			HoverKind = (!bAwaitingConfirm && Nearest == 0 && PendingPoints.Num() >= 3)
				? EHover::CloseTarget
				: EHover::Point;
			HoverPointIndex = Nearest;
			HoverPoint = FVector(Flat[Nearest].X, Flat[Nearest].Y, TracedHit.Z);
			return;
		}
	}

	// Segments.
	if (Flat.Num() >= 2)
	{
		const PlaceLabelsEdit::FEdgeHit Edge =
			PlaceLabelsEdit::ClosestEdge(Flat, XY, /*bClosed*/ bAwaitingConfirm);
		if (Edge.EdgeIndex != INDEX_NONE && Edge.DistSq <= PickRadiusSq)
		{
			HoverKind = EHover::Segment;
			HoverSegmentIndex = Edge.EdgeIndex;
			HoverPoint = FVector(Edge.Point.X, Edge.Point.Y, TracedHit.Z);
			return;
		}
	}

	// A building footprint, but only with nothing drawn yet.
	if (PendingPoints.Num() == 0 && Settings && Settings->bTraceFootprints)
	{
		PlaceLabelsFootprint::FFootprintHit Footprint;
		if (PlaceLabelsFootprint::FindFootprintNear(GetTargetWorld(), TracedHit,
				FootprintSearchRadiusCm, Footprint))
		{
			HoverKind = EHover::Footprint;
			HoveredFootprint = MoveTemp(Footprint.Corners);
			HoveredFootprintLabel = MoveTemp(Footprint.Label);
			HoverPoint = TracedHit;
			if (Readout)
			{
				Readout->Detail = FString::Printf(TEXT("footprint: %s"), *HoveredFootprintLabel);
			}
			return;
		}
	}

	HoverPoint = bAwaitingConfirm ? TracedHit : ResolveCandidatePoint(TracedHit);
}

void UPlaceRegionPenTool::AdoptFootprint()
{
	if (HoveredFootprint.Num() < 3)
	{
		return;
	}

	PendingPoints = HoveredFootprint;
	HoveredFootprint.Reset();

	// Straight to the confirm stage.
	bAwaitingConfirm = true;
	bHoverValid = false;
	UpdateReadout();

	if (Settings)
	{
		NotifyOfPropertyChangeByTool(Settings);
	}
}

FInputRayHit UPlaceRegionPenTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	return GroundRayHit(PressPos);
}

void UPlaceRegionPenTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	FVector Hit;
	if (!TryHitGround(PressPos, Hit))
	{
		return;
	}

	PressPixel = PressPos.bHas2D ? PressPos.ScreenPosition : FVector2D::ZeroVector;
	bDragMoved = false;
	bDraggingPoint = false;
	DraggedPointIndex = INDEX_NONE;

	UpdateHoverState(Hit);

	// Pressing a corner starts a potential drag at either stage.
	if (HoverKind == EHover::Point || HoverKind == EHover::CloseTarget)
	{
		bDraggingPoint = true;
		DraggedPointIndex = HoverPointIndex;
	}
}

void UPlaceRegionPenTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	if (!bDraggingPoint || !PendingPoints.IsValidIndex(DraggedPointIndex))
	{
		return;
	}

	FVector Hit;
	if (!TryHitGround(DragPos, Hit))
	{
		return;
	}

	if (DragPos.bHas2D && FVector2D::Distance(DragPos.ScreenPosition, PressPixel) > ClickSlopPixels)
	{
		bDragMoved = true;
	}
	if (!bDragMoved)
	{
		return;
	}

	// The point being dragged is excluded from its own snap query, or it sticks to where it started and refuses to move.
	PendingPoints[DraggedPointIndex] = ResolveCandidatePoint(Hit, DraggedPointIndex);
	HoverPoint = PendingPoints[DraggedPointIndex];
	UpdateReadout();
}

void UPlaceRegionPenTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	const bool bWasDragging = bDraggingPoint;
	const bool bMoved = bDragMoved;
	const int32 DraggedIndex = DraggedPointIndex;

	bDraggingPoint = false;
	bDragMoved = false;
	DraggedPointIndex = INDEX_NONE;

	// A corner that actually moved is finished.
	if (bWasDragging && bMoved)
	{
		UpdateReadout();
		return;
	}

	FVector Hit;
	if (!TryHitGround(ReleasePos, Hit))
	{
		return;
	}

	const FModifierKeysState Mods = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().GetModifierKeys()
		: FModifierKeysState();

	UpdateHoverState(Hit);

	switch (HoverKind)
	{
	case EHover::CloseTarget:
		CloseOutline();
		return;

	case EHover::Point:
		// Alt+click removes a corner.
		if (Mods.IsAltDown())
		{
			DeleteHoveredPoint();
		}
		return;

	case EHover::Segment:
	{
		if (!PendingPoints.IsValidIndex(HoverSegmentIndex))
		{
			return;
		}
		// At the click position, not the midpoint: following a bend means putting the corner where the bend is.
		PendingPoints.Insert(HoverPoint, HoverSegmentIndex + 1);
		HoverKind = EHover::Point;
		HoverPointIndex = HoverSegmentIndex + 1;
		UpdateReadout();
		return;
	}

	case EHover::Footprint:
		AdoptFootprint();
		return;

	case EHover::Ground:
	default:
		break;
	}

	// The outline is finished and waiting to be named; a click on open ground must not extend it.
	if (bAwaitingConfirm)
	{
		return;
	}

	const FVector Candidate = ResolveCandidatePoint(Hit);

	// Reject a click that landed on the previous point; two coincident vertices produce a zero-length edge that every downstream predicate has to special-case.
	if (PendingPoints.Num() > 0)
	{
		const FVector& Prev = PendingPoints.Last();
		if (FVector2D::Distance(FVector2D(Prev.X, Prev.Y), FVector2D(Candidate.X, Candidate.Y))
			< PlaceLabelsEdit::DuplicatePointToleranceCm)
		{
			return;
		}
	}

	PendingPoints.Add(Candidate);
	HoverPoint = Candidate;
	bHoverValid = true;
	UpdateReadout();
}

void UPlaceRegionPenTool::OnTerminateDragSequence()
{
	bDraggingPoint = false;
	bDragMoved = false;
	DraggedPointIndex = INDEX_NONE;
}

FInputRayHit UPlaceRegionPenTool::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
	return GroundRayHit(PressPos);
}

void UPlaceRegionPenTool::OnBeginHover(const FInputDeviceRay& DevicePos)
{
	OnUpdateHover(DevicePos);
}

bool UPlaceRegionPenTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	if (bDraggingPoint)
	{
		return true;
	}

	FVector Hit;
	if (!TryHitGround(DevicePos, Hit))
	{
		bHoverValid = false;
		return false;
	}

	bHoverValid = true;
	UpdateHoverState(Hit);
	UpdateReadout();
	return true;
}

void UPlaceRegionPenTool::OnEndHover()
{
	bHoverValid = false;
	HoverKind = EHover::Ground;
	HoverPointIndex = INDEX_NONE;
	HoverSegmentIndex = INDEX_NONE;
	HoveredFootprint.Reset();
}

void UPlaceRegionPenTool::UndoLastPoint()
{
	if (PendingPoints.Num() == 0)
	{
		return;
	}

	// Backspace at the confirm stage reopens the outline.
	if (bAwaitingConfirm)
	{
		bAwaitingConfirm = false;
		UpdateReadout();
		return;
	}

	PendingPoints.Pop();
	HoverPointIndex = INDEX_NONE;
	HoverKind = EHover::Ground;
	UpdateReadout();
}

void UPlaceRegionPenTool::DeleteHoveredPoint()
{
	if (!PendingPoints.IsValidIndex(HoverPointIndex))
	{
		return;
	}

	// Three corners is the floor while the outline is closed.
	if (bAwaitingConfirm && PendingPoints.Num() <= 3)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("PenCannotDelete",
				"A region needs at least three corners. Press Esc to discard this outline instead."),
			EToolMessageLevel::UserWarning);
		return;
	}

	PendingPoints.RemoveAt(HoverPointIndex);
	HoverPointIndex = INDEX_NONE;
	HoverKind = EHover::Ground;
	UpdateReadout();
}

void UPlaceRegionPenTool::CancelDrawing()
{
	PendingPoints.Reset();
	HoverKind = EHover::Ground;
	HoverPointIndex = INDEX_NONE;
	HoverSegmentIndex = INDEX_NONE;
	HoveredFootprint.Reset();
	bAwaitingConfirm = false;
	UpdateReadout();
}

void UPlaceRegionPenTool::UpdateReadout()
{
	if (!Readout)
	{
		return;
	}

	TArray<FVector2D> Flat;
	Flat.Reserve(PendingPoints.Num());
	for (const FVector& P : PendingPoints)
	{
		Flat.Emplace(P.X, P.Y);
	}

	Readout->PointCount = Flat.Num();
	Readout->Perimeter = PlaceLabelsGeo::Perimeter2D(Flat);
	Readout->AreaSquareMetres = FMath::Abs(PlaceLabelsGeo::SignedArea2D(Flat)) / 10000.0;

	// No NotifyOfPropertyChangeByTool: that rebuilds the whole details panel, and this runs on every hover tick.
}

void UPlaceRegionPenTool::CloseOutline()
{
	if (bAwaitingConfirm || PendingPoints.Num() < 3)
	{
		return;
	}

	bAwaitingConfirm = true;
	bHoverValid = false;
	HoverKind = EHover::Ground;
	UpdateReadout();

	// Bring the panel forward: the name and type fields are what the user is being asked for now, and they may well have been edited by the tool since the panel last read them.
	if (Settings)
	{
		NotifyOfPropertyChangeByTool(Settings);
	}
}

void UPlaceRegionPenTool::ConfirmRegion()
{
	if (!bAwaitingConfirm || PendingPoints.Num() < 3)
	{
		return;
	}

	UInteractiveToolManager* ToolManager = GetToolManager();
	UWorld* World = GetTargetWorld();
	if (!World || !ToolManager)
	{
		return;
	}

	// Drop points that coincide with their predecessor before judging the polygon.
	TArray<FVector> Points;
	Points.Reserve(PendingPoints.Num());
	for (const FVector& P : PendingPoints)
	{
		if (Points.Num() == 0
			|| FVector2D::Distance(FVector2D(Points.Last().X, Points.Last().Y),
				FVector2D(P.X, P.Y)) >= PlaceLabelsEdit::DuplicatePointToleranceCm)
		{
			Points.Add(P);
		}
	}
	if (Points.Num() < 3)
	{
		return;
	}

	TArray<FVector2D> Flat;
	Flat.Reserve(Points.Num());
	for (const FVector& P : Points)
	{
		Flat.Emplace(P.X, P.Y);
	}
	if (FMath::Abs(PlaceLabelsGeo::SignedArea2D(Flat)) < PlaceLabelsEdit::MinPolygonAreaCmSq)
	{
		return;
	}

	// Actor origin at the footprint centre keeps LocalPoints small, which matters for float precision far from the world origin and gives the transform gizmo something to grab.
	FVector Origin = FVector::ZeroVector;
	for (const FVector& P : Points)
	{
		Origin += P;
	}
	Origin /= static_cast<double>(Points.Num());

	ULevel* Level = World->GetCurrentLevel();
	if (!Level)
	{
		return;
	}

	ToolManager->BeginUndoTransaction(LOCTEXT("DrawRegion", "Draw Place Region"));

	// Level->Modify() as well as RF_Transactional.
	Level->Modify();

	FActorSpawnParameters SpawnParams;
	SpawnParams.OverrideLevel = Level;
	SpawnParams.ObjectFlags = RF_Transactional;

	APlaceRegionActor* Actor = World->SpawnActor<APlaceRegionActor>(
		APlaceRegionActor::StaticClass(), FTransform(Origin), SpawnParams);

	if (Actor && Actor->Region)
	{
		UPlaceRegionComponent* Region = Actor->Region;
		Region->Modify();

		Region->LocalPoints.Reset(Points.Num());
		for (const FVector& P : Points)
		{
			Region->LocalPoints.Emplace(P.X - Origin.X, P.Y - Origin.Y);
		}

		// Normalise winding at author time so the sign of SignedArea2D means something to everything downstream.
		if (PlaceLabelsGeo::SignedArea2D(Region->LocalPoints) < 0.0)
		{
			Algo::Reverse(Region->LocalPoints);
		}

		Region->EnsureRegionId();

		if (Settings)
		{
			Region->Type = Settings->Type;
			Region->Name = Settings->Name;
			Region->Source = Settings->Source;
			Region->Confidence = Settings->Confidence;
			Region->bAutoParent = Settings->bAutoParent;

			if (Settings->bUseHeightBounds)
			{
				Region->bUseHeightBounds = true;
				Region->MinZ = Origin.Z - Settings->DepthBelowGround;
				Region->MaxZ = Origin.Z + Settings->HeightAboveGround;
			}
		}

		Region->RebuildCache();
		Region->UpdateBounds();

		// Inside the same transaction as the spawn, so Ctrl+Z takes the parent link with it.
		if (Region->bAutoParent)
		{
			Region->RecomputeDerivedParent();
		}

		Actor->SetActorLabel(Actor->GetDefaultActorLabel());

		// Filed away in the outliner so a city's worth of regions does not bury everything else.
		if (Settings && !Settings->OutlinerFolder.IsNone())
		{
			Actor->SetFolderPath(Settings->OutlinerFolder);
		}
	}

	ToolManager->EndUndoTransaction();

	PendingPoints.Reset();
	HoverKind = EHover::Ground;
	HoverPointIndex = INDEX_NONE;
	bAwaitingConfirm = false;

	// The name is cleared and the type is kept, which is not the same decision twice.
	if (Settings)
	{
		Settings->Name = FPlaceName();
		NotifyOfPropertyChangeByTool(Settings);
	}

	// Select what was just placed.
	if (GEditor && Actor)
	{
		LastPlacedActor = Actor;
		GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDeselectBSPSurfs*/ true);
		GEditor->SelectActor(Actor, /*bInSelected*/ true, /*bNotify*/ true);
	}

	RefreshCachedRegions();
	UpdateReadout();
}

void UPlaceRegionPenTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	if (!RenderAPI)
	{
		return;
	}
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI)
	{
		return;
	}

	// Cache the camera so hover picking sizes handles exactly the way they are drawn.
	{
		const FViewCameraState Camera = RenderAPI->GetCameraState();
		CameraPosition = Camera.Position;
		bCameraIsOrthographic = Camera.bIsOrthographic;

		double ViewWidthPx = 1920.0;
		if (const FSceneView* View = RenderAPI->GetSceneView())
		{
			ViewWidthPx = FMath::Max<double>(View->UnscaledViewRect.Width(), 1.0);
		}

		if (bCameraIsOrthographic)
		{
			WorldPerPixelOrtho = Camera.OrthoWorldCoordinateWidth / ViewWidthPx;
		}
		else
		{
			const double HalfFovRadians =
				FMath::DegreesToRadians(FMath::Clamp(Camera.HorizontalFOVDegrees, 1.0f, 170.0f) * 0.5f);
			WorldPerPixelPerUnitDistance = 2.0 * FMath::Tan(HalfFovRadians) / ViewWidthPx;
		}
	}

	// Every region already in the level, dimmed.
	if (Settings && Settings->bShowExistingRegions)
	{
		for (const TWeakObjectPtr<UPlaceRegionComponent>& Weak : CachedRegions)
		{
			const UPlaceRegionComponent* Region = Weak.Get();
			if (!Region)
			{
				continue;
			}

			const TArray<FVector2D>& Points = Region->GetWorldPoints2D();
			const int32 N = Points.Num();
			if (N < 3)
			{
				continue;
			}

			FLinearColor Color = Region->GetOutlineColor();
			Color.A = 0.4f;

			const double Z = Region->GetComponentLocation().Z;
			for (int32 i = 0, j = N - 1; i < N; j = i++)
			{
				PlaceLabelsEdit::DrawLine(PDI,
					FVector(Points[j].X, Points[j].Y, Z),
					FVector(Points[i].X, Points[i].Y, Z),
					Color, 2.0f);
			}
		}
	}

	// A building footprint about to be adopted, drawn as the region it would become.
	if (HoverKind == EHover::Footprint && HoveredFootprint.Num() >= 3)
	{
		const int32 N = HoveredFootprint.Num();
		for (int32 i = 0, j = N - 1; i < N; j = i++)
		{
			PlaceLabelsEdit::DrawLine(PDI, HoveredFootprint[j], HoveredFootprint[i],
				PlaceLabelsEdit::CloseColor, 4.0f);
		}
		for (const FVector& P : HoveredFootprint)
		{
			PlaceLabelsEdit::DrawCrossHandle(PDI, P, PlaceLabelsEdit::CloseColor,
				HandleSizeAt(P), 3.0f);
		}
		return;
	}

	if (PendingPoints.Num() == 0)
	{
		return;
	}

	// A confirmed outline reads in green and closed.
	const FLinearColor OutlineColor = bAwaitingConfirm
		? PlaceLabelsEdit::CloseColor
		: PlaceLabelsEdit::SettledColor;

	for (int32 i = 1; i < PendingPoints.Num(); ++i)
	{
		PlaceLabelsEdit::DrawLine(PDI, PendingPoints[i - 1], PendingPoints[i], OutlineColor, 5.0f);
	}

	if (bAwaitingConfirm && PendingPoints.Num() >= 3)
	{
		PlaceLabelsEdit::DrawLine(PDI, PendingPoints.Last(), PendingPoints[0], OutlineColor, 5.0f);
	}

	const FModifierKeysState Mods = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().GetModifierKeys()
		: FModifierKeysState();

	for (int32 i = 0; i < PendingPoints.Num(); ++i)
	{
		const FVector& P = PendingPoints[i];
		const bool bHovered = (HoverPointIndex == i)
			&& (HoverKind == EHover::Point || HoverKind == EHover::CloseTarget);

		// Alt over a corner shows the removal cross.
		if (bHovered && HoverKind == EHover::Point && Mods.IsAltDown())
		{
			PlaceLabelsEdit::DrawCrossOutHandle(PDI, P, PlaceLabelsEdit::DeleteColor,
				HandleSizeAt(P) * 1.5, 5.0f);
			continue;
		}

		const bool bClose = (HoverKind == EHover::CloseTarget && i == 0);
		PlaceLabelsEdit::DrawCrossHandle(PDI, P,
			bClose ? PlaceLabelsEdit::CloseColor : OutlineColor,
			HandleSizeAt(P) * (bHovered ? 1.7 : 1.0),
			bHovered ? 5.0f : 3.0f);
	}

	// Where a corner would be inserted.
	if (HoverKind == EHover::Segment && !bDraggingPoint)
	{
		PlaceLabelsEdit::DrawDiamondHandle(PDI, HoverPoint, PlaceLabelsEdit::InsertColor,
			HandleSizeAt(HoverPoint) * 1.4, 4.0f);
	}

	if (bAwaitingConfirm || bDraggingPoint)
	{
		return;
	}

	// The rubber band, plus the segment that would close the polygon — dashed and thinner, because neither exists yet.
	if (bHoverValid && HoverKind == EHover::Ground)
	{
		PlaceLabelsEdit::DrawDashedLine(PDI, PendingPoints.Last(), HoverPoint,
			PlaceLabelsEdit::PendingColor, 3.0f);

		if (PendingPoints.Num() >= 2)
		{
			PlaceLabelsEdit::DrawDashedLine(PDI, HoverPoint, PendingPoints[0],
				PlaceLabelsEdit::PendingColor, 2.0f);
		}
	}
}

void UPlaceRegionPenTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	if (Canvas == nullptr || RenderAPI == nullptr)
	{
		return;
	}
	const FSceneView* View = RenderAPI->GetSceneView();
	if (View == nullptr)
	{
		return;
	}

	// The cursor while drawing; the last corner once the outline is frozen and the cursor has stopped meaning anything.
	FVector Anchor;
	if (bHoverValid)
	{
		Anchor = HoverPoint;
	}
	else if (bAwaitingConfirm && PendingPoints.Num() > 0)
	{
		Anchor = PendingPoints.Last();
	}
	else
	{
		return;
	}

	FVector2D Pixel;
	if (!View->WorldToPixel(Anchor, Pixel))
	{
		return;
	}

	// WorldToPixel returns backbuffer pixels; FCanvas draws in DPI-independent units.
	const float DPIScale = FMath::Max(Canvas->GetDPIScale(), UE_KINDA_SMALL_NUMBER);
	Pixel /= DPIScale;

	// The Slate font, not UEngine::GetMediumFont().
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);

	FCanvasTextItem Prompt(FVector2D(Pixel.X + 16.0, Pixel.Y - 46.0),
		GetStagePromptText(), Font, PlaceLabelsEdit::SettledColor);
	Prompt.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Prompt);

	if (PendingPoints.Num() > 0)
	{
		FCanvasTextItem Summary(FVector2D(Pixel.X + 16.0, Pixel.Y - 28.0),
			GetDrawingSummaryText(), Font, FLinearColor::White);
		Summary.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Summary);
	}
}

FText UPlaceRegionPenTool::GetStagePromptText() const
{
	switch (HoverKind)
	{
	case EHover::Footprint:
		return FText::Format(
			LOCTEXT("PromptFootprint", "Click to trace the footprint of {0}."),
			FText::FromString(HoveredFootprintLabel));
	case EHover::CloseTarget:
		return LOCTEXT("PromptClose", "Click to close the outline. Drag to move this corner.");
	case EHover::Point:
		return LOCTEXT("PromptCorner", "Drag to move this corner. Alt+click, or Delete, removes it.");
	case EHover::Segment:
		return LOCTEXT("PromptSegment", "Click to add a corner here.");
	default:
		break;
	}

	if (bAwaitingConfirm)
	{
		// Deliberately does not say "press Enter" first.
		return LOCTEXT("PromptConfirm",
			"Outline set — corners are still draggable. Fill in the type and name in the New Region "
			"form, then click Create Region. Backspace reopens the outline; Esc discards it. (Enter "
			"also places it, once you have clicked back in the viewport.)");
	}
	if (PendingPoints.Num() == 0)
	{
		return LOCTEXT("PromptStart", "Click the ground to start the region outline.");
	}
	if (PendingPoints.Num() < 3)
	{
		return LOCTEXT("PromptMore", "Keep clicking corners. Backspace removes the last point.");
	}
	return LOCTEXT("PromptFinish",
		"Click the first point or press Enter to close the outline. Backspace removes the last point.");
}

FText UPlaceRegionPenTool::GetDrawingSummaryText() const
{
	if (!Readout)
	{
		return FText::GetEmpty();
	}

	FString Line = FString::Printf(TEXT("%d points · %.0f m² · %.0f cm"),
		Readout->PointCount, Readout->AreaSquareMetres, Readout->Perimeter);

	if (!Readout->Detail.IsEmpty())
	{
		Line += FString::Printf(TEXT("  %s"), *Readout->Detail);
	}
	return FText::FromString(Line);
}

UInteractiveTool* UPlaceRegionPenToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UPlaceRegionPenTool>(SceneState.ToolManager);
}

#undef LOCTEXT_NAMESPACE
