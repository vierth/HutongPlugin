#pragma once

#include "CoreMinimal.h"
#include "PlaceLabelsTopology.h"

class UPlaceRegionComponent;
class UWorld;

// What is wrong with the labels in this level.
namespace PlaceLabelsValidation
{
	enum class EIssue : uint8
	{
		// Placed but not described.
		NoType,
		NoName,

		// Self-crossing outline: no inside, no fill.
		SelfIntersecting,

		// Fewer than three corners, or an area small enough to be rounding error.
		Degenerate,

		// Two regions of the same type carrying the same name.
		DuplicateName,

		// Its explicit parent no longer exists.
		DanglingParent,
	};

	FText DescribeIssue(EIssue Issue);

	// True if the region is unusable at runtime, not merely unfinished.
	bool IsSevere(EIssue Issue);

	struct FRegionIssue
	{
		TWeakObjectPtr<UPlaceRegionComponent> Region;
		EIssue Issue = EIssue::NoType;

		// Extra detail, e.g. the duplicated name or crossing count.
		FText Detail;
	};

	struct FReport
	{
		int32 RegionCount = 0;
		int32 NamedCount = 0;
		int32 TypedCount = 0;

		TArray<FRegionIssue> Issues;
		TArray<PlaceLabelsTopology::FSeamIssue> Seams;

		// Issue count per region, for the browser's warning badge.
		TMap<TWeakObjectPtr<UPlaceRegionComponent>, int32> IssueCountByRegion;

		int32 IssueCountFor(const UPlaceRegionComponent* Region) const;
		bool IsClean() const { return Issues.Num() == 0 && Seams.Num() == 0; }
	};

	// Sweep the whole level.
	void ValidateWorld(UWorld* World, double SeamToleranceCm, FReport& OutReport);

	void ValidateRegions(const TArray<TWeakObjectPtr<UPlaceRegionComponent>>& Regions,
		double SeamToleranceCm, FReport& OutReport);
}
