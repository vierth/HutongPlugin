#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;
class UHutongBuildingComponent;

namespace HutongSnap
{
	inline FVector StepBearing(const FVector& Start, const FVector& Cursor, double BaseYawDeg, double StepDeg)
	{
		const double dx = Cursor.X - Start.X, dy = Cursor.Y - Start.Y;
		const double Len = FMath::Sqrt(dx * dx + dy * dy);
		if (Len < 1.0 || StepDeg <= 0.0) return Cursor;
		const double Angle = FMath::RadiansToDegrees(FMath::Atan2(dy, dx));
		const double R = FMath::DegreesToRadians(BaseYawDeg + FMath::RoundToDouble((Angle - BaseYawDeg) / StepDeg) * StepDeg);
		return FVector(Start.X + FMath::Cos(R) * Len, Start.Y + FMath::Sin(R) * Len, Cursor.Z);
	}

	struct FFootprint
	{
		TWeakObjectPtr<UHutongBuildingComponent> Building;
		TWeakObjectPtr<const AActor> Actor;
		FTransform ActorToWorld;
		FVector2D Size = FVector2D::ZeroVector;
		FVector Corners[4] = {};
		bool bPlanOnly = false;
	};

	// Gathered once and reused: per-query level walks cost four full actor passes per frame.
	// One cache for all tools; level changes from outside tools (import, ops, editor delete, undo) call Invalidate().
	struct FFootprintCache
	{
		void Refresh(UWorld* World);

		// Negative MaxAgeSeconds disables age-out (mid-drag).
		const TArray<FFootprint>& Get(UWorld* World, double MaxAgeSeconds = 1.0);

		void Invalidate() { LastRefreshSeconds = -1.0; }

		TArray<FFootprint> Items;
		TWeakObjectPtr<UWorld> GatheredFrom;
		double LastRefreshSeconds = -1.0;
	};

	FFootprintCache& Cache();

	// Call after adding, removing, moving or resizing a building.
	inline void Invalidate() { Cache().Invalidate(); }
	struct FTarget
	{
		FVector Point = FVector::ZeroVector;
		TArray<double> EdgeYawsDeg;
	};

	struct FResult
	{
		bool bSnapped = false;
		FVector Point = FVector::ZeroVector;
		// Large negative if none.
		double EdgeYawDeg = -1000.0;
		double EdgeYaw2Deg = -1000.0;
		FVector2D Inward = FVector2D::ZeroVector;
		TWeakObjectPtr<UHutongBuildingComponent> Building;
	};

	FResult FindSnap(const TArray<FFootprint>& Footprints, const FVector& Query, double Radius,
		const AActor* IgnoreActor = nullptr);

	// Skips crossings within 2 cm of Origin: a butted wall shares that corner with its neighbour.
	FResult FindSnapAlongLine(const TArray<FFootprint>& Footprints, const FVector& Origin,
		const FVector2D& Dir, const FVector& Query, double Radius, const AActor* IgnoreActor = nullptr);

	double SnapYaw(double YawDeg, const TArray<double>& CandidatesDeg, double ToleranceDeg);

	struct FGap
	{
		bool bFound = false;
		double DistanceCm = 0.0;
		FVector2D Toward = FVector2D::ZeroVector;
		double EdgeYawDeg = 0.0;
	};

	// AnyYaw as QueryYawDeg disables the bearing filter.
	inline constexpr double AnyYaw = -1000.0;

	FGap FindParallelGap(const TArray<FFootprint>& Footprints, const FVector& Query,
		double QueryYawDeg, double MaxDistance, double AngleToleranceDeg = 12.0,
		const AActor* IgnoreActor = nullptr);

	TArray<double> GatherEdgeYaws(const TArray<FFootprint>& Footprints, const FVector& Query,
		double Radius, const AActor* IgnoreActor = nullptr);
}
