#pragma once

#include "CoreMinimal.h"
#include "Tools/HutongWallChain.h"

class UHutongWallBuildingComponent;

// Placed wall legs read back off the level, each end meeting the next start, so moving a vertex
// rebuilds every leg meeting there.
namespace HutongWallRun
{
	struct FRun
	{
		TArray<TWeakObjectPtr<UHutongWallBuildingComponent>> Legs;
		// Legs + 1: leg i runs Vertices[i] -> Vertices[i + 1], on the centre line.
		TArray<FVector2D> Vertices;
		double Z = 0.0;
		double Thickness = 0.0;
		// Setback of a leg continuing along a neighbour's face.
		double Setback = 0.0;
		// Current cut faces of the two outer ends; unset where square.
		HutongWallChain::FEndFace StartFace;
		HutongWallChain::FEndFace EndFace;

		int32 NumLegs() const { return Legs.Num(); }
		bool Contains(const UHutongWallBuildingComponent* Leg) const;
	};

	// Leg centre line in world XY, whichever local axis it runs along.
	void LegEnds(const UHutongWallBuildingComponent* Leg, FVector2D& OutStart, FVector2D& OutEnd);

	// Face the leg's outer end is cut on, read off its corners; unset when square.
	HutongWallChain::FEndFace OuterFace(const UHutongWallBuildingComponent* Leg, bool bStart);

	// Run containing Seed, walked both ways through Candidates: legs join where end meets start and
	// thickness matches. A lone wall is a run of one.
	bool Gather(UHutongWallBuildingComponent* Seed, const TArray<UHutongWallBuildingComponent*>& Candidates, FRun& Out);

	// Legs rebuilt for new vertices, outer ends cut on the given faces.
	bool Rebuild(const FRun& Run, const TArray<FVector2D>& Vertices,
		const HutongWallChain::FEndFace& StartFace, const HutongWallChain::FEndFace& EndFace,
		TArray<HutongWallChain::FSegment>& OutSegments);

	// Writes transform, length, corner offsets onto the legs. No redraw or rebake; caller does that.
	void Apply(const FRun& Run, const TArray<HutongWallChain::FSegment>& Segments);
}
