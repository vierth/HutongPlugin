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

		// The outline crosses itself, so "inside" has no answer and the fill will not build.
		SelfIntersecting,

		// Fewer than three corners, or an area small enough to be rounding error.
		Degenerate,

		// Two regions of the same type carrying the same name.
		DuplicateName,

		// Its explicit parent no longer exists.
		DanglingParent,
	};

	FText DescribeIssue(EIssue Issue);

	// True for the ones that make a region unusable at runtime, as opposed to merely unfinished.
	bool IsSevere(EIssue Issue);

	struct FRegionIssue
	{
		TWeakObjectPtr<UPlaceRegionComponent> Region;
		EIssue Issue = EIssue::NoType;

		// Filled in where the issue alone does not say enough — which name is duplicated, how many edges cross.
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
