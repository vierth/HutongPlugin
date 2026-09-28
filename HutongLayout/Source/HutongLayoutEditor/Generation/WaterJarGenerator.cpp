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

		// Footprint centred on the drag.
		const double Span = P.GetFootprint();
		const double CX = 0.5 * Span;
		const double CY = 0.5 * Span;

		// 1) 缸座 first; the jar's foot sits on it.
		double Z0 = 0.0;
		if (P.bHasBase && P.BaseHeight > 0.0)
		{
			FSlotScope BaseTag(Mesh, MatSlot_Stone);
			const double BH = FMath::Max(P.BaseHeight, 1.0);
			const double Half = FMath::Max(FootR + FMath::Max(P.BaseMargin, 0.0), 1.0);
			AppendBox(Mesh,
				FVector3d(CX - Half, CY - Half, 0.0),
				FVector3d(CX + Half, CY + Half, BH));
			BaseTag.Close();
			Z0 = BH;
		}

		// Jar: a unit circle scaled per station up the axis, as AppendColumn does 收分.
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

		// Tagged tile: fired clay like a 瓦, reads as one family against a roof.
		FSlotScope JarTag(Mesh, MatSlot_Roof);
		AppendSweptProfile(Mesh, Circle, Xf);
		JarTag.Close();
	}
}
