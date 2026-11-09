#pragma once

#include "CoreMinimal.h"
#include "Tools/HutongWallChain.h"

class UHutongWallBuildingComponent;

namespace HutongWallRun
{
	struct FRun
	{
		TArray<TWeakObjectPtr<UHutongWallBuildingComponent>> Legs;
		// Legs + 1: leg i runs Vertices[i] -> Vertices[i + 1], on the centre line.
		TArray<FVector2D> Vertices;
		double Z = 0.0;
		double Thickness = 0.0;
		double Setback = 0.0;
		HutongWallChain::FEndFace StartFace;
		HutongWallChain::FEndFace EndFace;

		int32 NumLegs() const { return Legs.Num(); }
		bool Contains(const UHutongWallBuildingComponent* Leg) const;
	};

	void LegEnds(const UHutongWallBuildingComponent* Leg, FVector2D& OutStart, FVector2D& OutEnd);

	HutongWallChain::FEndFace OuterFace(const UHutongWallBuildingComponent* Leg, bool bStart);

	bool Gather(UHutongWallBuildingComponent* Seed, const TArray<UHutongWallBuildingComponent*>& Candidates, FRun& Out);

	bool Rebuild(const FRun& Run, const TArray<FVector2D>& Vertices,
		const HutongWallChain::FEndFace& StartFace, const HutongWallChain::FEndFace& EndFace,
		TArray<HutongWallChain::FSegment>& OutSegments);

	void Apply(const FRun& Run, const TArray<HutongWallChain::FSegment>& Segments);
}
