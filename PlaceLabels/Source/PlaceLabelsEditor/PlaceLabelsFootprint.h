#pragma once

#include "CoreMinimal.h"

class UWorld;

// Reading a HutongLayout building's footprint, so a region can be traced onto it exactly.
namespace PlaceLabelsFootprint
{
	// True when HutongLayout is loaded and its component class is reachable.
	bool IsAvailable();

	struct FFootprintHit
	{
		// The four corners in order, at the building's own Z.
		TArray<FVector> Corners;

		// The actor's label, for the readout.
		FString Label;

		// Distance from the query point to the footprint, zero when inside it.
		double Distance = 0.0;
	};

	// The building footprint at or nearest to a world point, within SearchRadius.
	bool FindFootprintNear(UWorld* World, const FVector& WorldPoint, double SearchRadius,
		FFootprintHit& OutHit);
}
