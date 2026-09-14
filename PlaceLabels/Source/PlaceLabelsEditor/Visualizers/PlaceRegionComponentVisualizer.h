#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

class UPlaceRegionComponent;

// A corner of the polygon. Dragging one moves that vertex.
struct HPlaceRegionVertexProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HPlaceRegionVertexProxy(const UActorComponent* InComponent, int32 InVertexIndex)
		: HComponentVisProxy(InComponent, HPP_Wireframe)
		, VertexIndex(InVertexIndex)
	{
	}

	int32 VertexIndex;
};

// An edge.
struct HPlaceRegionEdgeProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HPlaceRegionEdgeProxy(const UActorComponent* InComponent, int32 InEdgeIndex)
		: HComponentVisProxy(InComponent, HPP_Wireframe)
		, EdgeIndex(InEdgeIndex)
	{
	}

	// Edge running from EdgeIndex to EdgeIndex + 1, wrapping.
	int32 EdgeIndex;
};

// Vertex editing for a placed region, in the spirit of the spline component's handles.
class FPlaceRegionComponentVisualizer : public FComponentVisualizer
{
public:
	FPlaceRegionComponentVisualizer();

	virtual void OnRegister() override;
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View,
		FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient,
		HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient,
		FVector& OutLocation) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport,
		FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	virtual bool HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport,
		FKey Key, EInputEvent Event) override;
	virtual void TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove) override;
	virtual void EndEditing() override;
	virtual UActorComponent* GetEditedComponent() const override;
	virtual TSharedPtr<SWidget> GenerateContextMenu() const override;

private:
	UPlaceRegionComponent* GetEditedRegion() const;

	// True when the selection still refers to corners that exist.
	bool IsSelectionValid() const;

	void OnDeleteVertex();
	bool CanDeleteVertex() const;
	void OnDuplicateVertex();
	bool CanDuplicateVertex() const;
	void OnSelectAllVertices();
	bool CanSelectAllVertices() const;

	// Adds a corner at the point on EdgeIndex nearest the click ray, falling back to the edge's midpoint when the ray is parallel to the region's plane.
	void InsertCornerOnEdge(UPlaceRegionComponent* Region, int32 EdgeIndex,
		const struct FViewportClick& Click);

	// Removes one corner without disturbing the rest of the selection, which is what Alt+click on a handle means.
	void DeleteSingleVertex(UPlaceRegionComponent* Region, int32 VertexIndex);

	// A property path rather than a raw or weak pointer.
	FComponentPropertyPath RegionPropertyPath;

	TSet<int32> SelectedVertices;
	int32 LastSelectedVertex = INDEX_NONE;

	FProperty* PointsProperty = nullptr;

	TSharedPtr<FUICommandList> VisualizerActions;
};
