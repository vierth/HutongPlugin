#include "Generation/HutongCollision.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "Selections/MeshConnectedComponents.h"
#include "ShapeApproximation/MeshSimpleShapeApproximation.h"
#include "Physics/PhysicsDataCollection.h"

namespace HutongGen
{
	namespace
	{
		// Smaller than this (bounding volume, cm³) stops nobody: a 3 x 3 cm lattice bar two metres long is 1,800.
		constexpr double MinPieceVolume = 20000.0;
		// Thinner than this is a sheet, not a solid.
		constexpr double MinPieceThickness = 1.0;
	}

	void BuildSimpleCollision(const UE::Geometry::FDynamicMesh3& Mesh, FKAggregateGeom& Out,
		FCollisionStats* Stats, UE::Geometry::FSimpleShapeSet3d* OutShapes)
	{
		using namespace UE::Geometry;
		Out.EmptyElements();
		FCollisionStats Local;

		FMeshConnectedComponents Components(&Mesh);
		Components.FindConnectedTriangles();
		Local.Pieces = Components.Num();

		TArray<FDynamicMesh3> Pieces;
		Pieces.Reserve(Components.Num());
		for (const FMeshConnectedComponents::FComponent& C : Components)
		{
			FAxisAlignedBox3d Box = FAxisAlignedBox3d::Empty();
			for (const int32 Tid : C.Indices)
			{
				const FIndex3i T = Mesh.GetTriangle(Tid);
				Box.Contain(Mesh.GetVertex(T.A)); Box.Contain(Mesh.GetVertex(T.B)); Box.Contain(Mesh.GetVertex(T.C));
			}
			const FVector3d Ext = Box.Diagonal();
			if (Ext.X * Ext.Y * Ext.Z < MinPieceVolume || Ext.GetMin() < MinPieceThickness) continue;

			FDynamicMesh3& Piece = Pieces.AddDefaulted_GetRef();
			TMap<int32, int32> Map;
			for (const int32 Tid : C.Indices)
			{
				const FIndex3i T = Mesh.GetTriangle(Tid);
				int32 V[3];
				for (int32 k = 0; k < 3; ++k)
				{
					const int32 Src = T[k];
					const int32* Found = Map.Find(Src);
					V[k] = Found ? *Found : Map.Add(Src, Piece.AppendVertex(Mesh.GetVertex(Src)));
				}
				Piece.AppendTriangle(V[0], V[1], V[2]);
			}
		}
		Local.Kept = Pieces.Num();

		if (Pieces.Num() > 0)
		{
			TArray<const FDynamicMesh3*> Inputs;
			for (const FDynamicMesh3& P : Pieces) Inputs.Add(&P);

			FMeshSimpleShapeApproximation Fit;
			Fit.bDetectSpheres = false;
			Fit.bDetectCapsules = false;
			Fit.bDetectBoxes = true;
			Fit.bDetectConvexes = true;
			Fit.bSimplifyHulls = true;
			Fit.HullTargetFaceCount = 24;
			Fit.InitializeSourceMeshes(Inputs);

			FPhysicsDataCollection Collection;
			Fit.Generate_ConvexHulls(Collection.Geometry);
			Collection.CopyGeometryToAggregate();
			if (OutShapes) *OutShapes = Collection.Geometry;
			Out = Collection.AggGeom;
			Local.Boxes = Out.BoxElems.Num();
			Local.Convexes = Out.ConvexElems.Num();
		}
		if (Stats) *Stats = Local;
	}

	bool CollisionContains(const UE::Geometry::FSimpleShapeSet3d& Shapes, const FVector3d& P)
	{
		using namespace UE::Geometry;
		for (const FBoxShape3d& B : Shapes.Boxes)
		{
			if (B.Box.Contains(P)) return true;
		}
		for (const FConvexShape3d& C : Shapes.Convexes)
		{
			const FDynamicMesh3& H = C.Mesh;
			// Signed volume picks which side of each face is inside.
			double Vol = 0.0;
			for (const int32 Tid : H.TriangleIndicesItr())
			{
				FVector3d A, B, D;
				H.GetTriVertices(Tid, A, B, D);
				Vol += A.Dot(B.Cross(D));
			}
			const double Sign = Vol >= 0.0 ? 1.0 : -1.0;
			bool bInside = H.TriangleCount() > 0;
			for (const int32 Tid : H.TriangleIndicesItr())
			{
				FVector3d A, B, D;
				H.GetTriVertices(Tid, A, B, D);
				if (Sign * (B - A).Cross(D - A).Dot(P - A) > 0.0) { bInside = false; break; }
			}
			if (bInside) return true;
		}
		return false;
	}
}
