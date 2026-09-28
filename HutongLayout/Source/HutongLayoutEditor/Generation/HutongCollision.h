#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/AggregateGeom.h"

namespace UE::Geometry { class FDynamicMesh3; struct FSimpleShapeSet3d; }

namespace HutongGen
{
	// Simple collision from a built mesh: every primitive is its own closed solid, so each connected
	// piece gets a box (where it is one) or a convex hull. Pieces too small to stop anyone (lattice
	// bars, 門簪, studs) are left out. Openings stay open: piers and lintels are separate pieces.
	struct FCollisionStats
	{
		int32 Pieces = 0;      // connected pieces in the mesh
		int32 Kept = 0;        // pieces that became a shape
		int32 Boxes = 0;
		int32 Convexes = 0;
	};

	// OutShapes: the same shapes in geometry form, for checks.
	void BuildSimpleCollision(const UE::Geometry::FDynamicMesh3& Mesh, FKAggregateGeom& Out,
		FCollisionStats* Stats = nullptr, UE::Geometry::FSimpleShapeSet3d* OutShapes = nullptr);

	// Whether P lies inside any of the shapes.
	bool CollisionContains(const UE::Geometry::FSimpleShapeSet3d& Shapes, const FVector3d& P);
}
