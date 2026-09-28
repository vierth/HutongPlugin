#pragma once

#include "CoreMinimal.h"

class UPlaceRegionComponent;
class UWorld;

// Works out which region a region belongs to.
namespace PlaceLabelsHierarchy
{
	PLACELABELS_API void CollectRegions(UWorld* World, TArray<UPlaceRegionComponent*>& OutRegions);

	// The region that should be Region's parent, or null if it is a root.
	PLACELABELS_API UPlaceRegionComponent* ResolveParentFor(
		UPlaceRegionComponent* Region, const TArray<UPlaceRegionComponent*>& AllRegions);

	// Whether making Proposed the parent of Child would close a loop.
	PLACELABELS_API bool WouldCreateCycle(
		const UPlaceRegionComponent* Child, const UPlaceRegionComponent* Proposed);

	// Re-resolves every region, coarsest type first so no region consults an unresolved parent.
	PLACELABELS_API int32 RecomputeAll(UWorld* World);
}
