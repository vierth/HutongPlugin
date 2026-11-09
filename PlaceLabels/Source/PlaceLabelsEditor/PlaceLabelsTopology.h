#pragma once

#include "CoreMinimal.h"

class UPlaceRegionComponent;
class UWorld;

namespace PlaceLabelsTopology
{
	inline constexpr double CoincidentEpsilonCm = 0.01;

	struct FSeamIssue
	{
		TWeakObjectPtr<UPlaceRegionComponent> A;
		TWeakObjectPtr<UPlaceRegionComponent> B;

		double WorstOffsetCm = 0.0;

		int32 NearMissCount = 0;

		bool bOverlapping = false;
	};

	void FindSeamIssues(const TArray<TWeakObjectPtr<UPlaceRegionComponent>>& Regions,
		double ToleranceCm, TArray<FSeamIssue>& OutIssues);

	struct FWeldReport
	{
		int32 CornersMoved = 0;
		int32 CornersInserted = 0;
		int32 CornersRemoved = 0;
		int32 RegionsChanged = 0;

		int32 ClustersRefused = 0;
    };

	void WeldBoundaries(const TArray<UPlaceRegionComponent*>& Regions, double ToleranceCm,
		FWeldReport& OutReport);

	void WeldPair(UPlaceRegionComponent* A, UPlaceRegionComponent* B, double ToleranceCm,
		FWeldReport& OutReport);

	void WeldPolygons(TArray<TArray<FVector2D>>& Polygons, double ToleranceCm,
		TArray<bool>& OutChanged, FWeldReport& OutReport);

	// A near-miss between two polygons, indices into the array passed in.
	struct FPolygonSeam
	{
		int32 A = 0;
		int32 B = 0;
		double WorstOffsetCm = 0.0;
		int32 NearMissCount = 0;
		bool bOverlapping = false;
	};

	void FindPolygonSeams(const TArray<TArray<FVector2D>>& Polygons, double ToleranceCm,
		TArray<FPolygonSeam>& OutSeams);

	struct FSeamMark
	{
		FVector2D From = FVector2D::ZeroVector;
		FVector2D To = FVector2D::ZeroVector;
		double DistanceCm = 0.0;
	};

	void FindSeamMarks(const TArray<FVector2D>& A, const TArray<FVector2D>& B, double ToleranceCm,
		TArray<FSeamMark>& OutMarks);
}
