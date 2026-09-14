#include "Generation/PavilionGenerator.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongShell.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	void BuildPavilion(FDynamicMesh3& Mesh, const FHutongPavilionParams& P)
	{
		using namespace HutongMeshUtils;

		const double W = FMath::Max(P.Width, 1.0);
		const double D = FMath::Max(P.Depth, 1.0);
		const double Eave = P.GetEaveHeight();
		const double ColR = FMath::Min(P.GetColumnRadius(), 0.15 * FMath::Min(W, D));
		const double Floor = FMath::Clamp(P.FloorHeight, 0.0, Eave * 0.3);
		const double PlatO = FMath::Max(P.PlatformOverhang, ColR);

		// The four columns, inset by a radius so they stand on the platform rather than half off it.
		const double X0 = ColR, X1 = W - ColR;
		const double Y0 = ColR, Y1 = D - ColR;

		// 1) 臺基.
		Shell::AppendPlatform(Mesh, W, D, Floor, PlatO, PlatO, 0, 30.0, 0.0, 0.0);

		// --- Woodwork ---
		const int32 WoodFirstTri = Mesh.MaxTriangleID();
		const double ColTopR = Frame::TaperedTopRadius(ColR, P.GetColumnHeight(), P.ColumnTaperRatio);
		const double PT = FMath::Max(2.0 * ColR * 0.85, 7.0);
		const double LintelBottom = Eave - 1.6 * PT;

		// 2) The four columns.
		for (int32 i = 0; i < 4; ++i)
		{
			const double CX = (i == 0 || i == 3) ? X0 : X1;
			const double CY = (i < 2) ? Y0 : Y1;
			Frame::AppendColumn(Mesh, CX, CY, ColR, ColTopR, Floor, Eave, 1.0, 14);
		}

		// 3) 額枋 round all four sides.
		Frame::AppendArchitrave(Mesh, X0, X1, Y0, PT, LintelBottom, Eave);
		Frame::AppendArchitrave(Mesh, X0, X1, Y1, PT, LintelBottom, Eave);
		{
			// Ends overshoot into the corner columns, exactly as the X pair's do.
			const double H = 0.5 * PT;
			AppendBox(Mesh, FVector3d(X0 - H, Y0, LintelBottom), FVector3d(X0 + H, Y1, Eave));
			AppendBox(Mesh, FVector3d(X1 - H, Y0, LintelBottom), FVector3d(X1 + H, Y1, Eave));
		}

		// 倒掛楣子 off each 額枋's soffit and 坐凳楣子 below on the closed sides, on the corridor's rules.
		const double BarT = FMath::Max(P.FriezeBarSection, 1.0);
		const double Drop = FMath::Min(P.FriezeDrop, LintelBottom - Floor - 30.0);

		// Sides in order, each as the span it runs along and the fixed coordinate it sits at.
		const int32 Open = FMath::Clamp(P.OpenSides, 0, 4);
		for (int32 Side = 0; Side < 4; ++Side)
		{
			const bool bAlongX = (Side == 0 || Side == 2);
			const double Fixed = (Side == 0) ? Y0 : (Side == 1) ? X1 : (Side == 2) ? Y1 : X0;
			const double SpanLo = bAlongX ? X0 : Y0;
			const double SpanHi = bAlongX ? X1 : Y1;
			// Outward normal of this side, for pushing the bench away from the middle.
			const double Out = (Side == 0 || Side == 3) ? -1.0 : 1.0;

			// A runs along the side, C across it.
			auto Box = [&](double A0, double A1, double C0, double C1, double Z0, double Z1)
			{
				if (bAlongX) AppendBox(Mesh, FVector3d(A0, C0, Z0), FVector3d(A1, C1, Z1));
				else         AppendBox(Mesh, FVector3d(C0, A0, Z0), FVector3d(C1, A1, Z1));
			};

			if (P.bHasFrieze && P.FriezeBars > 0 && Drop > BarT && SpanHi - SpanLo > 4.0 * BarT)
			{
				Box(SpanLo, SpanHi, Fixed - 0.5 * BarT, Fixed + 0.5 * BarT,
					LintelBottom - BarT, LintelBottom);

				const int32 Count = FMath::Clamp(P.FriezeBars, 1, 24);
				for (int32 j = 1; j <= Count; ++j)
				{
					const double A = SpanLo + (SpanHi - SpanLo) * j / double(Count + 1);
					Box(A - 0.5 * BarT, A + 0.5 * BarT, Fixed - 0.9 * BarT, Fixed + 0.9 * BarT,
						LintelBottom - Drop, LintelBottom - 0.4 * BarT);
				}
			}

			if (P.bHasBench && Side >= Open)
			{
				const double BenchZ = Floor + FMath::Max(P.BenchHeight, 10.0);
				const double BenchD = FMath::Max(P.BenchDepth, 5.0);
				const double Seat = FMath::Max(0.22 * BenchD, 4.0);
				// Runs inward from the column line, so the columns cover its ends.
				const double C0 = FMath::Min(Fixed, Fixed - Out * BenchD);
				const double C1 = FMath::Max(Fixed, Fixed - Out * BenchD);
				Box(SpanLo, SpanHi, C0, C1, BenchZ - Seat, BenchZ);
			}
		}

		SetMaterialIDForTrianglesFrom(Mesh, WoodFirstTri, MatSlot_Wood);

		// 5) The roof.
		const double O = FMath::Max(P.RoofOverhang, 0.0);
		const int32 RoofFirstTri = Mesh.MaxTriangleID();

		const double RoofW = (X1 - X0) + 2.0 * O;
		const double RoofD = (Y1 - Y0) + 2.0 * O;
		const double RoofRise = P.GetRoofRise(D);
		const FVector3d RoofMin(X0 - O, Y0 - O, Eave);
		const HutongGen::FHutongRoofSection Section =
			HutongGen::Jiajia::MakeSection(P.Purlins, 0.5 * RoofD, 0.0, P.RoofApexRoll);

		if (P.RoofType == EHutongRoofType::Xieshan)
		{
			FXieshanRoofSpec Xie;
			Xie.Width = RoofW;
			Xie.Depth = RoofD;
			Xie.Rise = RoofRise;
			Xie.Section = Section;
			Xie.bEaveCaps = HutongGen::RoofTile::HasEaveCaps(P.RoofTile);
			Xie.TileRowSpacing = P.TileRowSpacing;
			Xie.ShouInset = FMath::Max(P.ShouInset, 1.0);
			Xie.FlareRise = FMath::Max(P.RoofFlareRise, 0.0);
			Xie.FlareRun = FMath::Max(P.RoofFlareRun, 0.0);
			Xie.FlareLength = 0.4 * FMath::Min(Xie.Width, Xie.Depth);
			Xie.FasciaDrop = FMath::Max(P.EaveFasciaDepth, 0.0);
			Xie.RidgeWidth = FMath::Max(P.RidgeCourseWidth, 0.0);
			Xie.RidgeHeight = FMath::Max(P.RidgeCourseHeight, 0.0);
			Xie.BargeThickness = FMath::Max(P.BargeBoardThickness, 0.0);
			Xie.BargeDepth = FMath::Max(P.BargeBoardDepth, 0.0);
			Xie.EaveSegments = P.RoofEaveSegments;
			// The gable needs rows on both sides of the 收山 line.
			Xie.SlopeSegments = FMath::Max(P.RoofSlopeSegments, 3);

			AppendXieshanRoof(Mesh, RoofMin, Xie);
		}
		else
		{
			FHipRoofSpec Hip;
			Hip.Width = RoofW;
			Hip.Depth = RoofD;
			// Zero collapses the apex to a point.
			Hip.RidgeLength = (P.RoofType == EHutongRoofType::Wudian)
				? FMath::Max(RoofW - RoofD, 0.0) : 0.0;
			Hip.Rise = RoofRise;
			Hip.Section = Section;
			Hip.bEaveCaps = HutongGen::RoofTile::HasEaveCaps(P.RoofTile);
			Hip.TileRowSpacing = P.TileRowSpacing;
			Hip.FlareRise = FMath::Max(P.RoofFlareRise, 0.0);
			Hip.FlareRun = FMath::Max(P.RoofFlareRun, 0.0);
			Hip.FlareLength = 0.4 * FMath::Min(Hip.Width, Hip.Depth);
			Hip.FasciaDrop = FMath::Max(P.EaveFasciaDepth, 0.0);
			Hip.EaveSegments = P.RoofEaveSegments;
			Hip.SlopeSegments = P.RoofSlopeSegments;

			AppendHippedRoof(Mesh, RoofMin, Hip);
		}

		// 寶頂 over the point the four hips meet, sunk well into the apex.
		if (P.bHasFinial && P.RoofType == EHutongRoofType::Cuanjian)
		{
			const double FW = FMath::Max(P.FinialWidth, 2.0);
			const double FH = FMath::Max(P.FinialHeight, 4.0);
			const double ApexZ = Eave + RoofRise;
			const double CX = RoofMin.X + 0.5 * RoofW;
			const double CY = RoofMin.Y + 0.5 * RoofD;

			// A drum swelling out of the roof and closing to a point.
			static const double Prof[] = { 0.30, 0.62, 0.92, 1.0, 0.86, 0.58, 0.30, 0.10 };
			const int32 Steps = UE_ARRAY_COUNT(Prof);
			const double Base = ApexZ - 0.35 * FH;

			const TArray<FVector2d> Circle = MakeCircleProfile(1.0, 12);
			TArray<FTransform> Stations;
			Stations.Reserve(Steps);
			for (int32 i = 0; i < Steps; ++i)
			{
				const double T = double(i) / double(Steps - 1);
				const double R = 0.5 * FW * Prof[i];
				Stations.Add(FTransform(FQuat::Identity,
					FVector(CX, CY, Base + T * FH), FVector(R, R, 1.0)));
			}
			AppendSweptProfile(Mesh, Circle, Stations);
		}

		SetMaterialIDForTrianglesFrom(Mesh, RoofFirstTri, MatSlot_Roof);
	}
}
