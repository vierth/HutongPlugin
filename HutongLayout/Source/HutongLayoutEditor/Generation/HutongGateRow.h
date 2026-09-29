#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "Generation/HutongRidge.h"

// Sizes a gate house to the street row it sits in: a style's band assumes a free-standing gate, but
// beside a row the row decides (a 如意門 against a 6 m 倒座房 was a 3 m box). 進深 = the row's (one
// continuous street face); ridge clears the row's (the 大門 is tallest on that face); frontage stays
// in the style's band (rank fixes the bay). Plain scalars so HutongLayout.Gates.MatchRow runs
// without a world.
namespace HutongGen::GateRow
{
	// Row depth behind its own facade. Measured off the row, never the gate's frame: the gate's axes at
	// first click come from camera and clicked face, and read along the run gave a row-length gate.
	inline double RowDepth(const FVector2D& Size, bool bHasFacade, bool bFacadeAlongX)
	{
		if (bHasFacade) return bFacadeAlongX ? Size.Y : Size.X;
		return FMath::Min(Size.X, Size.Y);
	}

	// Row run bearing, shared by a gate set into it: actor yaw if the facade runs along local X, +90 if Y.
	inline double RunYawDeg(double ActorYawDeg, bool bFacadeAlongX)
	{
		return bFacadeAlongX ? ActorYawDeg : ActorYawDeg + 90.0;
	}

	// A neighbour is a row if it has an eave and is deeper than a wall run met end-on.
	inline bool IsRow(double NeighbourEave, double NeighbourDepth)
	{
		return NeighbourEave > 0.0 && NeighbourDepth >= 150.0;
	}

	// The eave that puts the gate's ridge at TargetRidgeZ, up or down. Solved, not added: 上檐出
	// derives from 柱高 and the overhang carries the first 舉, so a taller gate grows a taller roof
	// and adding the shortfall once overshoots.
	inline double EaveForRidge(const FHutongGateHouseParams& InGate, double GateDepth, double TargetRidgeZ)
	{
		FHutongGateHouseParams Gate = InGate;
		for (int32 Pass = 0; Pass < 6; ++Pass)
		{
			const double Have = Ridge::Gate(Gate, GateDepth);
			if (FMath::Abs(Have - TargetRidgeZ) < 0.005) break;
			FHutongGateHouseParams Probe = Gate;
			Probe.EaveHeight = Gate.GetEaveHeight() + 10.0;
			const double Slope = 0.1 * (Ridge::Gate(Probe, GateDepth) - Have);
			if (Slope < UE_KINDA_SMALL_NUMBER) break;
			Gate.EaveHeight = Gate.GetEaveHeight() + (TargetRidgeZ - Have) / Slope;
		}
		return Gate.GetEaveHeight();
	}

	// Raises the gate's eave until its ridge clears the row's by Clearance; zero row = no change.
	inline void LiftGateAboveRidge(FHutongGateHouseParams& Gate, double GateDepth,
		double RowRidgeZ, double Clearance = HutongCanon::Gate::RidgeAboveRowCm)
	{
		if (RowRidgeZ <= 0.0) return;
		const double Want = RowRidgeZ + FMath::Max(Clearance, 0.0);
		if (Ridge::Gate(Gate, GateDepth) >= Want) return;
		// Never lowered.
		Gate.EaveHeight = FMath::Max(EaveForRidge(Gate, GateDepth, Want), Gate.GetEaveHeight());
	}
}
