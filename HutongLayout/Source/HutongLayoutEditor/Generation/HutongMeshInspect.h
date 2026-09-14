#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

// Whether a mesh is the kind of solid this plugin's architecture rests on.
namespace HutongMeshInspect
{
	struct FShellReport
	{
		int32 Triangles = 0;
		int32 Unmatched = 0;      // edges without exactly one opposing partner
		int32 Duplicated = 0;     // the same directed edge emitted twice: a winding flip
		double Volume = 0.0;

		// Closed, two-manifold, consistently wound and inside-out-free.
		bool IsSolid() const { return Unmatched == 0 && Duplicated == 0 && Volume > 0.0; }
	};

	// Edges are matched by position, not index.
	inline FShellReport InspectShell(const UE::Geometry::FDynamicMesh3& Mesh)
	{
		FShellReport R;

		auto Key = [](const FVector3d& P)
		{
			// 1e-4 cm is a thousandth of a millimetre.
			return FIntVector3(
				(int32)FMath::RoundToInt(P.X * 1e4),
				(int32)FMath::RoundToInt(P.Y * 1e4),
				(int32)FMath::RoundToInt(P.Z * 1e4));
		};

		TMap<TPair<FIntVector3, FIntVector3>, int32> Directed;
		for (int32 tid = 0; tid < Mesh.MaxTriangleID(); ++tid)
		{
			if (!Mesh.IsTriangle(tid)) continue;
			++R.Triangles;

			const UE::Geometry::FIndex3i T = Mesh.GetTriangle(tid);
			const FVector3d A = Mesh.GetVertex(T.A), B = Mesh.GetVertex(T.B), C = Mesh.GetVertex(T.C);
			R.Volume += A.Dot(B.Cross(C)) / 6.0;

			const FIntVector3 Ka = Key(A), Kb = Key(B), Kc = Key(C);
			for (const TPair<FIntVector3, FIntVector3>& E :
				{ TPair<FIntVector3, FIntVector3>(Ka, Kb),
				  TPair<FIntVector3, FIntVector3>(Kb, Kc),
				  TPair<FIntVector3, FIntVector3>(Kc, Ka) })
			{
				Directed.FindOrAdd(E, 0) += 1;
			}
		}

		for (const TPair<TPair<FIntVector3, FIntVector3>, int32>& It : Directed)
		{
			if (It.Value > 1) R.Duplicated += It.Value - 1;

			const TPair<FIntVector3, FIntVector3> Opposite(It.Key.Value, It.Key.Key);
			const int32* Back = Directed.Find(Opposite);
			if (!Back || *Back != It.Value) ++R.Unmatched;
		}
		return R;
	}
}
