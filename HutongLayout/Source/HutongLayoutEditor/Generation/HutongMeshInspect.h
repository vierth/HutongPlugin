#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"

// Solidity and z-fight checks for generated meshes.
namespace HutongMeshInspect
{
	struct FShellReport
	{
		int32 Triangles = 0;
		int32 Unmatched = 0;      // edges without exactly one opposing partner
		int32 Duplicated = 0;     // same directed edge twice: a winding flip
		double Volume = 0.0;

		// Closed, two-manifold, consistently wound, positive volume.
		bool IsSolid() const { return Unmatched == 0 && Duplicated == 0 && Volume > 0.0; }
	};

	// Edges are matched by position, not index.
	inline FShellReport InspectShell(const UE::Geometry::FDynamicMesh3& Mesh)
	{
		FShellReport R;

		auto Key = [](const FVector3d& P)
		{
			// 1e-4 cm grid.
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

	// Same-plane, same-facing triangle pairs overlapping beyond a sliver: z-fights. Opposite-facing
	// pairs are fine. Only faces wholly above MinZ count.
	inline int32 CountSameFacingOverlaps(const UE::Geometry::FDynamicMesh3& Mesh, double MinZ = -BIG_NUMBER, TArray<FString>* OutWhere = nullptr)
	{
		TMap<FIntVector4, TArray<int32>> Planes;
		for (int32 tid : Mesh.TriangleIndicesItr())
		{
			FVector3d A, B, C;
			Mesh.GetTriVertices(tid, A, B, C);
			if (FMath::Min3(A.Z, B.Z, C.Z) < MinZ) continue;
			const FVector3d N = (B - A).Cross(C - A);
			if (N.Length() < 1e-6) continue;
			const FVector3d U = N.GetSafeNormal();
			Planes.FindOrAdd(FIntVector4(FMath::RoundToInt32(U.X * 1000.0), FMath::RoundToInt32(U.Y * 1000.0),
				FMath::RoundToInt32(U.Z * 1000.0), FMath::RoundToInt32(U.Dot(A) * 10.0))).Add(tid);
		}

		// In-plane separating axes; overlap only if every axis overlaps by more than Eps.
		constexpr double Eps = 0.05;
		auto Overlap = [&Mesh](int32 T0, int32 T1)
		{
			FVector3d P[2][3];
			Mesh.GetTriVertices(T0, P[0][0], P[0][1], P[0][2]);
			Mesh.GetTriVertices(T1, P[1][0], P[1][1], P[1][2]);
			const FVector3d N = (P[0][1] - P[0][0]).Cross(P[0][2] - P[0][0]).GetSafeNormal();
			for (int32 t = 0; t < 2; ++t)
			{
				for (int32 e = 0; e < 3; ++e)
				{
					const FVector3d Axis = N.Cross(P[t][(e + 1) % 3] - P[t][e]).GetSafeNormal();
					double Min[2] = { BIG_NUMBER, BIG_NUMBER }, Max[2] = { -BIG_NUMBER, -BIG_NUMBER };
					for (int32 k = 0; k < 2; ++k)
					{
						for (int32 v = 0; v < 3; ++v)
						{
							const double D = Axis.Dot(P[k][v]);
							Min[k] = FMath::Min(Min[k], D);
							Max[k] = FMath::Max(Max[k], D);
						}
					}
					if (FMath::Min(Max[0], Max[1]) - FMath::Max(Min[0], Min[1]) <= Eps) return false;
				}
			}
			return true;
		};

		int32 Count = 0;
		for (const TPair<FIntVector4, TArray<int32>>& It : Planes)
		{
			const TArray<int32>& Tris = It.Value;
			for (int32 i = 0; i < Tris.Num(); ++i)
			{
				for (int32 j = i + 1; j < Tris.Num(); ++j)
				{
					if (!Overlap(Tris[i], Tris[j])) continue;
					++Count;
					if (OutWhere && OutWhere->Num() < 12)
					{
						OutWhere->Add(FString::Printf(TEXT("n(%d,%d,%d)/1000 at %.1f: %s and %s"),
							It.Key.X, It.Key.Y, It.Key.Z, It.Key.W / 10.0,
							*Mesh.GetTriCentroid(Tris[i]).ToString(), *Mesh.GetTriCentroid(Tris[j]).ToString()));
					}
				}
			}
		}
		return Count;
	}
}
