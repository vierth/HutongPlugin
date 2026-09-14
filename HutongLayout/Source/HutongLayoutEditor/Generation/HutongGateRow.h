#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "Generation/HutongRidge.h"

// Sizing a gate house to the street row it is set into. A 如意門 placed against a traced 倒座房
// is a 3 m box against a 6 m building: the style's band was written for a gate standing alone,
// and beside a row the row is the evidence. Its 進深 is the row's, since the street face is one
// continuous run of masonry, and its ridge stands clear above the row's, since the 大門 is the
// tallest thing on that face. The frontage stays inside the style's band: the bay is what the
// rank fixes. Plain scalars, so HutongLayout.Gates.MatchRow can run it without a world.
namespace HutongGen::GateRow
{
	// The row's depth is the extent behind its own facade. Measured off the row and never off the
	// gate's frame: the gate's axes at the first click come from the camera and from whichever
	// face was clicked, and read along the run they made a gate the length of the row.
	inline double RowDepth(const FVector2D& Size, bool bHasFacade, bool bFacadeAlongX)
	{
		if (bHasFacade) return bFacadeAlongX ? Size.Y : Size.X;
		return FMath::Min(Size.X, Size.Y);
	}

	// The bearing of the row's run, which a gate set into it shares: the row actor's yaw when
	// the facade runs along its local X, a quarter turn on when it runs along Y.
	inline double RunYawDeg(double ActorYawDeg, bool bFacadeAlongX)
	{
		return bFacadeAlongX ? ActorYawDeg : ActorYawDeg + 90.0;
	}

	// Whether a neighbour is a row a gate can be set into: something with an eave, and deep
	// enough to be a building rather than a wall run met end-on.
	inline bool IsRow(double NeighbourEave, double NeighbourDepth)
	{
		return NeighbourEave > 0.0 && NeighbourDepth >= 150.0;
	}

	// Raises the gate's eave until its ridge clears the row's by Clearance. Zero row leaves it alone.
	inline void LiftGateAboveRidge(FHutongGateHouseParams& Gate, double GateDepth,
		double RowRidgeZ, double Clearance = HutongCanon::Gate::RidgeAboveRowCm)
	{
		if (RowRidgeZ <= 0.0) return;
		const double Have = Ridge::Gate(Gate, GateDepth);
		const double Want = RowRidgeZ + FMath::Max(Clearance, 0.0);
		if (Have < Want) Gate.EaveHeight = Gate.GetEaveHeight() + (Want - Have);
	}
}
