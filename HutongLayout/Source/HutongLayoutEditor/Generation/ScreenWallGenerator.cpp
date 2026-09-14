#include "Generation/ScreenWallGenerator.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongShell.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	void BuildScreenWall(FDynamicMesh3& Mesh, const FHutongScreenWallParams& P)
	{
		using namespace HutongMeshUtils;

		const double L = FMath::Max(P.Length, 1.0);
		const double T = FMath::Max(P.Thickness, 1.0);
		const double Eave = P.GetEaveHeight();

		// The screen is built centred on Y = 0 in its own frame and translated to the rect's min corner by the caller's footprint depth.
		const double PlinthP = FMath::Max(P.PlinthProjection, 0.0);
		const double CY = PlinthP + 0.5 * T;                 // wall centre line in the rect
		const double WY0 = CY - 0.5 * T;
		const double WY1 = CY + 0.5 * T;

		const double PlinthH = FMath::Clamp(P.PlinthHeight, 0.0, Eave * 0.4);
		const double BaseH = FMath::Clamp(P.GetBaseCourseHeight(), 0.0, (Eave - PlinthH) * 0.7);
		const double BaseP = FMath::Max(P.BaseCourseProjection, 0.0);

		// 須彌座, three courses with the middle one recessed.
		const int32 StoneFirstTri = Mesh.MaxTriangleID();
		if (PlinthH > 0.0)
		{
			const double Course = PlinthH / 3.0;
			const double WaistP = 0.35 * PlinthP;
			AppendBox(Mesh,
				FVector3d(-PlinthP,     CY - 0.5 * T - PlinthP, 0.0),
				FVector3d(L + PlinthP,  CY + 0.5 * T + PlinthP, Course));
			AppendBox(Mesh,
				FVector3d(-WaistP,      CY - 0.5 * T - WaistP,  Course),
				FVector3d(L + WaistP,   CY + 0.5 * T + WaistP,  2.0 * Course));
			AppendBox(Mesh,
				FVector3d(-PlinthP,     CY - 0.5 * T - PlinthP, 2.0 * Course),
				FVector3d(L + PlinthP,  CY + 0.5 * T + PlinthP, PlinthH));
		}
		SetMaterialIDForTrianglesFrom(Mesh, StoneFirstTri, MatSlot_Stone);

		// 2) 下鹼 and the wall body above it.
		const double BodyBottom = PlinthH + BaseH;
		if (BaseH > 0.0 && BaseP > 0.0)
		{
			AppendBox(Mesh,
				FVector3d(0.0, WY0 - BaseP, PlinthH),
				FVector3d(L,   WY1 + BaseP, BodyBottom));
			AppendBox(Mesh, FVector3d(0.0, WY0, BodyBottom), FVector3d(L, WY1, Eave));
		}
		else
		{
			AppendBox(Mesh, FVector3d(0.0, WY0, PlinthH), FVector3d(L, WY1, Eave));
		}

		// 影壁心: a frame of four bars standing proud of the wall rather than a slab with a hole cut in it.
		if (P.bHasPanel)
		{
			const double FieldZ0 = BodyBottom;
			const double FieldZ1 = Eave;
			const double FieldH = FieldZ1 - FieldZ0;
			const double Border = FMath::Clamp(P.BorderFraction, 0.02, 0.45)
				* FMath::Min(L, FMath::Max(FieldH, 1.0));
			const double Proj = FMath::Max(P.BorderProjection, 0.0);

			// Leave the panel out entirely.
			if (Proj > 0.0 && L > 4.0 * Border && FieldH > 4.0 * Border)
			{
				const double X0 = 1.5 * Border, X1 = L - 1.5 * Border;
				const double Z0 = FieldZ0 + 1.2 * Border, Z1 = FieldZ1 - 1.2 * Border;

				auto AppendFrame = [&](double FaceY, double Dir)
				{
					const double A = FaceY;
					const double B = FaceY + Dir * Proj;
					const double Lo = FMath::Min(A, B), Hi = FMath::Max(A, B);
					// Top and bottom bars run the full width; the side bars sit between them.
					AppendBox(Mesh, FVector3d(X0, Lo, Z0), FVector3d(X1, Hi, Z0 + Border));
					AppendBox(Mesh, FVector3d(X0, Lo, Z1 - Border), FVector3d(X1, Hi, Z1));
					AppendBox(Mesh, FVector3d(X0, Lo, Z0 + Border),
						FVector3d(X0 + Border, Hi, Z1 - Border));
					AppendBox(Mesh, FVector3d(X1 - Border, Lo, Z0 + Border),
						FVector3d(X1, Hi, Z1 - Border));
					// The field inside the frame is 白灰, a skim standing a hair proud of the brick
					// so the two faces are not one plane, tagged in its own range since more follows.
					const double SkimLo = Dir < 0.0 ? FaceY - 0.6 : FaceY - 0.2;
					const double SkimHi = Dir < 0.0 ? FaceY + 0.2 : FaceY + 0.6;
					const int32 First = Mesh.MaxTriangleID();
					AppendBox(Mesh, FVector3d(X0 + Border, SkimLo, Z0 + Border), FVector3d(X1 - Border, SkimHi, Z1 - Border));
					SetMaterialIDForTriangleRange(Mesh, First, Mesh.MaxTriangleID(), MatSlot_Plaster);
				};

				AppendFrame(WY0, -1.0);
				AppendFrame(WY1,  1.0);
			}
		}

		// 4) Roof.
		Shell::FRoofParams Roof;
		Roof.FrontOverhang = FMath::Max(P.RoofOverhang, 0.0);
		Roof.RearOverhang = FMath::Max(P.RoofOverhang, 0.0);
		Roof.GableOverhang = FMath::Max(P.GableOverhang, 0.0);
		Roof.Rise = P.GetRoofRise();
		// 三檁: a screen is one 步架 deep and its roof is a single slope each side.
		Roof.Section = Jiajia::MakeSection(
			EHutongPurlins::Three, 0.5 * T, Roof.FrontOverhang, P.RoofApexRoll);
		Roof.bHasRidgeCourse = P.bHasRidgeCourse;
		Roof.RidgeCourseHeight = P.RidgeCourseHeight;
		Roof.RidgeCourseWidth = P.RidgeCourseWidth;
		Roof.RidgeEndKick = P.RidgeEndKick;
		Roof.SlopeSegments = 8;
		Roof.FasciaDepth = P.EaveFasciaDepth;
		Roof.FasciaWidth = P.EaveFasciaWidth;
		Roof.RafterSection = P.RafterEndSection;
		Roof.RafterSpacing = P.RafterEndSpacing;

		// AppendGableRoof builds for walls spanning Y = 0..T.
		const int32 RoofFirstVert = Mesh.MaxVertexID();
		Shell::AppendGableRoof(Mesh, L, T, Eave, Roof);
		TransformVerticesFrom(Mesh, RoofFirstVert,
			FTransform(FQuat::Identity, FVector(0.0, WY0, 0.0)));
	}
}
