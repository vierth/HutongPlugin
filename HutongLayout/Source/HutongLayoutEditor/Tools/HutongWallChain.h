#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongFootprint.h"

// A run of wall drawn as a polyline: one rectangle per segment, the ends where two segments meet
// cut on the bisector so they meld, and the two outer ends cut flush against whatever face they
// rest on. Pure arithmetic on ground points, so the tool's preview, its spawn and the tests all
// read the same corners.
namespace HutongWallChain
{
	struct FSegment
	{
		// World XY of the rectangle's min corner: the segment builds along its own +X from here,
		// thickness along +Y, which is what the actor's transform then places.
		FVector2D Origin = FVector2D::ZeroVector;
		double YawDeg = 0.0;
		double Length = 0.0;
		// Along-run corner offsets — the miters at the joins, the flush cuts at the outer ends.
		FHutongFootprintSkew Skew;
	};

	// A neighbour's face line an outer end rests on: the snapped point and the face's direction.
	struct FEndFace
	{
		bool bSet = false;
		FVector2D Point = FVector2D::ZeroVector;
		FVector2D Dir = FVector2D::ZeroVector;
	};

	// Points: the drawn polyline, on the ground. DrawnY: where that line sits across the wall, as
	// the local Y of the rectangle it is drawn inside — half the thickness for a centre line, 0 or
	// the thickness when the line is one face. Segments shorter than MinLength are dropped, so a
	// repeated point costs nothing. Returns false when fewer than one segment remains.
	bool Build(const TArray<FVector2D>& Points, double Thickness, double DrawnY,
		const FEndFace& StartFace, const FEndFace& EndFace, TArray<FSegment>& OutSegments,
		double MinLength = 10.0);

	// The four world corners of a segment, offsets applied, from its origin anticlockwise.
	void Corners(const FSegment& Segment, double Thickness, FVector2D OutCorners[4]);

	// The turn between two directions, signed the way yaw is, in degrees.
	double TurnDeg(const FVector2D& From, const FVector2D& To);

	// Where a run's drawn line sits across the wall when a vertex of it snapped to a neighbour:
	// if one of the edges meeting there runs along the segment (within AlongFaceDeg), the wall's outer
	// face goes on that edge's line and the body on the neighbour's side of it — a wall
	// continuing a house's front wall stands flush with it, not centred on it. Yaw2 is the other
	// edge at a corner, or a large negative; Inward points from the snapped point into the
	// neighbour. Returns false when no edge runs along the segment, leaving the line centred.
	// ToleranceDeg is how far off the segment an edge may run and still count; a caller that
	// decided "along" last frame passes the wider WithinDegAfter, so a segment near the limit does
	// not flip the whole run half a thickness sideways with every pixel of mouse movement.
	// Setback holds the outer face that far inside the neighbour's face — the drawn line then
	// lies outside the wall, at -Setback or Thickness + Setback — so the run reads as a piece
	// set against the house rather than the same slab continued.
	inline constexpr double AlongFaceDeg = 10.0;
	inline constexpr double AlongFaceDegAfter = 16.0;
	bool SideAlongFace(const FVector2D& SegmentDir, double EdgeYawDeg, double EdgeYaw2Deg,
		const FVector2D& Inward, double Thickness, double& OutDrawnY, double ToleranceDeg = AlongFaceDeg,
		double Setback = 0.0);
}
