#include "Generation/WaterJarGenerator.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	void BuildWaterJar(FDynamicMesh3& Mesh, const FHutongWaterJarParams& P)
	{
		using namespace HutongMeshUtils;

		const double Belly = FMath::Max(P.BellyDiameter, 1.0);
		const double H = FMath::Max(P.Height, 5.0);
		const int32 Sides = FMath::Clamp(P.Sides, 6, 96);

		const double FootR = 0.5 * Belly * FMath::Clamp(P.FootFraction, 0.1, 1.0);
		const double BellyR = 0.5 * Belly;
		const double MouthR = 0.5 * Belly * FMath::Clamp(P.MouthFraction, 0.1, 1.0);
		const double BellyZ = H * FMath::Clamp(P.BellyFraction, 0.05, 0.95);

		// The whole footprint is centred on the drag.
		const double Span = P.GetFootprint();
		const double CX = 0.5 * Span;
		const double CY = 0.5 * Span;

		// 1) 缸座 first, so the jar's foot sits on it.
		double Z0 = 0.0;
		if (P.bHasBase && P.BaseHeight > 0.0)
		{
			const int32 BaseFirstTri = Mesh.MaxTriangleID();
			const double BH = FMath::Max(P.BaseHeight, 1.0);
			const double Half = FMath::Max(FootR + FMath::Max(P.BaseMargin, 0.0), 1.0);
			AppendBox(Mesh,
				FVector3d(CX - Half, CY - Half, 0.0),
				FVector3d(CX + Half, CY + Half, BH));
			SetMaterialIDForTrianglesFrom(Mesh, BaseFirstTri, MatSlot_Stone);
			Z0 = BH;
		}

		// The jar: one unit circle scaled per station up the axis, the same way AppendColumn does 收分 and the 垂蓮柱 gets its bud.
		const TArray<FVector2d> Circle = MakeCircleProfile(1.0, Sides);

		struct FStation { double Z; double R; };
		const double NeckR = FMath::Min(MouthR * 0.94, BellyR);
		const FStation Stations[] = {
			{ 0.0,                  FootR * 0.94 },
			{ H * 0.04,             FootR        },
			{ BellyZ,               BellyR       },
			{ FMath::Lerp(BellyZ, H, 0.62), FMath::Lerp(BellyR, NeckR, 0.85) },
			{ H * 0.96,             NeckR        },
			{ H,                    MouthR       },
		};

		TArray<FTransform> Xf;
		for (const FStation& S : Stations)
		{
			Xf.Add(FTransform(FQuat::Identity,
				FVector(CX, CY, Z0 + S.Z),
				FVector(FMath::Max(S.R, 0.5), FMath::Max(S.R, 0.5), 1.0)));
		}

		// Tagged as tile: a 魚缸 is fired clay and so is a 瓦, and reading as one family is what the object looks like against a roof.
		const int32 JarFirstTri = Mesh.MaxTriangleID();
		AppendSweptProfile(Mesh, Circle, Xf);
		SetMaterialIDForTrianglesFrom(Mesh, JarFirstTri, MatSlot_Roof);
	}
}
