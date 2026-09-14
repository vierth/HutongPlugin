#pragma once

#include "CoreMinimal.h"

class FPrimitiveDrawInterface;
class UPlaceRegionComponent;
class UWorld;

// The parts of polygon authoring the pen tool and the edit tool both need.
namespace PlaceLabelsEdit
{
	// One palette for both tools.
	inline const FLinearColor SettledColor(1.0f, 0.9f, 0.15f);
	inline const FLinearColor PendingColor(0.45f, 0.8f, 1.0f);
	inline const FLinearColor CloseColor(0.3f, 1.0f, 0.4f);
	inline const FLinearColor DeleteColor(1.0f, 0.25f, 0.2f);
	inline const FLinearColor InsertColor(0.55f, 1.0f, 0.65f);
	inline const FLinearColor WarningColor(1.0f, 0.55f, 0.1f);
	inline const FLinearColor SelectedColor(1.0f, 0.45f, 0.1f);

	inline constexpr double VertexMarkerSize = 15.0;
	inline constexpr double InsertMarkerSize = 11.0;

	// Two clicks closer together than this are the same click.
	inline constexpr double DuplicatePointToleranceCm = 1.0;

	// Below this a polygon is a line with rounding error, not a place.
	inline constexpr double MinPolygonAreaCmSq = 100.0;

	// A press that travels less than this before release was a click, not a drag.
	inline constexpr double ClickSlopPixels = 4.0;

	// Two passes: a wide near-black backing line, then the coloured line on top.
	void DrawLine(FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B,
		const FLinearColor& Color, float Thickness);

	void DrawDashedLine(FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B,
		const FLinearColor& Color, float Thickness, double DashLength = 30.0);

	// A corner.
	void DrawCrossHandle(FPrimitiveDrawInterface* PDI, const FVector& P, const FLinearColor& Color,
		double Size, float Thickness);

	// An insertion point.
	void DrawDiamondHandle(FPrimitiveDrawInterface* PDI, const FVector& P, const FLinearColor& Color,
		double Size, float Thickness);

	// A removal. Drawn rotated 45 degrees off the corner cross for the same reason.
	void DrawCrossOutHandle(FPrimitiveDrawInterface* PDI, const FVector& P, const FLinearColor& Color,
		double Size, float Thickness);

	// Where a screen ray meets the ground.
	bool TraceGround(UWorld* World, const FVector& RayOrigin, const FVector& RayDirection,
		double FallbackPlaneZ, FVector& OutHit);

	// Nearest vertex to P. Returns INDEX_NONE on an empty polygon; OutDistSq is squared XY.
	int32 ClosestVertex(const TArray<FVector2D>& Poly, const FVector2D& P, double& OutDistSq);

	struct FEdgeHit
	{
		// Edge running from EdgeIndex to EdgeIndex + 1, wrapping when the polygon is closed.
		int32 EdgeIndex = INDEX_NONE;
		FVector2D Point = FVector2D::ZeroVector;
		double DistSq = TNumericLimits<double>::Max();
	};

	// Nearest point on the boundary.
	FEdgeHit ClosestEdge(const TArray<FVector2D>& Poly, const FVector2D& P, bool bClosed);

	struct FSnapSettings
	{
		bool bToVertices = true;
		bool bToEdges = true;
		double Radius = 100.0;
	};

	struct FSnapQuery
	{
		// Regions already in the level, cached by the caller rather than walked per hover tick.
		const TArray<TWeakObjectPtr<UPlaceRegionComponent>>* Regions = nullptr;

		// The region being edited, so it never snaps to itself.
		const UPlaceRegionComponent* Exclude = nullptr;

		// Points of the polygon under construction or under edit, in world space.
		const TArray<FVector>* OwnPoints = nullptr;

		// Index within OwnPoints that is currently being dragged, and so is not a snap target.
		int32 IgnoreOwnIndex = INDEX_NONE;
	};

	struct FSnapResult
	{
		FVector Point = FVector::ZeroVector;
		FString Detail;
		bool bSnapped = false;
	};

	// Own points first, then other regions' vertices, then their edges.
	FSnapResult ResolveSnap(const FVector& TracedHit, const FSnapSettings& Settings,
		const FSnapQuery& Query);

	// Every region component in the world with a usable polygon.
	// Every region in the world. Snapping and welding want an outline they can work against, so
	// they pass true; the browser, validation and the exchange must see the rest — a region cut
	// below three corners in the Details panel is exactly the broken state the warn-never-block
	// design exists to surface, and filtering it out here made it the one state nothing could see.
	void GatherRegions(UWorld* World, TArray<TWeakObjectPtr<UPlaceRegionComponent>>& OutRegions,
		bool bOnlyWithUsableOutline = false);

	// World-space points of a region, at the component's own Z so handles sit on its plane.
	void GetRegionWorldPoints3D(const UPlaceRegionComponent* Region, TArray<FVector>& OutPoints);
}
