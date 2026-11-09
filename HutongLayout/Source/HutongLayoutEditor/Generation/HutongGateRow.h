#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "Generation/HutongRidge.h"

// The row decides a gate's 進深 and ridge; its frontage stays in the style's band.
namespace HutongGen::GateRow
{
	// Measured off the row: the gate's axes at first click come from camera and clicked face.
	inline double RowDepth(const FVector2D& Size, bool bHasFacade, bool bFacadeAlongX)
	{
		if (bHasFacade) return bFacadeAlongX ? Size.Y : Size.X;
		return FMath::Min(Size.X, Size.Y);
	}

	inline double RunYawDeg(double ActorYawDeg, bool bFacadeAlongX)
	{
		return bFacadeAlongX ? ActorYawDeg : ActorYawDeg + 90.0;
	}

	inline bool IsRow(double NeighbourEave, double NeighbourDepth)
	{
		return NeighbourEave > 0.0 && NeighbourDepth >= 150.0;
	}

	// Solved: 上檐出 derives from 柱高, so adding the shortfall a single time overshoots.
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

	inline void LiftGateAboveRidge(FHutongGateHouseParams& Gate, double GateDepth,
		double RowRidgeZ, double Clearance = HutongCanon::Gate::RidgeAboveRowCm)
	{
		if (RowRidgeZ <= 0.0) return;
		const double Want = RowRidgeZ + FMath::Max(Clearance, 0.0);
		if (Ridge::Gate(Gate, GateDepth) >= Want) return;
		Gate.EaveHeight = FMath::Max(EaveForRidge(Gate, GateDepth, Want), Gate.GetEaveHeight());
	}
}
