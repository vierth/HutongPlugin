#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongFootprint.h"

namespace HutongWallChain
{
	struct FSegment
	{
		// World XY of the min corner; segment builds along local +X, thickness along +Y.
		FVector2D Origin = FVector2D::ZeroVector;
		double YawDeg = 0.0;
		double Length = 0.0;
		FHutongFootprintSkew Skew;
	};

	struct FEndFace
	{
		bool bSet = false;
		FVector2D Point = FVector2D::ZeroVector;
		FVector2D Dir = FVector2D::ZeroVector;
	};

	// DrawnY: the line's local Y across the wall (half thickness = centre line, 0 or thickness = a face).
	bool Build(const TArray<FVector2D>& Points, double Thickness, double DrawnY,
		const FEndFace& StartFace, const FEndFace& EndFace, TArray<FSegment>& OutSegments,
		double MinLength = 10.0);

	void Corners(const FSegment& Segment, double Thickness, FVector2D OutCorners[4]);

	// A placed leg carried on by a new run: Existing with the join end's corners taken from FromFar, the leg rebuilt
	// from its far end to the join (the join its +X end). bJoinAtStart: the join is the leg's own start, built the other way.
	FHutongFootprintSkew CarryJoinSkew(const FHutongFootprintSkew& Existing, bool bJoinAtStart, const FHutongFootprintSkew& FromFar);

	// A segment's skew read from its other end: a half turn swaps and negates the corners.
	FHutongFootprintSkew Reversed(const FHutongFootprintSkew& Skew);

	double TurnDeg(const FVector2D& From, const FVector2D& To);

	// CornerEnd: the run's outside corner is the neighbour's corner exactly; the drawn line is that outer face.
	void CornerEnd(const FVector2D& SegmentDir, bool bAtStart, double Yaw1Deg, double Yaw2Deg,
		const FVector2D& Inward, const FVector2D& Corner, double Thickness, double& OutDrawnY, FEndFace& OutFace);

	// SideAlongFace: Yaw2 is a large negative without a second edge; WithinDegAfter gives hysteresis near the limit.
	inline constexpr double AlongFaceDeg = 10.0;
	inline constexpr double AlongFaceDegAfter = 16.0;
	bool SideAlongFace(const FVector2D& SegmentDir, double EdgeYawDeg, double EdgeYaw2Deg,
		const FVector2D& Inward, double Thickness, double& OutDrawnY, double ToleranceDeg = AlongFaceDeg,
		double Setback = 0.0);
}
