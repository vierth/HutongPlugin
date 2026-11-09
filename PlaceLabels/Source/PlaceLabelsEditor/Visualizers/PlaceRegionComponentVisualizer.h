#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

class UPlaceRegionComponent;

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

struct HPlaceRegionEdgeProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HPlaceRegionEdgeProxy(const UActorComponent* InComponent, int32 InEdgeIndex)
		: HComponentVisProxy(InComponent, HPP_Wireframe)
		, EdgeIndex(InEdgeIndex)
	{
	}

	int32 EdgeIndex;
};

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

	bool IsSelectionValid() const;

	void OnDeleteVertex();
	bool CanDeleteVertex() const;
	void OnDuplicateVertex();
	bool CanDuplicateVertex() const;
	void OnSelectAllVertices();
	bool CanSelectAllVertices() const;

	void InsertCornerOnEdge(UPlaceRegionComponent* Region, int32 EdgeIndex,
		const struct FViewportClick& Click);

	void DeleteSingleVertex(UPlaceRegionComponent* Region, int32 VertexIndex);

	FComponentPropertyPath RegionPropertyPath;

	TSet<int32> SelectedVertices;
	int32 LastSelectedVertex = INDEX_NONE;

	FProperty* PointsProperty = nullptr;

	TSharedPtr<FUICommandList> VisualizerActions;
};
