#pragma once

#include "CoreMinimal.h"

class UPlaceRegionComponent;
class UWorld;

namespace PlaceLabelsHierarchy
{
	PLACELABELS_API void CollectRegions(UWorld* World, TArray<UPlaceRegionComponent*>& OutRegions);

	PLACELABELS_API UPlaceRegionComponent* ResolveParentFor(
		UPlaceRegionComponent* Region, const TArray<UPlaceRegionComponent*>& AllRegions);

	PLACELABELS_API bool WouldCreateCycle(
		const UPlaceRegionComponent* Child, const UPlaceRegionComponent* Proposed);

	// Re-resolves every region, coarsest type first so each parent is resolved before its children.
	PLACELABELS_API int32 RecomputeAll(UWorld* World);
}
