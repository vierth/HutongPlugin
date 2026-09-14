#pragma once

#include "CoreMinimal.h"

class UPlaceRegionComponent;
class UWorld;

// Shared boundaries between neighbouring regions.
namespace PlaceLabelsTopology
{
	// Two positions closer than this are the same position.
	inline constexpr double CoincidentEpsilonCm = 0.01;

	// A pair of regions whose boundaries nearly, but do not exactly, coincide.
	struct FSeamIssue
	{
		TWeakObjectPtr<UPlaceRegionComponent> A;
		TWeakObjectPtr<UPlaceRegionComponent> B;

		// Largest offset found along the shared run.
		double WorstOffsetCm = 0.0;

		// How many places along the two boundaries come close without meeting.
		int32 NearMissCount = 0;

		// The boundaries cross rather than fall short: this is an overlap, not a gap.
		bool bOverlapping = false;
	};

	// Every pair of regions that nearly share a boundary without sharing it exactly.
	void FindSeamIssues(const TArray<TWeakObjectPtr<UPlaceRegionComponent>>& Regions,
		double ToleranceCm, TArray<FSeamIssue>& OutIssues);

	struct FWeldReport
	{
		int32 CornersMoved = 0;
		int32 CornersInserted = 0;
		int32 CornersRemoved = 0;
		int32 RegionsChanged = 0;

		// Clusters that spread further than the tolerance should allow.
		int32 ClustersRefused = 0;
    };

	// Make the given regions share exact vertex chains wherever they nearly do.
	void WeldBoundaries(const TArray<UPlaceRegionComponent*>& Regions, double ToleranceCm,
		FWeldReport& OutReport);

	// Convenience: weld one pair, which is what the per-issue button in the problems list does.
	void WeldPair(UPlaceRegionComponent* A, UPlaceRegionComponent* B, double ToleranceCm,
		FWeldReport& OutReport);

	// The weld itself, on plain polygons in a shared coordinate space.
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

	// The detection half, likewise on plain polygons.
	void FindPolygonSeams(const TArray<TArray<FVector2D>>& Polygons, double ToleranceCm,
		TArray<FPolygonSeam>& OutSeams);

	// One place where two boundaries come close without meeting: the gap, drawn end to end.
	struct FSeamMark
	{
		FVector2D From = FVector2D::ZeroVector;
		FVector2D To = FVector2D::ZeroVector;
		double DistanceCm = 0.0;
	};

	// Every near-miss between two boundaries, measured edge against edge rather than corner against boundary.
	void FindSeamMarks(const TArray<FVector2D>& A, const TArray<FVector2D>& B, double ToleranceCm,
		TArray<FSeamMark>& OutMarks);
}
