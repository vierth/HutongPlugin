#include "Visualizers/PlaceRegionComponentVisualizer.h"

#include "PlaceLabelGeometry.h"
#include "PlaceLabelsVisualizerCommands.h"
#include "PlaceRegionComponent.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "SceneManagement.h"

IMPLEMENT_HIT_PROXY(HPlaceRegionVertexProxy, HComponentVisProxy);
IMPLEMENT_HIT_PROXY(HPlaceRegionEdgeProxy, HComponentVisProxy);

#define LOCTEXT_NAMESPACE "PlaceRegionComponentVisualizer"

namespace
{
	const FLinearColor VertexColor(1.0f, 0.9f, 0.15f);
	const FLinearColor SelectedVertexColor(1.0f, 0.45f, 0.1f);
	const FLinearColor MidpointColor(0.55f, 1.0f, 0.65f, 0.85f);
	const FLinearColor DeleteColor(1.0f, 0.25f, 0.2f);
	const FLinearColor ErrorColor(1.0f, 0.1f, 0.1f);

	// In pixels, converted per handle.
	constexpr double HandlePixels = 9.0;
	constexpr double MidpointPixels = 5.0;

	// World size covering the given number of pixels at P.
	double WorldSizeForPixels(const FSceneView* View, const FVector& P, double Pixels)
	{
		if (!View)
		{
			return Pixels;
		}
		const FMatrix& Projection = View->ViewMatrices.GetViewToClip();
		const double ZoomFactor = FMath::Max<double>(
			FMath::Min(Projection.M[0][0], Projection.M[1][1]), UE_KINDA_SMALL_NUMBER);
		const double ViewWidth = FMath::Max(View->UnscaledViewRect.Width(), 1);
		const double W = FMath::Abs(View->WorldToScreen(P).W);
		return FMath::Max(W * (Pixels / ViewWidth / ZoomFactor), 1.0);
	}

	// Handles are drawn as crossed segments.
	void DrawHandle(FPrimitiveDrawInterface* PDI, const FVector& P, const FLinearColor& Color,
		double Size, float Thickness)
	{
		const double H = Size * 0.5;
		PDI->DrawLine(P - FVector(H, 0, 0), P + FVector(H, 0, 0), Color, SDPG_Foreground, Thickness);
		PDI->DrawLine(P - FVector(0, H, 0), P + FVector(0, H, 0), Color, SDPG_Foreground, Thickness);
		PDI->DrawLine(P - FVector(0, 0, H), P + FVector(0, 0, H), Color, SDPG_Foreground, Thickness);
	}

	// Rotated 45 degrees off DrawHandle, so "this click removes the corner" never reads as "this is a corner" at a glance.
	void DrawDeleteHandle(FPrimitiveDrawInterface* PDI, const FVector& P, const FLinearColor& Color,
		double Size, float Thickness)
	{
		const double H = Size * 0.5 * UE_INV_SQRT_2;
		PDI->DrawLine(P + FVector(-H, -H, 0), P + FVector(H, H, 0), Color, SDPG_Foreground, Thickness);
		PDI->DrawLine(P + FVector(-H, H, 0), P + FVector(H, -H, 0), Color, SDPG_Foreground, Thickness);
		PDI->DrawLine(P + FVector(0, -H, -H), P + FVector(0, H, H), Color, SDPG_Foreground, Thickness);
	}

	FVector LocalPointToWorld(const UPlaceRegionComponent* Region, int32 Index)
	{
		return Region->GetComponentTransform().TransformPosition(
			FVector(Region->LocalPoints[Index].X, Region->LocalPoints[Index].Y, 0.0));
	}

	bool IsAltHeld()
	{
		return FSlateApplication::IsInitialized()
			&& FSlateApplication::Get().GetModifierKeys().IsAltDown();
	}
}

FPlaceRegionComponentVisualizer::FPlaceRegionComponentVisualizer()
{
	PointsProperty = FindFProperty<FProperty>(UPlaceRegionComponent::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UPlaceRegionComponent, LocalPoints));
}

void FPlaceRegionComponentVisualizer::OnRegister()
{
	VisualizerActions = MakeShareable(new FUICommandList);

	const FPlaceLabelsVisualizerCommands& Commands = FPlaceLabelsVisualizerCommands::Get();

	VisualizerActions->MapAction(Commands.DeleteVertex,
		FExecuteAction::CreateSP(this, &FPlaceRegionComponentVisualizer::OnDeleteVertex),
		FCanExecuteAction::CreateSP(this, &FPlaceRegionComponentVisualizer::CanDeleteVertex));

	VisualizerActions->MapAction(Commands.DuplicateVertex,
		FExecuteAction::CreateSP(this, &FPlaceRegionComponentVisualizer::OnDuplicateVertex),
		FCanExecuteAction::CreateSP(this, &FPlaceRegionComponentVisualizer::CanDuplicateVertex));

	VisualizerActions->MapAction(Commands.SelectAllVertices,
		FExecuteAction::CreateSP(this, &FPlaceRegionComponentVisualizer::OnSelectAllVertices),
		FCanExecuteAction::CreateSP(this, &FPlaceRegionComponentVisualizer::CanSelectAllVertices));
}

UPlaceRegionComponent* FPlaceRegionComponentVisualizer::GetEditedRegion() const
{
	return Cast<UPlaceRegionComponent>(RegionPropertyPath.GetComponent());
}

UActorComponent* FPlaceRegionComponentVisualizer::GetEditedComponent() const
{
	return RegionPropertyPath.GetComponent();
}

bool FPlaceRegionComponentVisualizer::IsSelectionValid() const
{
	const UPlaceRegionComponent* Region = GetEditedRegion();
	if (!Region)
	{
		return false;
	}
	for (int32 Index : SelectedVertices)
	{
		if (!Region->LocalPoints.IsValidIndex(Index))
		{
			return false;
		}
	}
	return true;
}

void FPlaceRegionComponentVisualizer::DrawVisualization(const UActorComponent* Component,
	const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UPlaceRegionComponent* Region = Cast<const UPlaceRegionComponent>(Component);
	if (!Region || Region->LocalPoints.Num() < 2)
	{
		return;
	}

	const int32 N = Region->LocalPoints.Num();
	const bool bIsEditedComponent = (GetEditedComponent() == Component);

	// Crossing edges are reported, never silently repaired.
	TArray<TPair<int32, int32>> Crossings;
	PlaceLabelsGeo::FindSelfIntersections(Region->GetWorldPoints2D(), Crossings);
	TSet<int32> BadEdges;
	for (const TPair<int32, int32>& Pair : Crossings)
	{
		BadEdges.Add(Pair.Key);
		BadEdges.Add(Pair.Value);
	}

	const FLinearColor OutlineColor = Region->GetOutlineColor();
	const bool bAlt = IsAltHeld();

	// The edges carry the insert proxy, not just their midpoints.
	for (int32 i = 0; i < N; ++i)
	{
		const int32 Next = (i + 1) % N;
		const FVector A = LocalPointToWorld(Region, i);
		const FVector B = LocalPointToWorld(Region, Next);
		const bool bBad = BadEdges.Contains(i);

		PDI->SetHitProxy(N >= 3 ? new HPlaceRegionEdgeProxy(Component, i) : nullptr);
		PDI->DrawLine(A, B, bBad ? ErrorColor : OutlineColor, SDPG_Foreground, bBad ? 4.0f : 2.5f);
		PDI->SetHitProxy(nullptr);
	}

	// Midpoint markers stay as the visible hint that edges are insertable — the outline alone does not advertise it.
	if (N >= 3)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const FVector Mid =
				(LocalPointToWorld(Region, i) + LocalPointToWorld(Region, (i + 1) % N)) * 0.5;

			PDI->SetHitProxy(new HPlaceRegionEdgeProxy(Component, i));
			DrawHandle(PDI, Mid, MidpointColor, WorldSizeForPixels(View, Mid, MidpointPixels) * 2.0,
				2.0f);
			PDI->SetHitProxy(nullptr);
		}
	}

	const bool bCanDelete = N > 3;

	for (int32 i = 0; i < N; ++i)
	{
		const bool bSelected = bIsEditedComponent && SelectedVertices.Contains(i);
		const FVector P = LocalPointToWorld(Region, i);
		const double Size = WorldSizeForPixels(View, P, HandlePixels) * 2.0;

		// Every SetHitProxy must be paired with a null, or everything drawn afterwards inherits this proxy and the whole outline becomes clickable as vertex i.
		PDI->SetHitProxy(new HPlaceRegionVertexProxy(Component, i));

		// Holding Alt turns every corner into a removal, and says so before the click rather than after it.
		if (bAlt && bCanDelete)
		{
			DrawDeleteHandle(PDI, P, DeleteColor, Size * 1.4, 4.0f);
		}
		else
		{
			DrawHandle(PDI, P, bSelected ? SelectedVertexColor : VertexColor,
				bSelected ? Size * 1.4 : Size, bSelected ? 5.0f : 3.0f);
		}
		PDI->SetHitProxy(nullptr);
	}
}

bool FPlaceRegionComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient,
	HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	if (!VisProxy || !VisProxy->Component.IsValid())
	{
		return false;
	}

	if (VisProxy->IsA(HPlaceRegionVertexProxy::StaticGetType()))
	{
		const HPlaceRegionVertexProxy* Proxy = static_cast<HPlaceRegionVertexProxy*>(VisProxy);

		RegionPropertyPath = FComponentPropertyPath(VisProxy->Component.Get());

		// Alt+click removes this one corner, leaving any other selection alone.
		if (Click.IsAltDown())
		{
			if (UPlaceRegionComponent* Region = GetEditedRegion())
			{
				DeleteSingleVertex(Region, Proxy->VertexIndex);
				return true;
			}
		}

		if (!InViewportClient || !InViewportClient->IsCtrlPressed())
		{
			SelectedVertices.Reset();
		}
		SelectedVertices.Add(Proxy->VertexIndex);
		LastSelectedVertex = Proxy->VertexIndex;
		return true;
	}

	if (VisProxy->IsA(HPlaceRegionEdgeProxy::StaticGetType()))
	{
		const HPlaceRegionEdgeProxy* Proxy = static_cast<HPlaceRegionEdgeProxy*>(VisProxy);

		RegionPropertyPath = FComponentPropertyPath(VisProxy->Component.Get());
		UPlaceRegionComponent* Region = GetEditedRegion();
		if (!Region || !Region->LocalPoints.IsValidIndex(Proxy->EdgeIndex))
		{
			return false;
		}

		InsertCornerOnEdge(Region, Proxy->EdgeIndex, Click);
		return true;
	}

	return false;
}

void FPlaceRegionComponentVisualizer::InsertCornerOnEdge(UPlaceRegionComponent* Region,
	int32 EdgeIndex, const FViewportClick& Click)
{
	const int32 N = Region->LocalPoints.Num();
	if (N < 3 || !Region->LocalPoints.IsValidIndex(EdgeIndex))
	{
		return;
	}

	const FVector2D A = Region->LocalPoints[EdgeIndex];
	const FVector2D B = Region->LocalPoints[(EdgeIndex + 1) % N];

	// Where the click ray meets the region's own plane, in component space.
	FVector2D Insert = (A + B) * 0.5;
	{
		const FTransform& Xf = Region->GetComponentTransform();
		const FVector LocalOrigin = Xf.InverseTransformPosition(Click.GetOrigin());
		const FVector LocalDirection = Xf.InverseTransformVector(Click.GetDirection());

		if (FMath::Abs(LocalDirection.Z) > UE_KINDA_SMALL_NUMBER)
		{
			const double T = -LocalOrigin.Z / LocalDirection.Z;
			if (T > 0.0)
			{
				const FVector OnPlane = LocalOrigin + LocalDirection * T;
				// Clamped to the segment: a grazing ray can land the intersection well past the end of the edge that was clicked.
				Insert = FMath::ClosestPointOnSegment2D(FVector2D(OnPlane.X, OnPlane.Y), A, B);
			}
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("InsertVertex", "Insert Region Vertex"));
	if (AActor* Owner = Region->GetOwner())
	{
		Owner->Modify();
	}
	Region->Modify();

	const int32 NewIndex = EdgeIndex + 1;
	Region->LocalPoints.Insert(Insert, NewIndex);
	Region->RebuildCache();
	Region->UpdateBounds();

	NotifyPropertyModified(Region, PointsProperty, EPropertyChangeType::ValueSet);

	SelectedVertices.Reset();
	SelectedVertices.Add(NewIndex);
	LastSelectedVertex = NewIndex;

	GEditor->RedrawLevelEditingViewports(true);
}

void FPlaceRegionComponentVisualizer::DeleteSingleVertex(UPlaceRegionComponent* Region,
	int32 VertexIndex)
{
	if (!Region || !Region->LocalPoints.IsValidIndex(VertexIndex))
	{
		return;
	}
	// Refusing to leave a degenerate polygon beats repairing one later.
	if (Region->LocalPoints.Num() <= 3)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteVertex", "Delete Region Vertex"));
	if (AActor* Owner = Region->GetOwner())
	{
		Owner->Modify();
	}
	Region->Modify();

	Region->LocalPoints.RemoveAt(VertexIndex);
	Region->RebuildCache();
	Region->UpdateBounds();
	NotifyPropertyModified(Region, PointsProperty, EPropertyChangeType::ValueSet);

	SelectedVertices.Reset();
	LastSelectedVertex = INDEX_NONE;
	GEditor->RedrawLevelEditingViewports(true);
}

bool FPlaceRegionComponentVisualizer::GetWidgetLocation(
	const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	const UPlaceRegionComponent* Region = GetEditedRegion();
	if (!Region || SelectedVertices.Num() == 0 || !IsSelectionValid())
	{
		return false;
	}

	// Centroid of the selection, so a multi-vertex drag has a sensible pivot.
	FVector Sum = FVector::ZeroVector;
	for (int32 Index : SelectedVertices)
	{
		Sum += LocalPointToWorld(Region, Index);
	}
	OutLocation = Sum / static_cast<double>(SelectedVertices.Num());
	return true;
}

bool FPlaceRegionComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient,
	FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	UPlaceRegionComponent* Region = GetEditedRegion();
	if (!Region || SelectedVertices.Num() == 0)
	{
		return false;
	}
	if (!IsSelectionValid())
	{
		// Something changed the point count behind our back.
		EndEditing();
		return false;
	}

	// Rotate and scale fall through to the actor; only translation edits vertices.
	if (DeltaTranslate.IsNearlyZero())
	{
		return false;
	}

	Region->Modify();

	// DeltaTranslate arrives in world space and LocalPoints are component-local.
	const FTransform& Xf = Region->GetComponentTransform();
	for (int32 Index : SelectedVertices)
	{
		const FVector World =
			Xf.TransformPosition(FVector(Region->LocalPoints[Index].X, Region->LocalPoints[Index].Y, 0.0));
		const FVector Local = Xf.InverseTransformPosition(World + DeltaTranslate);
		Region->LocalPoints[Index] = FVector2D(Local.X, Local.Y);
	}

	Region->RebuildCache();
	Region->UpdateBounds();

	// Interactive, not ValueSet: re-parenting during a drag would run once per mouse move.
	NotifyPropertyModified(Region, PointsProperty, EPropertyChangeType::Interactive);
	return true;
}

bool FPlaceRegionComponentVisualizer::HandleInputKey(FEditorViewportClient* ViewportClient,
	FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (Event != IE_Pressed || !VisualizerActions.IsValid())
	{
		return false;
	}
	if (!IsSelectionValid())
	{
		EndEditing();
		return false;
	}

	return VisualizerActions->ProcessCommandBindings(
		Key, FSlateApplication::Get().GetModifierKeys(), /*bRepeat*/ false);
}

void FPlaceRegionComponentVisualizer::TrackingStopped(
	FEditorViewportClient* InViewportClient, bool bInDidMove)
{
	if (!bInDidMove)
	{
		return;
	}
	UPlaceRegionComponent* Region = GetEditedRegion();
	if (!Region)
	{
		return;
	}

	Region->RebuildCache();
	Region->UpdateBounds();

	// The commit the Interactive notifications during the drag deliberately skipped.
	NotifyPropertyModified(Region, PointsProperty, EPropertyChangeType::ValueSet);

	// Its own transaction: the gizmo's has already closed by the time tracking stops.
	if (Region->bAutoParent && !Region->ExplicitParent)
	{
		const FScopedTransaction Transaction(
			LOCTEXT("ReparentRegion", "Recompute Region Parent"));
		Region->RecomputeDerivedParent();
	}
}

void FPlaceRegionComponentVisualizer::EndEditing()
{
	RegionPropertyPath = FComponentPropertyPath();
	SelectedVertices.Reset();
	LastSelectedVertex = INDEX_NONE;
}

bool FPlaceRegionComponentVisualizer::CanDeleteVertex() const
{
	const UPlaceRegionComponent* Region = GetEditedRegion();
	if (!Region || SelectedVertices.Num() == 0 || !IsSelectionValid())
	{
		return false;
	}
	// Refusing to leave a degenerate polygon beats repairing one later.
	return Region->LocalPoints.Num() - SelectedVertices.Num() >= 3;
}

void FPlaceRegionComponentVisualizer::OnDeleteVertex()
{
	UPlaceRegionComponent* Region = GetEditedRegion();
	if (!Region || !CanDeleteVertex())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DeleteVertex", "Delete Region Vertex"));
	if (AActor* Owner = Region->GetOwner())
	{
		Owner->Modify();
	}
	Region->Modify();

	// Descending, so earlier removals do not shift the indices still to be removed.
	TArray<int32> Indices = SelectedVertices.Array();
	Indices.Sort([](int32 A, int32 B) { return A > B; });
	for (int32 Index : Indices)
	{
		Region->LocalPoints.RemoveAt(Index);
	}

	Region->RebuildCache();
	Region->UpdateBounds();
	NotifyPropertyModified(Region, PointsProperty, EPropertyChangeType::ValueSet);

	SelectedVertices.Reset();
	LastSelectedVertex = INDEX_NONE;
	GEditor->RedrawLevelEditingViewports(true);
}

bool FPlaceRegionComponentVisualizer::CanDuplicateVertex() const
{
	const UPlaceRegionComponent* Region = GetEditedRegion();
	return Region && SelectedVertices.Num() == 1 && IsSelectionValid();
}

void FPlaceRegionComponentVisualizer::OnDuplicateVertex()
{
	UPlaceRegionComponent* Region = GetEditedRegion();
	if (!Region || !CanDuplicateVertex() || !Region->LocalPoints.IsValidIndex(LastSelectedVertex))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("DuplicateVertex", "Duplicate Region Vertex"));
	if (AActor* Owner = Region->GetOwner())
	{
		Owner->Modify();
	}
	Region->Modify();

	// Offset toward the next corner.
	const int32 N = Region->LocalPoints.Num();
	const FVector2D Here = Region->LocalPoints[LastSelectedVertex];
	const FVector2D Next = Region->LocalPoints[(LastSelectedVertex + 1) % N];
	const FVector2D Offset = (Next - Here) * 0.25;

	const int32 NewIndex = LastSelectedVertex + 1;
	Region->LocalPoints.Insert(Here + Offset, NewIndex);

	Region->RebuildCache();
	Region->UpdateBounds();
	NotifyPropertyModified(Region, PointsProperty, EPropertyChangeType::ValueSet);

	SelectedVertices.Reset();
	SelectedVertices.Add(NewIndex);
	LastSelectedVertex = NewIndex;
	GEditor->RedrawLevelEditingViewports(true);
}

bool FPlaceRegionComponentVisualizer::CanSelectAllVertices() const
{
	const UPlaceRegionComponent* Region = GetEditedRegion();
	return Region && Region->LocalPoints.Num() > 0;
}

void FPlaceRegionComponentVisualizer::OnSelectAllVertices()
{
	const UPlaceRegionComponent* Region = GetEditedRegion();
	if (!Region)
	{
		return;
	}

	SelectedVertices.Reset();
	for (int32 i = 0; i < Region->LocalPoints.Num(); ++i)
	{
		SelectedVertices.Add(i);
	}
	LastSelectedVertex = Region->LocalPoints.Num() - 1;
	GEditor->RedrawLevelEditingViewports(true);
}

TSharedPtr<SWidget> FPlaceRegionComponentVisualizer::GenerateContextMenu() const
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection*/ true, VisualizerActions);

	const FPlaceLabelsVisualizerCommands& Commands = FPlaceLabelsVisualizerCommands::Get();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("RegionVertexHeader", "Region Vertex"));
	MenuBuilder.AddMenuEntry(Commands.DeleteVertex);
	MenuBuilder.AddMenuEntry(Commands.DuplicateVertex);
	MenuBuilder.AddMenuEntry(Commands.SelectAllVertices);
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
