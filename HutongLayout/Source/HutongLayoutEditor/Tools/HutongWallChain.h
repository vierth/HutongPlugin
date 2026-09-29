#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongFootprint.h"

// Polyline wall: one rectangle per segment, joins mitred on the bisector, outer ends cut flush on
// the face they rest on. Pure ground-plane arithmetic shared by preview, spawn and tests.
namespace HutongWallChain
{
	struct FSegment
	{
		// World XY of the min corner; segment builds along local +X, thickness along +Y.
		FVector2D Origin = FVector2D::ZeroVector;
		double YawDeg = 0.0;
		double Length = 0.0;
		// Along-run corner offsets: miters at joins, flush cuts at outer ends.
		FHutongFootprintSkew Skew;
	};

	// Neighbour face an outer end rests on: snapped point and face direction.
	struct FEndFace
	{
		bool bSet = false;
		FVector2D Point = FVector2D::ZeroVector;
		FVector2D Dir = FVector2D::ZeroVector;
	};

	// Points: ground polyline. DrawnY: the line's local Y across the wall (half thickness = centre line,
	// 0 or thickness = a face). Segments under MinLength drop, so repeated points are harmless. False
	// when no segment remains.
	bool Build(const TArray<FVector2D>& Points, double Thickness, double DrawnY,
		const FEndFace& StartFace, const FEndFace& EndFace, TArray<FSegment>& OutSegments,
		double MinLength = 10.0);

	// Segment's four world corners, offsets applied, anticlockwise from origin.
	void Corners(const FSegment& Segment, double Thickness, FVector2D OutCorners[4]);

	// Signed turn between directions, yaw convention, degrees.
	double TurnDeg(const FVector2D& From, const FVector2D& To);

	// Where the drawn line sits when a vertex snapped to a neighbour: if an edge there runs along the
	// segment (within ToleranceDeg), the outer face goes on that edge's line, body on the neighbour's side,
	// so a wall continuing a house front stands flush, not centred. Yaw2: other edge at a corner, else a
	// large negative. Inward: from snapped point into the neighbour. False = no such edge, line centred.
	// A caller that chose "along" last frame passes WithinDegAfter (hysteresis) so the run does not jump
	// half a thickness per pixel near the limit. Setback holds the outer face that far inside the
	// neighbour's face (drawn line at -Setback or Thickness + Setback), so the wall reads as a separate piece.
	// A run end on a neighbour's corner (a wall is snapped to corners only): the end's short face rests
	// on the corner's edge more nearly square to the run, and the run's outside corner is the
	// neighbour's corner exactly — the drawn line is that outer face (0 or Thickness), the body on the
	// side the face edge runs from the corner. SegmentDir: the run's direction of travel; bAtStart: the
	// corner is the run's first vertex (else its last). Yaw1/Yaw2: the corner's two edges. Inward: from
	// the corner into the neighbour. OutFace: the end cut, through the corner along that edge.
	void CornerEnd(const FVector2D& SegmentDir, bool bAtStart, double Yaw1Deg, double Yaw2Deg,
		const FVector2D& Inward, const FVector2D& Corner, double Thickness, double& OutDrawnY, FEndFace& OutFace);

	inline constexpr double AlongFaceDeg = 10.0;
	inline constexpr double AlongFaceDegAfter = 16.0;
	bool SideAlongFace(const FVector2D& SegmentDir, double EdgeYawDeg, double EdgeYaw2Deg,
		const FVector2D& Inward, double Thickness, double& OutDrawnY, double ToleranceDeg = AlongFaceDeg,
		double Setback = 0.0);
}
