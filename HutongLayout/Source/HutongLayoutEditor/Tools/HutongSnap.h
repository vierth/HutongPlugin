#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;
class UHutongBuildingComponent;

// Snapping placements to buildings already placed.
namespace HutongSnap
{
	// Cursor turned about Start to the nearest StepDeg off BaseYawDeg, at the same distance (Shift on a run's
	// segment). Z kept. Under a centimetre from Start, unchanged.
	inline FVector StepBearing(const FVector& Start, const FVector& Cursor, double BaseYawDeg, double StepDeg)
	{
		const double dx = Cursor.X - Start.X, dy = Cursor.Y - Start.Y;
		const double Len = FMath::Sqrt(dx * dx + dy * dy);
		if (Len < 1.0 || StepDeg <= 0.0) return Cursor;
		const double Angle = FMath::RadiansToDegrees(FMath::Atan2(dy, dx));
		const double R = FMath::DegreesToRadians(BaseYawDeg + FMath::RoundToDouble((Angle - BaseYawDeg) / StepDeg) * StepDeg);
		return FVector(Start.X + FMath::Cos(R) * Len, Start.Y + FMath::Sin(R) * Len, Cursor.Z);
	}

	// A placed building's four world footprint corners, never actor bounds (those include eaves, platform, 下鹼).
	struct FFootprint
	{
		TWeakObjectPtr<UHutongBuildingComponent> Building;
		TWeakObjectPtr<const AActor> Actor;
		FTransform ActorToWorld;
		FVector2D Size = FVector2D::ZeroVector;
		FVector Corners[4] = {};
		bool bPlanOnly = false;
	};

	// Placed buildings, gathered once and reused: per-query level walks cost four full actor passes per
	// frame. Refreshed on a short timer and on any level mutation (as PlaceLabels' region cache).
	// **One cache, not one per tool**: it outlives tool switches, and most level changes are not tools
	// (import, promote/rebuild ops, editor delete, undo). Those call Invalidate() here.
	struct FFootprintCache
	{
		void Refresh(UWorld* World);

		// Refreshes if older than MaxAgeSeconds, if the world changed, or after Invalidate. Negative age
		// disables age-out (mid-drag: nothing moves while the mouse is down).
		const TArray<FFootprint>& Get(UWorld* World, double MaxAgeSeconds = 1.0);

		void Invalidate() { LastRefreshSeconds = -1.0; }

		TArray<FFootprint> Items;
		TWeakObjectPtr<UWorld> GatheredFrom;
		double LastRefreshSeconds = -1.0;
	};

	// Shared by tools and ops.
	FFootprintCache& Cache();

	// Call after adding, removing, moving or resizing a building.
	inline void Invalidate() { Cache().Invalidate(); }
	struct FTarget
	{
		FVector Point = FVector::ZeroVector;
		// Yaws of the footprint edges meeting here.
		TArray<double> EdgeYawsDeg;
	};

	struct FResult
	{
		bool bSnapped = false;
		FVector Point = FVector::ZeroVector;
		// Yaw of the snapped edge, or a large negative if none.
		double EdgeYawDeg = -1000.0;
		// Other edge at a corner; large negative on an edge snap.
		double EdgeYaw2Deg = -1000.0;
		// Unit direction from the snapped point into its footprint.
		FVector2D Inward = FVector2D::ZeroVector;
		// Owning building, for placements that size to a neighbour.
		TWeakObjectPtr<UHutongBuildingComponent> Building;
	};

	// Nearest corner, then nearest point along an edge, within Radius of Query.
	FResult FindSnap(const TArray<FFootprint>& Footprints, const FVector& Query, double Radius,
		const AActor* IgnoreActor = nullptr);

	// Point confined to a line (Origin + t*Dir), snapped where that line crosses a neighbour's edge
	// line: the mitre a wall end needs on an off-square face, at any cursor distance. Query is the
	// unsnapped point; nearest crossing within Radius wins, edge lines counting only within Radius of
	// their segment. Never within 2 cm of Origin: a butted wall shares that corner with its neighbour.
	FResult FindSnapAlongLine(const TArray<FFootprint>& Footprints, const FVector& Origin,
		const FVector2D& Dir, const FVector& Query, double Radius, const AActor* IgnoreActor = nullptr);

	// Nearest of Candidates to Yaw, within Tolerance, else Yaw. Both in degrees.
	double SnapYaw(double YawDeg, const TArray<double>& CandidatesDeg, double ToleranceDeg);

	// Nearest roughly-parallel placed edge across from Query and its distance: a lane.
	struct FGap
	{
		bool bFound = false;
		double DistanceCm = 0.0;
		// Unit plan direction from Query toward the edge.
		FVector2D Toward = FVector2D::ZeroVector;
		// Edge bearing.
		double EdgeYawDeg = 0.0;
	};

	// AnyYaw as QueryYawDeg disables the bearing filter.
	inline constexpr double AnyYaw = -1000.0;

	FGap FindParallelGap(const TArray<FFootprint>& Footprints, const FVector& Query,
		double QueryYawDeg, double MaxDistance, double AngleToleranceDeg = 12.0,
		const AActor* IgnoreActor = nullptr);

	// All footprint edge yaws within Radius of Query, for rotation snapping.
	TArray<double> GatherEdgeYaws(const TArray<FFootprint>& Footprints, const FVector& Query,
		double Radius, const AActor* IgnoreActor = nullptr);
}
