#pragma once

#include "CoreMinimal.h"
#include "PlaceLabelsTopology.h"

class UPlaceRegionComponent;
class UWorld;

namespace PlaceLabelsValidation
{
	enum class EIssue : uint8
	{
		NoType,
		NoName,

		SelfIntersecting,

		Degenerate,

		DuplicateName,

		DanglingParent,
	};

	FText DescribeIssue(EIssue Issue);

	bool IsSevere(EIssue Issue);

	struct FRegionIssue
	{
		TWeakObjectPtr<UPlaceRegionComponent> Region;
		EIssue Issue = EIssue::NoType;

		FText Detail;
	};

	struct FReport
	{
		int32 RegionCount = 0;
		int32 NamedCount = 0;
		int32 TypedCount = 0;

		TArray<FRegionIssue> Issues;
		TArray<PlaceLabelsTopology::FSeamIssue> Seams;

		TMap<TWeakObjectPtr<UPlaceRegionComponent>, int32> IssueCountByRegion;

		int32 IssueCountFor(const UPlaceRegionComponent* Region) const;
		bool IsClean() const { return Issues.Num() == 0 && Seams.Num() == 0; }
	};

	void ValidateWorld(UWorld* World, double SeamToleranceCm, FReport& OutReport);

	void ValidateRegions(const TArray<TWeakObjectPtr<UPlaceRegionComponent>>& Regions,
		double SeamToleranceCm, FReport& OutReport);
}
