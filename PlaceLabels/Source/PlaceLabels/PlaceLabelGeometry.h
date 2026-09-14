#pragma once

#include "CoreMinimal.h"

// 2D polygon predicates shared by the runtime query, the hierarchy resolver and the editor tools.
namespace PlaceLabelsGeo
{
	// Crossing-number ray cast along +X.
	PLACELABELS_API bool PointInPolygon2D(const TArray<FVector2D>& Poly, const FVector2D& P);

	// Shoelace formula. The sign carries the winding; callers that want size take FMath::Abs.
	PLACELABELS_API double SignedArea2D(const TArray<FVector2D>& Poly);

	PLACELABELS_API FBox2D ComputeBounds2D(const TArray<FVector2D>& Poly);

	PLACELABELS_API double Perimeter2D(const TArray<FVector2D>& Poly);

	// Zero when P is inside or on the boundary, otherwise the distance to the nearest edge.
	PLACELABELS_API double DistancePointToPolygon2D(const TArray<FVector2D>& Poly, const FVector2D& P);

	// Smallest gap between two polygon boundaries; zero if they touch or overlap.
	PLACELABELS_API double DistancePolygonToPolygon2D(
		const TArray<FVector2D>& A, const TArray<FVector2D>& B, double MaxDistance);

	// A point guaranteed to lie inside the polygon: the centroid when that works, otherwise the nearest vertex to it.
	PLACELABELS_API FVector2D RepresentativePoint2D(const TArray<FVector2D>& Poly);

	// Index pairs of edges that cross.
	PLACELABELS_API void FindSelfIntersections(
		const TArray<FVector2D>& Poly, TArray<TPair<int32, int32>>& OutCrossingEdges);

	// Ear clipping into a triangle index list, three indices per triangle, wound counter-clockwise, for concave polygons in either input winding.
	PLACELABELS_API bool TriangulatePolygon2D(const TArray<FVector2D>& Poly, TArray<int32>& OutIndices);
}
