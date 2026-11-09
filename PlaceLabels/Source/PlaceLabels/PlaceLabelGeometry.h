#pragma once

#include "CoreMinimal.h"

namespace PlaceLabelsGeo
{
	PLACELABELS_API bool PointInPolygon2D(const TArray<FVector2D>& Poly, const FVector2D& P);

	// The sign carries the winding.
	PLACELABELS_API double SignedArea2D(const TArray<FVector2D>& Poly);

	PLACELABELS_API FBox2D ComputeBounds2D(const TArray<FVector2D>& Poly);

	PLACELABELS_API double Perimeter2D(const TArray<FVector2D>& Poly);

	PLACELABELS_API double DistancePointToPolygon2D(const TArray<FVector2D>& Poly, const FVector2D& P);

	PLACELABELS_API double DistancePolygonToPolygon2D(
		const TArray<FVector2D>& A, const TArray<FVector2D>& B, double MaxDistance);

	PLACELABELS_API FVector2D RepresentativePoint2D(const TArray<FVector2D>& Poly);

	PLACELABELS_API void FindSelfIntersections(
		const TArray<FVector2D>& Poly, TArray<TPair<int32, int32>>& OutCrossingEdges);

	// CCW index triples; concave input, either winding.
	PLACELABELS_API bool TriangulatePolygon2D(const TArray<FVector2D>& Poly, TArray<int32>& OutIndices);
}
