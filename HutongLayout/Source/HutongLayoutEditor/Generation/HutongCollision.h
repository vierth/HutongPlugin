#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/AggregateGeom.h"

namespace UE::Geometry { class FDynamicMesh3; struct FSimpleShapeSet3d; }

namespace HutongGen
{
	// Every primitive is its own closed solid, so each connected piece gets a box or a convex hull.
	struct FCollisionStats
	{
		int32 Pieces = 0;
		int32 Kept = 0;
		int32 Boxes = 0;
		int32 Convexes = 0;
	};

	void BuildSimpleCollision(const UE::Geometry::FDynamicMesh3& Mesh, FKAggregateGeom& Out,
		FCollisionStats* Stats = nullptr, UE::Geometry::FSimpleShapeSet3d* OutShapes = nullptr);

	bool CollisionContains(const UE::Geometry::FSimpleShapeSet3d& Shapes, const FVector3d& P);
}
