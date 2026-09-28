#include "Generation/PathGenerator.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	void BuildPath(FDynamicMesh3& Mesh, const FHutongPathParams& P)
	{
		using namespace HutongMeshUtils;

		const double L = FMath::Max(P.Length, 1.0);
		const double W = FMath::Max(P.Width, 1.0);
		const double Rise = FMath::Max(P.Rise, 0.5);
		const double Kerb = P.GetKerbWidth();

		// Across the rect: kerb, paving, kerb.
		const double PaveY0 = Kerb;
		const double PaveY1 = Kerb + W;

		FSlotScope StoneTag(Mesh, MatSlot_Stone);

		// 牙子石 each side, a lip proud of the paving.
		if (Kerb > 0.0)
		{
			const double Lip = FMath::Max(P.KerbLip, 0.0);
			AppendBox(Mesh, FVector3d(0.0, 0.0, 0.0), FVector3d(L, PaveY0, Rise + Lip));
			AppendBox(Mesh, FVector3d(0.0, PaveY1, 0.0), FVector3d(L, PaveY1 + Kerb, Rise + Lip));
		}

		// Paving.
		const double Spacing = FMath::Max(P.CourseSpacing, 0.0);
		const double Joint = FMath::Clamp(P.JointWidth, 0.0, FMath::Max(Spacing * 0.4, 0.0));

		if (Spacing <= 0.0 || Joint <= 0.0 || L < 2.0 * Spacing)
		{
			AppendBox(Mesh, FVector3d(0.0, PaveY0, 0.0), FVector3d(L, PaveY1, Rise));
		}
		else
		{
			// Course count falls out of the length.
			const int32 Count = FMath::Max(FMath::RoundToInt32(L / Spacing), 1);
			for (int32 i = 0; i < Count; ++i)
			{
				const double A = L * i / double(Count);
				const double B = L * (i + 1) / double(Count);
				// Joint taken off each course's far end, except the last.
				const double End = (i == Count - 1) ? B : (B - 0.5 * Joint);
				const double Start = (i == 0) ? A : (A + 0.5 * Joint);
				if (End <= Start) continue;

				// A hair below the kerb's retained face.
				AppendBox(Mesh, FVector3d(Start, PaveY0, 0.0), FVector3d(End, PaveY1, Rise));
			}
		}

		StoneTag.Close();
	}
}
