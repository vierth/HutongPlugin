#pragma once

#include "CoreMinimal.h"
#include "Tools/HutongWallChain.h"

class UHutongWallBuildingComponent;

// A run of placed wall segments read back off the level: legs whose end meets the next one's
// start, so a vertex of the run can be moved and every leg meeting there rebuilt to meld again.
namespace HutongWallRun
{
	struct FRun
	{
		TArray<TWeakObjectPtr<UHutongWallBuildingComponent>> Legs;
		// One more than the legs: each leg runs from Vertices[i] to Vertices[i + 1], on the centre line.
		TArray<FVector2D> Vertices;
		double Z = 0.0;
		double Thickness = 0.0;
		// The faces the two outer ends are cut on, as they stand; unset where an end is square.
		HutongWallChain::FEndFace StartFace;
		HutongWallChain::FEndFace EndFace;

		int32 NumLegs() const { return Legs.Num(); }
		bool Contains(const UHutongWallBuildingComponent* Leg) const;
	};

	// A leg's centre line in world XY, whichever local axis it runs along.
	void LegEnds(const UHutongWallBuildingComponent* Leg, FVector2D& OutStart, FVector2D& OutEnd);

	// The face a leg's outer end is cut on, read off its corners; unset when the end is square.
	HutongWallChain::FEndFace OuterFace(const UHutongWallBuildingComponent* Leg, bool bStart);

	// The run Seed is a leg of, walked both ways through Candidates: a leg joins the next where
	// its end sits on the other's start, both of one thickness. A lone wall is a run of one.
	bool Gather(UHutongWallBuildingComponent* Seed, const TArray<UHutongWallBuildingComponent*>& Candidates, FRun& Out);

	// The legs rebuilt for a new set of vertices, start and end cut on the faces given.
	bool Rebuild(const FRun& Run, const TArray<FVector2D>& Vertices,
		const HutongWallChain::FEndFace& StartFace, const HutongWallChain::FEndFace& EndFace,
		TArray<HutongWallChain::FSegment>& OutSegments);

	// Puts rebuilt segments onto the legs: transform, length, corner offsets. Nothing is redrawn
	// or rebaked here; the caller does that at the pace it wants.
	void Apply(const FRun& Run, const TArray<HutongWallChain::FSegment>& Segments);
}
