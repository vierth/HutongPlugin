#pragma once

#include "CoreMinimal.h"

class FPrimitiveDrawInterface;
class UPlaceRegionComponent;
class UWorld;

namespace PlaceLabelsEdit
{
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

	// Below this a polygon is a line with rounding error.
	inline constexpr double MinPolygonAreaCmSq = 100.0;

	// A press that travels less than this before release is a click.
	inline constexpr double ClickSlopPixels = 4.0;

	void DrawLine(FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B,
		const FLinearColor& Color, float Thickness);

	void DrawDashedLine(FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B,
		const FLinearColor& Color, float Thickness, double DashLength = 30.0);

	void DrawCrossHandle(FPrimitiveDrawInterface* PDI, const FVector& P, const FLinearColor& Color,
		double Size, float Thickness);

	void DrawDiamondHandle(FPrimitiveDrawInterface* PDI, const FVector& P, const FLinearColor& Color,
		double Size, float Thickness);

	void DrawCrossOutHandle(FPrimitiveDrawInterface* PDI, const FVector& P, const FLinearColor& Color,
		double Size, float Thickness);

	bool TraceGround(UWorld* World, const FVector& RayOrigin, const FVector& RayDirection,
		double FallbackPlaneZ, FVector& OutHit);

	// OutDistSq is squared XY.
	int32 ClosestVertex(const TArray<FVector2D>& Poly, const FVector2D& P, double& OutDistSq);

	struct FEdgeHit
	{
		// Edge running from EdgeIndex to EdgeIndex + 1, wrapping when the polygon is closed.
		int32 EdgeIndex = INDEX_NONE;
		FVector2D Point = FVector2D::ZeroVector;
		double DistSq = TNumericLimits<double>::Max();
	};

	FEdgeHit ClosestEdge(const TArray<FVector2D>& Poly, const FVector2D& P, bool bClosed);

	struct FSnapSettings
	{
		bool bToVertices = true;
		bool bToEdges = true;
		double Radius = 100.0;
	};

	struct FSnapQuery
	{
		const TArray<TWeakObjectPtr<UPlaceRegionComponent>>* Regions = nullptr;

		const UPlaceRegionComponent* Exclude = nullptr;

		const TArray<FVector>* OwnPoints = nullptr;

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

	// Snapping and welding pass true; browser, validation and exchange pass false to surface broken regions.
	void GatherRegions(UWorld* World, TArray<TWeakObjectPtr<UPlaceRegionComponent>>& OutRegions,
		bool bOnlyWithUsableOutline = false);

	// At the component's own Z so handles sit on its plane.
	void GetRegionWorldPoints3D(const UPlaceRegionComponent* Region, TArray<FVector>& OutPoints);
}
