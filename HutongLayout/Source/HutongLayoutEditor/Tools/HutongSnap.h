#pragma once

#include "CoreMinimal.h"

class UWorld;
class AActor;
class UHutongBuildingComponent;

// Snapping a placement to the buildings already down.
namespace HutongSnap
{
	// One placed building, as every query here needs it: the four world corners of its own
	// footprint, never its actor bounds, which include eaves, platform overhang and 下鹼 projection.
	struct FFootprint
	{
		TWeakObjectPtr<UHutongBuildingComponent> Building;
		TWeakObjectPtr<const AActor> Actor;
		FTransform ActorToWorld;
		FVector2D Size = FVector2D::ZeroVector;
		FVector Corners[4] = {};
		bool bPlanOnly = false;
	};

	// The placed buildings, gathered once and reused. Every query below used to walk the whole
	// level itself, and the tools ask two or three of them per hover tick and again out of Render
	// — four passes over every actor in the world, every frame, on a plugin whose point is a
	// district. The cache is refreshed on a short timer and whenever anything mutates the level,
	// which is the same bargain PlaceLabels' region cache makes.
	//
	// **One cache, not one per tool.** It outlives a tool switch, and — more to the point — the
	// things that change what is in the level are mostly not tools: the import, the promote and
	// rebuild ops, an editor delete, an undo. Those raise Invalidate() by calling it here rather
	// than reaching into whichever tool happens to be up.
	struct FFootprintCache
	{
		void Refresh(UWorld* World);

		// Refreshed if it is older than MaxAgeSeconds, if the world changed under it, or if
		// Invalidate has been called since. Pass a negative age to refuse the age-out, which is
		// what a tool does mid-drag: nothing can have moved while the mouse is down.
		const TArray<FFootprint>& Get(UWorld* World, double MaxAgeSeconds = 1.0);

		void Invalidate() { LastRefreshSeconds = -1.0; }

		TArray<FFootprint> Items;
		TWeakObjectPtr<UWorld> GatheredFrom;
		double LastRefreshSeconds = -1.0;
	};

	// The one the tools and the ops share.
	FFootprintCache& Cache();

	// After anything that adds, removes, moves or resizes a building.
	inline void Invalidate() { Cache().Invalidate(); }
	struct FTarget
	{
		FVector Point = FVector::ZeroVector;
		// Directions of the footprint edges meeting here.
		TArray<double> EdgeYawsDeg;
	};

	struct FResult
	{
		bool bSnapped = false;
		FVector Point = FVector::ZeroVector;
		// Yaw of the edge the snap landed on or beside, or a large negative when there is none.
		double EdgeYawDeg = -1000.0;
		// At a corner, the other edge meeting there; a large negative on an edge snap.
		double EdgeYaw2Deg = -1000.0;
		// Unit direction from the snapped point into the footprint it belongs to.
		FVector2D Inward = FVector2D::ZeroVector;
		// The placed building the point belongs to, for a placement that sizes itself to a neighbour.
		TWeakObjectPtr<UHutongBuildingComponent> Building;
	};

	// Nearest corner, then nearest point along an edge, within Radius of Query.
	FResult FindSnap(const TArray<FFootprint>& Footprints, const FVector& Query, double Radius,
		const AActor* IgnoreActor = nullptr);

	// A point held to a line — a corner that may only slide along its run, or across it — snapped
	// to where that line crosses a neighbour's edge line: the mitre a wall end wants against an
	// off-square face, found however far from the face the cursor is. Query is where the point
	// would sit unsnapped, on the line through Origin along Dir; the crossing nearest it within
	// Radius wins, an edge line counting only within Radius of its own segment. Never within
	// 2 cm of Origin: a butted wall shares that corner with its neighbour.
	FResult FindSnapAlongLine(const TArray<FFootprint>& Footprints, const FVector& Origin,
		const FVector2D& Dir, const FVector& Query, double Radius, const AActor* IgnoreActor = nullptr);

	// Nearest of Candidates to Yaw, within Tolerance, else Yaw. Both in degrees.
	double SnapYaw(double YawDeg, const TArray<double>& CandidatesDeg, double ToleranceDeg);

	// The nearest roughly-parallel placed edge across from Query, and how far away it is — which is what a lane is.
	struct FGap
	{
		bool bFound = false;
		double DistanceCm = 0.0;
		// Unit direction from Query toward the edge, in plan.
		FVector2D Toward = FVector2D::ZeroVector;
		// The edge's own bearing.
		double EdgeYawDeg = 0.0;
	};

	// Pass AnyYaw for QueryYawDeg to drop the bearing filter.
	inline constexpr double AnyYaw = -1000.0;

	FGap FindParallelGap(const TArray<FFootprint>& Footprints, const FVector& Query,
		double QueryYawDeg, double MaxDistance, double AngleToleranceDeg = 12.0,
		const AActor* IgnoreActor = nullptr);

	// Every footprint edge yaw within Radius of Query, for snapping a rotation to a neighbour.
	TArray<double> GatherEdgeYaws(const TArray<FFootprint>& Footprints, const FVector& Query,
		double Radius, const AActor* IgnoreActor = nullptr);
}
