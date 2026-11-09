#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace PlaceLabelsFootprint
{
	bool IsAvailable();

	struct FFootprintHit
	{
		TArray<FVector> Corners;

		FString Label;

		// Distance from the query point to the footprint, zero when inside it.
		double Distance = 0.0;
	};

	bool FindFootprintNear(UWorld* World, const FVector& WorldPoint, double SearchRadius,
		FFootprintHit& OutHit);
}
