#include "Tools/PlaceRegionEditTool.h"

#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "InteractiveToolManager.h"
#include "ToolContextInterfaces.h"
#include "PlaceLabelGeometry.h"
#include "PlaceLabelsTopology.h"
#include "PlaceRegionComponent.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "PlaceRegionEditTool"

namespace
{
	// Handles are sized and picked in pixels, then converted to world units per point.
	constexpr double HandlePickPixels = 13.0;
	constexpr double HandleDrawPixels = 9.0;
	constexpr double InsertDrawPixels = 7.0;

	// Below this the polygon stops being one.
	constexpr int32 MinCorners = 3;
}

void UPlaceRegionEditToolProperties::WeldToNeighbours()
{
	if (UPlaceRegionEditTool* Tool = OwningTool.Get())
	{
		Tool->WeldTargetToNeighbours();
	}
}

void UPlaceRegionEditTargetProperties::DeleteSelectedCorners()
{
	if (UPlaceRegionEditTool* Tool = OwningTool.Get())
	{
		Tool->DeleteSelectedCorners();
	}
}

void UPlaceRegionEditTargetProperties::DoneEditing()
{
	if (UPlaceRegionEditTool* Tool = OwningTool.Get())
	{
		Tool->ClearTarget();
	}
}

#if WITH_EDITOR
void UPlaceRegionEditTargetProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Interactive is a slider or colour picker still being dragged.
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}
	if (UPlaceRegionEditTool* Tool = OwningTool.Get())
	{
		// Which field, not all of them: the same region is also edited from the mode panel's form,
		// and writing back a whole snapshot would carry two stale fields over that edit.
		Tool->ApplyTargetProperties(PropertyChangedEvent.GetPropertyName());
	}
}
#endif

void UPlaceRegionEditTool::Setup()
{
	UInteractiveTool::Setup();

	ClickDragBehavior = NewObject<UClickDragInputBehavior>(this);
	ClickDragBehavior->Initialize(this);

	// Without this the release is hit-tested again, and a release over nothing.
	ClickDragBehavior->bUpdateModifiersDuringDrag = true;
	AddInputBehavior(ClickDragBehavior);

	HoverBehavior = NewObject<UMouseHoverBehavior>(this);
	HoverBehavior->Initialize(this);
	AddInputBehavior(HoverBehavior);

	Settings = NewObject<UPlaceRegionEditToolProperties>(this);
	RegisterSettings(Settings);
	Settings->OwningTool = this;

	// Output-ish: it mirrors whichever region is being edited.
	TargetPanel = NewObject<UPlaceRegionEditTargetProperties>(this);
	RegisterSettings(TargetPanel, /*bPersist*/ false);
	TargetPanel->OwningTool = this;

	RefreshCachedRegions();
	RefreshSeams();
}

void UPlaceRegionEditTool::Shutdown(EToolShutdownType ShutdownType)
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

void UPlaceRegionEditTool::RegisterSettings(UInteractiveToolPropertySet* PropertySet, bool bPersist)
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

UWorld* UPlaceRegionEditTool::GetTargetWorld() const
{
	return GetToolManager() && GetToolManager()->GetContextQueriesAPI()
		? GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld()
		: nullptr;
}

void UPlaceRegionEditTool::OnTick(float DeltaTime)
{
	TimeSinceCacheRefresh += DeltaTime;
	if (TimeSinceCacheRefresh < 1.5)
	{
		return;
	}
	TimeSinceCacheRefresh = 0.0;

	// Not mid-drag: rebuilding the snap set while a corner is following the cursor would change what it snaps to underneath the hand holding it.
	if (bDragging)
	{
		return;
	}

	RefreshCachedRegions();
	RefreshSeams();

	// The mode panel's form edits the same region, so this snapshot goes stale between clicks.
	if (TargetRegion.IsValid())
	{
		PushTargetToPanel();
	}

	// The region being edited can be deleted out from under the tool, and a dangling target leaves the panel describing a place that is no longer there.
	if (!TargetRegion.IsValid() && TargetPanel && TargetPanel->CornerCount > 0)
	{
		ClearTarget();
		PushTargetToPanel();
	}
}

void UPlaceRegionEditTool::RefreshCachedRegions()
{
	// Snapping wants something to snap to.
	PlaceLabelsEdit::GatherRegions(GetTargetWorld(), CachedRegions, /*bOnlyWithUsableOutline*/ true);
}

void UPlaceRegionEditTool::RefreshSeams()
{
	CachedSeams.Reset();
	if (Settings && Settings->bHighlightSeams)
	{
		PlaceLabelsTopology::FindSeamIssues(CachedRegions, Settings->WeldTolerance, CachedSeams);
	}
}

bool UPlaceRegionEditTool::TraceGround(const FInputDeviceRay& Ray, FVector& OutHit) const
{
	const double FallbackZ = TargetRegion.IsValid()
		? TargetRegion->GetComponentLocation().Z
		: 0.0;
	return PlaceLabelsEdit::TraceGround(GetTargetWorld(), Ray.WorldRay.Origin,
		Ray.WorldRay.Direction, FallbackZ, OutHit);
}

double UPlaceRegionEditTool::PickRadiusAt(const FVector& WorldPoint) const
{
	const double WorldPerPixel = bCameraIsOrthographic
		? WorldPerPixelOrtho
		: FVector::Dist(WorldPoint, CameraPosition) * WorldPerPixelPerUnitDistance;
	return FMath::Max(WorldPerPixel * HandlePickPixels, 1.0);
}

double UPlaceRegionEditTool::HandleSizeAt(const FVector& WorldPoint) const
{
	const double WorldPerPixel = bCameraIsOrthographic
		? WorldPerPixelOrtho
		: FVector::Dist(WorldPoint, CameraPosition) * WorldPerPixelPerUnitDistance;
	return FMath::Max(WorldPerPixel * HandleDrawPixels * 2.0, 2.0);
}

UPlaceRegionComponent* UPlaceRegionEditTool::PickRegionAt(const FVector& WorldPoint) const
{
	const FVector2D XY(WorldPoint.X, WorldPoint.Y);

	// Containment first, smallest wins.
	UPlaceRegionComponent* BestContaining = nullptr;
	double BestArea = TNumericLimits<double>::Max();

	UPlaceRegionComponent* BestNear = nullptr;
	double BestNearDistSq = TNumericLimits<double>::Max();
	const double NearRadius = PickRadiusAt(WorldPoint);

	for (const TWeakObjectPtr<UPlaceRegionComponent>& Weak : CachedRegions)
	{
		UPlaceRegionComponent* Region = Weak.Get();
		if (!Region)
		{
			continue;
		}

		if (Region->ContainsWorldPoint2D(XY) && Region->GetWorldArea() < BestArea)
		{
			BestArea = Region->GetWorldArea();
			BestContaining = Region;
		}

		// A region you are on the edge of but not inside still has to be clickable, or the outline of a region drawn round a courtyard is unselectable from the courtyard.
		const PlaceLabelsEdit::FEdgeHit Hit =
			PlaceLabelsEdit::ClosestEdge(Region->GetWorldPoints2D(), XY, /*bClosed*/ true);
		if (Hit.EdgeIndex != INDEX_NONE && Hit.DistSq < BestNearDistSq
			&& Hit.DistSq <= NearRadius * NearRadius)
		{
			BestNearDistSq = Hit.DistSq;
			BestNear = Region;
		}
	}

	return BestContaining ? BestContaining : BestNear;
}

void UPlaceRegionEditTool::SetTarget(UPlaceRegionComponent* Region)
{
	if (TargetRegion.Get() == Region)
	{
		return;
	}
	TargetRegion = Region;
	SelectedCorners.Reset();
	HoverKind = EHover::None;
	HoverCorner = INDEX_NONE;
	HoverEdge = INDEX_NONE;
	PushTargetToPanel();
}

void UPlaceRegionEditTool::ClearTarget()
{
	SetTarget(nullptr);
}

void UPlaceRegionEditTool::PushTargetToPanel()
{
	if (!TargetPanel)
	{
		return;
	}

	const UPlaceRegionComponent* Region = TargetRegion.Get();
	if (!Region)
	{
		TargetPanel->RegionLabel = TEXT("none — click a region to edit it");
		TargetPanel->Name = FPlaceName();
		TargetPanel->Type = nullptr;
		TargetPanel->Note = FText::GetEmpty();
		TargetPanel->CornerCount = 0;
		TargetPanel->AreaSquareMetres = 0.0;
	}
	else
	{
		TargetPanel->RegionLabel = Region->GetOwner()
			? Region->GetOwner()->GetActorLabel()
			: Region->GetName();
		TargetPanel->Name = Region->Name;
		TargetPanel->Type = Region->Type;
		TargetPanel->Note = Region->Note;
		TargetPanel->CornerCount = Region->LocalPoints.Num();
		TargetPanel->AreaSquareMetres = Region->GetWorldArea() / 10000.0;
	}

	// A genuine stage change — which region the panel is describing — so the details view does have to be rebuilt.
	NotifyOfPropertyChangeByTool(TargetPanel);
}

void UPlaceRegionEditTool::ApplyTargetProperties(FName ChangedProperty)
{
	UPlaceRegionComponent* Region = TargetRegion.Get();
	if (!Region || !TargetPanel)
	{
		return;
	}

	UInteractiveToolManager* ToolManager = GetToolManager();
	if (!ToolManager)
	{
		return;
	}

	// NAME_None means "everything", which is what the tool's own callers want.
	auto Wants = [&ChangedProperty](const TCHAR* Field)
	{
		return ChangedProperty.IsNone() || ChangedProperty == FName(Field);
	};

	// FPlaceName's three FTexts arrive as the inner property's name, not as "Name".
	const bool bNameChanged = Wants(TEXT("Name")) || ChangedProperty == FName(TEXT("Chinese"))
		|| ChangedProperty == FName(TEXT("Pinyin")) || ChangedProperty == FName(TEXT("English"));

	ToolManager->BeginUndoTransaction(LOCTEXT("EditRegionLabel", "Edit Region Label"));
	Region->Modify();

	const bool bTypeChanged = Wants(TEXT("Type")) && Region->Type != TargetPanel->Type;

	if (bNameChanged) { Region->Name = TargetPanel->Name; }
	if (Wants(TEXT("Type"))) { Region->Type = TargetPanel->Type; }
	if (Wants(TEXT("Note"))) { Region->Note = TargetPanel->Note; }

	if (AActor* Owner = Region->GetOwner())
	{
		Owner->Modify();
		Owner->SetActorLabel(Owner->GetDefaultActorLabel());
	}

	// The type drives the parenting rule, so changing it can change what this region hangs off.
	if (bTypeChanged && Region->bAutoParent && !Region->ExplicitParent)
	{
		Region->RecomputeDerivedParent();
	}

	// The outline colour is a function of the type and of whether anything is missing, and both just moved.
	Region->MarkRenderStateDirty();

	ToolManager->EndUndoTransaction();
}

void UPlaceRegionEditTool::BeginPointEdit()
{
	PreEditLocalPoints.Reset();
	if (const UPlaceRegionComponent* Region = TargetRegion.Get())
	{
		PreEditLocalPoints = Region->LocalPoints;
	}
}

void UPlaceRegionEditTool::CommitPointEdit(const FText& TransactionName)
{
	UPlaceRegionComponent* Region = TargetRegion.Get();
	UInteractiveToolManager* ToolManager = GetToolManager();
	if (!Region || !ToolManager || PreEditLocalPoints.Num() == 0)
	{
		return;
	}

	// The edit has already been applied straight to LocalPoints so the viewport could show it happening.
	TArray<FVector2D> Final = Region->LocalPoints;
	if (Final == PreEditLocalPoints)
	{
		PreEditLocalPoints.Reset();
		return;
	}

	Region->LocalPoints = PreEditLocalPoints;

	ToolManager->BeginUndoTransaction(TransactionName);
	Region->Modify();
	if (AActor* Owner = Region->GetOwner())
	{
		Owner->Modify();
	}

	Region->LocalPoints = MoveTemp(Final);
	Region->RebuildCache();
	Region->UpdateBounds();

	if (Region->bAutoParent && !Region->ExplicitParent)
	{
		Region->RecomputeDerivedParent();
	}

	ToolManager->EndUndoTransaction();

	PreEditLocalPoints.Reset();

	if (TargetPanel)
	{
		TargetPanel->CornerCount = Region->LocalPoints.Num();
		TargetPanel->AreaSquareMetres = Region->GetWorldArea() / 10000.0;
	}
	RefreshSeams();
}

void UPlaceRegionEditTool::DeleteSelectedCorners()
{
	UPlaceRegionComponent* Region = TargetRegion.Get();
	if (!Region || SelectedCorners.Num() == 0)
	{
		return;
	}

	// Refusing to make a degenerate polygon beats repairing one afterwards, and the author gets to see which corners they had selected.
	if (Region->LocalPoints.Num() - SelectedCorners.Num() < MinCorners)
	{
		GetToolManager()->DisplayMessage(
			LOCTEXT("CannotDeleteCorners",
				"A region needs at least three corners — delete fewer, or delete the whole region."),
			EToolMessageLevel::UserWarning);
		return;
	}

	BeginPointEdit();

	TArray<int32> Indices = SelectedCorners.Array();
	// Descending, so an earlier removal never shifts an index still to be removed.
	Indices.Sort([](int32 A, int32 B) { return A > B; });
	for (int32 Index : Indices)
	{
		if (Region->LocalPoints.IsValidIndex(Index))
		{
			Region->LocalPoints.RemoveAt(Index);
		}
	}

	Region->RebuildCache();
	Region->UpdateBounds();
	CommitPointEdit(LOCTEXT("DeleteCorners", "Delete Region Corners"));

	SelectedCorners.Reset();
	HoverCorner = INDEX_NONE;
	HoverKind = EHover::None;
}

void UPlaceRegionEditTool::SelectAllCorners()
{
	const UPlaceRegionComponent* Region = TargetRegion.Get();
	if (!Region)
	{
		return;
	}
	SelectedCorners.Reset();
	for (int32 i = 0; i < Region->LocalPoints.Num(); ++i)
	{
		SelectedCorners.Add(i);
	}
}

void UPlaceRegionEditTool::WeldTargetToNeighbours()
{
	UPlaceRegionComponent* Region = TargetRegion.Get();
	UInteractiveToolManager* ToolManager = GetToolManager();
	if (!Region || !ToolManager || !Settings)
	{
		return;
	}

	// Only the neighbours whose bounds come within tolerance.
	TArray<UPlaceRegionComponent*> Group;
	Group.Add(Region);

	const FBox2D Reach = Region->GetWorldBounds2D().ExpandBy(Settings->WeldTolerance);
	for (const TWeakObjectPtr<UPlaceRegionComponent>& Weak : CachedRegions)
	{
		UPlaceRegionComponent* Other = Weak.Get();
		if (Other && Other != Region && Reach.Intersect(Other->GetWorldBounds2D()))
		{
			Group.Add(Other);
		}
	}

	if (Group.Num() < 2)
	{
		ToolManager->DisplayMessage(
			LOCTEXT("NothingToWeld", "Nothing within welding distance of this region."),
			EToolMessageLevel::UserMessage);
		return;
	}

	PlaceLabelsTopology::FWeldReport Report;

	ToolManager->BeginUndoTransaction(LOCTEXT("WeldNeighbours", "Weld Region Boundaries"));
	PlaceLabelsTopology::WeldBoundaries(Group, Settings->WeldTolerance, Report);
	ToolManager->EndUndoTransaction();

	if (Report.RegionsChanged == 0)
	{
		ToolManager->DisplayMessage(
			LOCTEXT("AlreadyWelded", "These boundaries already meet exactly."),
			EToolMessageLevel::UserMessage);
	}
	else
	{
		ToolManager->DisplayMessage(
			FText::Format(
				LOCTEXT("WeldDone", "Welded {0} region(s): {1} corner(s) moved, {2} added, {3} removed."),
				FText::AsNumber(Report.RegionsChanged), FText::AsNumber(Report.CornersMoved),
				FText::AsNumber(Report.CornersInserted), FText::AsNumber(Report.CornersRemoved)),
			EToolMessageLevel::UserMessage);
	}

	SelectedCorners.Reset();
	RefreshSeams();
	PushTargetToPanel();
}

void UPlaceRegionEditTool::UpdateHoverState(const FVector& WorldHit)
{
	HoverKind = EHover::None;
	HoverCorner = INDEX_NONE;
	HoverEdge = INDEX_NONE;
	HoverRegion = nullptr;
	HoverPoint = WorldHit;
	SnapDetail.Reset();

	const UPlaceRegionComponent* Region = TargetRegion.Get();
	if (!Region)
	{
		if (UPlaceRegionComponent* Under = PickRegionAt(WorldHit))
		{
			HoverKind = EHover::OtherRegion;
			HoverRegion = Under;
		}
		return;
	}

	const FVector2D XY(WorldHit.X, WorldHit.Y);
	const TArray<FVector2D>& World = Region->GetWorldPoints2D();
	const double PickRadius = PickRadiusAt(WorldHit);
	const double PickRadiusSq = PickRadius * PickRadius;

	// Corner beats edge: a corner is on two edges, and at a corner the edge insert handle and the corner handle sit on top of one another.
	double CornerDistSq = TNumericLimits<double>::Max();
	const int32 NearestCorner = PlaceLabelsEdit::ClosestVertex(World, XY, CornerDistSq);
	if (NearestCorner != INDEX_NONE && CornerDistSq <= PickRadiusSq)
	{
		HoverKind = EHover::Corner;
		HoverCorner = NearestCorner;
		HoverPoint = FVector(World[NearestCorner].X, World[NearestCorner].Y, WorldHit.Z);
		return;
	}

	const PlaceLabelsEdit::FEdgeHit Edge =
		PlaceLabelsEdit::ClosestEdge(World, XY, /*bClosed*/ true);
	if (Edge.EdgeIndex != INDEX_NONE && Edge.DistSq <= PickRadiusSq)
	{
		HoverKind = EHover::Edge;
		HoverEdge = Edge.EdgeIndex;
		HoverPoint = FVector(Edge.Point.X, Edge.Point.Y, WorldHit.Z);
		return;
	}

	// Nowhere near this region's boundary — is there another one under the cursor to switch to?
	if (UPlaceRegionComponent* Under = PickRegionAt(WorldHit))
	{
		if (Under != Region)
		{
			HoverKind = EHover::OtherRegion;
			HoverRegion = Under;
		}
	}
}

FInputRayHit UPlaceRegionEditTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	FVector Hit;
	if (!TraceGround(PressPos, Hit))
	{
		return FInputRayHit();
	}
	return FInputRayHit(static_cast<float>((Hit - PressPos.WorldRay.Origin).Size()));
}

void UPlaceRegionEditTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	FVector Hit;
	if (!TraceGround(PressPos, Hit))
	{
		return;
	}

	PressPixel = PressPos.bHas2D ? PressPos.ScreenPosition : FVector2D::ZeroVector;
	bDragMoved = false;
	bDragging = false;
	DragStartWorld = Hit;

	UpdateHoverState(Hit);

	UPlaceRegionComponent* Region = TargetRegion.Get();
	if (!Region || HoverKind != EHover::Corner)
	{
		return;
	}

	const FModifierKeysState Mods = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().GetModifierKeys()
		: FModifierKeysState();

	// Alt+click removes a corner, and the removal lands on the release. Beginning a drag here
	// would make the release early-out and the gesture unreachable.
	if (Mods.IsAltDown())
	{
		return;
	}

	// Ctrl extends the selection; a plain press on an unselected corner replaces it.
	if (Mods.IsControlDown())
	{
		if (SelectedCorners.Contains(HoverCorner))
		{
			SelectedCorners.Remove(HoverCorner);
		}
		else
		{
			SelectedCorners.Add(HoverCorner);
		}
	}
	else if (!SelectedCorners.Contains(HoverCorner))
	{
		SelectedCorners.Reset();
		SelectedCorners.Add(HoverCorner);
	}

	if (SelectedCorners.Contains(HoverCorner))
	{
		bDragging = true;
		DragStartLocalPoints = Region->LocalPoints;
		BeginPointEdit();
	}
}

void UPlaceRegionEditTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	UPlaceRegionComponent* Region = TargetRegion.Get();
	if (!bDragging || !Region || !Region->LocalPoints.IsValidIndex(HoverCorner))
	{
		return;
	}

	FVector Hit;
	if (!TraceGround(DragPos, Hit))
	{
		return;
	}

	if (DragPos.bHas2D
		&& FVector2D::Distance(DragPos.ScreenPosition, PressPixel) > PlaceLabelsEdit::ClickSlopPixels)
	{
		bDragMoved = true;
	}
	if (!bDragMoved)
	{
		return;
	}

	const FModifierKeysState Mods = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().GetModifierKeys()
		: FModifierKeysState();

	FVector Target = Hit;
	SnapDetail.Reset();

	// Snapping applies to the corner actually under the cursor.
	if (!Mods.IsAltDown() && Settings)
	{
		TArray<FVector> OwnWorld;
		PlaceLabelsEdit::GetRegionWorldPoints3D(Region, OwnWorld);

		PlaceLabelsEdit::FSnapSettings SnapSettings;
		SnapSettings.bToVertices = Settings->bSnapToVertices;
		SnapSettings.bToEdges = Settings->bSnapToEdges;
		SnapSettings.Radius = Settings->SnapRadius;

		PlaceLabelsEdit::FSnapQuery Query;
		Query.Regions = &CachedRegions;
		Query.Exclude = Region;
		Query.OwnPoints = &OwnWorld;
		Query.IgnoreOwnIndex = HoverCorner;

		const PlaceLabelsEdit::FSnapResult Snap =
			PlaceLabelsEdit::ResolveSnap(Hit, SnapSettings, Query);
		Target = Snap.Point;
		if (Snap.bSnapped)
		{
			SnapDetail = Snap.Detail;
		}
	}
	else if (Mods.IsAltDown())
	{
		SnapDetail = TEXT("snapping off (Alt)");
	}

	const FTransform& Xf = Region->GetComponentTransform();
	const FVector LocalTarget = Xf.InverseTransformPosition(Target);
	const FVector2D NewPoint(LocalTarget.X, LocalTarget.Y);

	if (!DragStartLocalPoints.IsValidIndex(HoverCorner))
	{
		return;
	}
	const FVector2D Delta = NewPoint - DragStartLocalPoints[HoverCorner];

	for (int32 Index : SelectedCorners)
	{
		if (Region->LocalPoints.IsValidIndex(Index) && DragStartLocalPoints.IsValidIndex(Index))
		{
			Region->LocalPoints[Index] = DragStartLocalPoints[Index] + Delta;
		}
	}
	// The dragged corner itself lands exactly on the snap target.
	Region->LocalPoints[HoverCorner] = NewPoint;

	Region->RebuildCache();
	Region->UpdateBounds();

	HoverPoint = Target;
}

void UPlaceRegionEditTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	FVector Hit;
	const bool bHitGround = TraceGround(ReleasePos, Hit);

	if (bDragging)
	{
		if (bDragMoved)
		{
			CommitPointEdit(LOCTEXT("MoveCorner", "Move Region Corner"));
		}
		else
		{
			// A press with no movement on a corner is a selection, which OnClickPress already did.
			PreEditLocalPoints.Reset();
		}
		bDragging = false;
		bDragMoved = false;
		return;
	}

	if (!bHitGround)
	{
		return;
	}

	const FModifierKeysState Mods = FSlateApplication::IsInitialized()
		? FSlateApplication::Get().GetModifierKeys()
		: FModifierKeysState();

	UpdateHoverState(Hit);

	UPlaceRegionComponent* Region = TargetRegion.Get();

	// No region in hand: this click picks one.
	if (!Region)
	{
		if (UPlaceRegionComponent* Picked = PickRegionAt(Hit))
		{
			SetTarget(Picked);
		}
		return;
	}

	switch (HoverKind)
	{
	case EHover::Corner:
	{
		// Alt-click removes.
		if (Mods.IsAltDown())
		{
			SelectedCorners.Reset();
			SelectedCorners.Add(HoverCorner);
			DeleteSelectedCorners();
		}
		break;
	}

	case EHover::Edge:
	{
		if (!Region->LocalPoints.IsValidIndex(HoverEdge))
		{
			break;
		}

		BeginPointEdit();

		const FTransform& Xf = Region->GetComponentTransform();
		const FVector Local = Xf.InverseTransformPosition(HoverPoint);

		// At the click position, not at the midpoint.
		const int32 NewIndex = HoverEdge + 1;
		Region->LocalPoints.Insert(FVector2D(Local.X, Local.Y), NewIndex);
		Region->RebuildCache();
		Region->UpdateBounds();

		CommitPointEdit(LOCTEXT("InsertCorner", "Insert Region Corner"));

		SelectedCorners.Reset();
		SelectedCorners.Add(NewIndex);
		HoverKind = EHover::Corner;
		HoverCorner = NewIndex;
		break;
	}

	case EHover::OtherRegion:
		SetTarget(HoverRegion.Get());
		break;

	case EHover::None:
	default:
		// Clicking clear ground drops the region rather than doing nothing, so getting out is the same gesture as getting in.
		SelectedCorners.Reset();
		ClearTarget();
		break;
	}
}

void UPlaceRegionEditTool::OnTerminateDragSequence()
{
	// Aborted mid-drag — put the geometry back the way it was rather than leaving it wherever the cursor happened to die.
	if (bDragging && bDragMoved)
	{
		if (UPlaceRegionComponent* Region = TargetRegion.Get())
		{
			if (PreEditLocalPoints.Num() > 0)
			{
				Region->LocalPoints = PreEditLocalPoints;
				Region->RebuildCache();
				Region->UpdateBounds();
			}
		}
	}
	PreEditLocalPoints.Reset();
	bDragging = false;
	bDragMoved = false;
}

FInputRayHit UPlaceRegionEditTool::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
	FVector Hit;
	if (!TraceGround(PressPos, Hit))
	{
		return FInputRayHit();
	}
	return FInputRayHit(static_cast<float>((Hit - PressPos.WorldRay.Origin).Size()));
}

void UPlaceRegionEditTool::OnBeginHover(const FInputDeviceRay& DevicePos)
{
	OnUpdateHover(DevicePos);
}

bool UPlaceRegionEditTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	if (bDragging)
	{
		return true;
	}

	FVector Hit;
	if (!TraceGround(DevicePos, Hit))
	{
		bHoverValid = false;
		return false;
	}

	bHoverValid = true;
	UpdateHoverState(Hit);
	return true;
}

void UPlaceRegionEditTool::OnEndHover()
{
	bHoverValid = false;
	HoverKind = EHover::None;
}

void UPlaceRegionEditTool::Render(IToolsContextRenderAPI* RenderAPI)
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

	const UPlaceRegionComponent* Target = TargetRegion.Get();

	// Every other region, dimmed.
	if (Settings && Settings->bShowOtherRegions)
	{
		for (const TWeakObjectPtr<UPlaceRegionComponent>& Weak : CachedRegions)
		{
			const UPlaceRegionComponent* Region = Weak.Get();
			if (!Region || Region == Target)
			{
				continue;
			}

			const TArray<FVector2D>& Points = Region->GetWorldPoints2D();
			const int32 N = Points.Num();
			if (N < 3)
			{
				continue;
			}

			const bool bHovered = (HoverRegion.Get() == Region);
			FLinearColor Color = Region->GetOutlineColor();
			Color.A = bHovered ? 0.95f : 0.35f;

			const double Z = Region->GetComponentLocation().Z;
			for (int32 i = 0, j = N - 1; i < N; j = i++)
			{
				PlaceLabelsEdit::DrawLine(PDI,
					FVector(Points[j].X, Points[j].Y, Z),
					FVector(Points[i].X, Points[i].Y, Z),
					Color, bHovered ? 4.0f : 2.0f);
			}
		}
	}

	// Seams: every boundary pair that nearly meets.
	if (Settings && Settings->bHighlightSeams)
	{
		for (const PlaceLabelsTopology::FSeamIssue& Seam : CachedSeams)
		{
			const UPlaceRegionComponent* A = Seam.A.Get();
			const UPlaceRegionComponent* B = Seam.B.Get();
			if (!A || !B)
			{
				continue;
			}

			const double Z = A->GetComponentLocation().Z;

			// The same call the report came from.
			TArray<PlaceLabelsTopology::FSeamMark> Marks;
			PlaceLabelsTopology::FindSeamMarks(A->GetWorldPoints2D(), B->GetWorldPoints2D(),
				Settings->WeldTolerance, Marks);

			for (const PlaceLabelsTopology::FSeamMark& Mark : Marks)
			{
				const FVector From(Mark.From.X, Mark.From.Y, Z);
				const FVector To(Mark.To.X, Mark.To.Y, Z);

				// The gap itself is far too small to see from map height.
				PlaceLabelsEdit::DrawCrossOutHandle(PDI, From, PlaceLabelsEdit::WarningColor,
					HandleSizeAt(From) * 1.2, 3.0f);
				PlaceLabelsEdit::DrawCrossOutHandle(PDI, To, PlaceLabelsEdit::WarningColor,
					HandleSizeAt(To) * 1.2, 3.0f);
				PlaceLabelsEdit::DrawLine(PDI, From, To, PlaceLabelsEdit::WarningColor, 3.0f);
			}
		}
	}

	if (!Target)
	{
		return;
	}

	// The region in hand, at full strength with its corners exposed.
	const TArray<FVector2D>& Points = Target->GetWorldPoints2D();
	const int32 N = Points.Num();
	if (N < 2)
	{
		return;
	}
	const double Z = Target->GetComponentLocation().Z;

	for (int32 i = 0, j = N - 1; i < N; j = i++)
	{
		PlaceLabelsEdit::DrawLine(PDI,
			FVector(Points[j].X, Points[j].Y, Z),
			FVector(Points[i].X, Points[i].Y, Z),
			PlaceLabelsEdit::SettledColor, 4.0f);
	}

	for (int32 i = 0; i < N; ++i)
	{
		const FVector P(Points[i].X, Points[i].Y, Z);
		const bool bSelected = SelectedCorners.Contains(i);
		const bool bHovered = (HoverKind == EHover::Corner && HoverCorner == i);

		const FModifierKeysState Mods = FSlateApplication::IsInitialized()
			? FSlateApplication::Get().GetModifierKeys()
			: FModifierKeysState();

		// Hovering a corner with Alt held shows the removal cross.
		if (bHovered && Mods.IsAltDown())
		{
			PlaceLabelsEdit::DrawCrossOutHandle(PDI, P, PlaceLabelsEdit::DeleteColor,
				HandleSizeAt(P) * 1.5, 5.0f);
			continue;
		}

		const FLinearColor Color = bSelected ? PlaceLabelsEdit::SelectedColor
											 : PlaceLabelsEdit::SettledColor;
		const double Size = HandleSizeAt(P) * (bHovered ? 1.6 : bSelected ? 1.3 : 1.0);
		PlaceLabelsEdit::DrawCrossHandle(PDI, P, Color, Size, bHovered ? 5.0f : 3.0f);
	}

	// Where a corner would be inserted.
	if (HoverKind == EHover::Edge && !bDragging)
	{
		PlaceLabelsEdit::DrawDiamondHandle(PDI, HoverPoint, PlaceLabelsEdit::InsertColor,
			HandleSizeAt(HoverPoint) * (InsertDrawPixels / HandleDrawPixels) * 1.6, 4.0f);
	}
}

void UPlaceRegionEditTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	if (!Canvas || !RenderAPI || !bHoverValid)
	{
		return;
	}
	const FSceneView* View = RenderAPI->GetSceneView();
	if (!View)
	{
		return;
	}

	FVector2D Pixel;
	if (!View->WorldToPixel(HoverPoint, Pixel))
	{
		return;
	}

	// WorldToPixel returns backbuffer pixels; FCanvas draws in DPI-independent units.
	const float DPIScale = FMath::Max(Canvas->GetDPIScale(), UE_KINDA_SMALL_NUMBER);
	Pixel /= DPIScale;

	// The Slate font rather than UEngine::GetMediumFont().
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 10);

	FCanvasTextItem Prompt(FVector2D(Pixel.X + 16.0, Pixel.Y - 40.0), GetStagePromptText(), Font,
		PlaceLabelsEdit::SettledColor);
	Prompt.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Prompt);

	const FText Status = GetStatusText();
	if (!Status.IsEmpty())
	{
		FCanvasTextItem Line(FVector2D(Pixel.X + 16.0, Pixel.Y - 22.0), Status, Font,
			FLinearColor::White);
		Line.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Line);
	}
}

FText UPlaceRegionEditTool::GetStagePromptText() const
{
	const UPlaceRegionComponent* Region = TargetRegion.Get();
	if (!Region)
	{
		return CachedRegions.Num() == 0
			? LOCTEXT("EditPromptEmpty", "No regions in this level yet — draw one with the Pen tool first.")
			: LOCTEXT("EditPromptPick", "Click a region to edit its shape.");
	}

	switch (HoverKind)
	{
	case EHover::Corner:
		return LOCTEXT("EditPromptCorner",
			"Drag to move this corner. Alt+click removes it. Ctrl+click adds it to the selection.");
	case EHover::Edge:
		return LOCTEXT("EditPromptEdge", "Click to add a corner here.");
	case EHover::OtherRegion:
		return LOCTEXT("EditPromptSwitch", "Click to edit this region instead.");
	default:
		return LOCTEXT("EditPromptGeneral",
			"Drag a corner to move it, click an edge to add one, Alt+click a corner to remove it. "
			"Click empty ground when you are done.");
	}
}

FText UPlaceRegionEditTool::GetStatusText() const
{
	const UPlaceRegionComponent* Region = TargetRegion.Get();
	if (!Region)
	{
		if (const UPlaceRegionComponent* Under = HoverRegion.Get())
		{
			return FText::Format(LOCTEXT("EditStatusHover", "{0}"), Under->Name.GetDisplayText());
		}
		return FText::GetEmpty();
	}

	FString Line = FString::Printf(TEXT("%s · %d corners · %.0f m²"),
		*Region->Name.GetDisplayText().ToString(),
		Region->LocalPoints.Num(),
		Region->GetWorldArea() / 10000.0);

	if (SelectedCorners.Num() > 0)
	{
		Line += FString::Printf(TEXT("  ·  %d selected"), SelectedCorners.Num());
	}
	if (!SnapDetail.IsEmpty())
	{
		Line += FString::Printf(TEXT("  →  %s"), *SnapDetail);
	}
	return FText::FromString(Line);
}

UInteractiveTool* UPlaceRegionEditToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UPlaceRegionEditTool>(SceneState.ToolManager);
}

#undef LOCTEXT_NAMESPACE
