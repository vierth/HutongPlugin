#include "Generation/FlowerBedGenerator.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	void BuildFlowerBed(FDynamicMesh3& Mesh, const FHutongFlowerBedParams& P)
	{
		using namespace HutongMeshUtils;

		const double W = FMath::Max(P.SizeX, 1.0);
		const double D = FMath::Max(P.SizeY, 1.0);
		const double H = FMath::Max(P.KerbHeight, 1.0);

		// The kerb cannot eat the bed.
		const double K = FMath::Clamp(FMath::Max(P.KerbWidth, 1.0),
			1.0, FMath::Max(0.5 * FMath::Min(W, D) - 5.0, 1.0));

		// Kerb: four tiling runs.
		{
			FSlotScope KerbTag(Mesh, P.bStoneKerb ? MatSlot_Stone : MatSlot_BaseCourse);
			AppendBox(Mesh, FVector3d(0.0, 0.0, 0.0),     FVector3d(W, K, H));
			AppendBox(Mesh, FVector3d(0.0, D - K, 0.0),   FVector3d(W, D, H));
			AppendBox(Mesh, FVector3d(0.0, K, 0.0),       FVector3d(K, D - K, H));
			AppendBox(Mesh, FVector3d(W - K, K, 0.0),     FVector3d(W, D - K, H));
		}

		// Earth, set down from the kerb top.
		const double Soil = FMath::Max(H - FMath::Max(P.SoilDrop, 0.0), 1.0);
		if (W - 2.0 * K > 1.0 && D - 2.0 * K > 1.0)
		{
			FSlotScope SoilTag(Mesh, MatSlot_Earth);
			AppendBox(Mesh, FVector3d(K, K, 0.0), FVector3d(W - K, D - K, Soil));
			SoilTag.Close();
		}
	}
}
