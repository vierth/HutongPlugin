#include "Generation/PaifangGenerator.h"
#include "Generation/HutongBays.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	void BuildPaifang(FDynamicMesh3& Mesh, const FHutongPaifangParams& P)
	{
		using namespace HutongMeshUtils;

		const double L = FMath::Max(P.Length, 1.0);
		const double Dep = FMath::Max(P.Depth, 1.0);
		const double H = P.GetHeight();
		const int32 N = P.GetBayCount();
		const double ColR = P.GetColumnRadiusFor(Dep);

		// The frame is stone on a 牌坊 and painted timber on a 牌樓.
		const int32 BodySlot = P.bHasRoofs ? MatSlot_Paint : MatSlot_Stone;
		const int32 BodyFirstTri = Mesh.MaxTriangleID();

		// 明間 is wider here for the same reason it is on a facade; the params own the arithmetic
		// so the plan's divisions cannot drift from the built ones.
		const int32 CentralBay = N / 2;
		auto BoundaryX = [&](int32 i) { return P.GetBayBoundary(i, N, L, ColR); };

		const double CenterY = 0.5 * Dep;
		const double ArchT = FMath::Max(P.ArchitraveThickness, 5.0);
		const double ArchD = FMath::Max(P.ArchitraveDepth, 8.0);

		// A side bay's lintel sits lower than the central one.
		const double Drop = FMath::Clamp(P.SideBayDrop, 0.0, 0.6) * H;
		auto BayTopZ = [&](int32 Bay) { return (Bay == CentralBay) ? H : H - Drop; };

		// Columns, each from the ground to its own height. On a 衝天式 they carry on past the roofs.
		const double ColumnExtra = P.bColumnsThroughRoof ? (1.6 * ArchD + P.GetRoofRise()) : 0.0;
		for (int32 i = 0; i <= N; ++i)
		{
			// A column's height follows the taller of the bays it separates.
			double Top = BayTopZ(FMath::Clamp(i, 0, N - 1));
			if (i > 0) Top = FMath::Max(Top, BayTopZ(FMath::Clamp(i - 1, 0, N - 1)));
			Top += ArchD + ColumnExtra;

			// Through the shared helper so a paifang post takes the same 收分 as every other column here.
			Frame::AppendColumn(Mesh, BoundaryX(i), CenterY, ColR,
				Frame::TaperedTopRadius(ColR, Top, P.ColumnTaperRatio),
				0.0, Top, 0.0, 12);
		}

		// 夾杆石 clamping each column foot. Always stone, even on a 牌樓 — the timber stands on it.
		const double PlinthH = FMath::Max(P.PlinthHeight, 0.0);
		const double Spread = FMath::Max(P.PlinthSpread, 0.0);
		if (PlinthH > 0.0)
		{
			const int32 StoneFirstTri = Mesh.MaxTriangleID();
			for (int32 i = 0; i <= N; ++i)
			{
				const double CX = BoundaryX(i);
				const double R = ColR + Spread;
				AppendBox(Mesh,
					FVector3d(CX - R, CenterY - R, 0.0),
					FVector3d(CX + R, CenterY + R, PlinthH));
			}
			SetMaterialIDForTrianglesFrom(Mesh, StoneFirstTri, MatSlot_Stone);
		}

		// Lintels, per bay rather than one run: the side bays sit lower, so a single beam would have to be stepped.
		for (int32 Bay = 0; Bay < N; ++Bay)
		{
			const double X0 = BoundaryX(Bay);
			const double X1 = BoundaryX(Bay + 1);
			if (X1 - X0 < 2.0 * ColR) continue;

			const double Top = BayTopZ(Bay);

			// 額枋.
			AppendBox(Mesh,
				FVector3d(X0, CenterY - 0.5 * ArchT, Top),
				FVector3d(X1, CenterY + 0.5 * ArchT, Top + ArchD));

			if (!P.bHasLowerArchitrave) continue;

			// 小額枋 below it, with the gap between them for the 花板 / 匾額.
			const double Gap = FMath::Clamp(P.PanelGap, 5.0, Top * 0.5);
			const double LowerD = 0.6 * ArchD;
			const double LowerTop = Top - Gap;
			AppendBox(Mesh,
				FVector3d(X0, CenterY - 0.5 * ArchT, LowerTop - LowerD),
				FVector3d(X1, CenterY + 0.5 * ArchT, LowerTop));

			// 匾額: the inscribed plaque, only in the central bay and standing proud of the beams on both faces so it reads as applied.
			if (P.bHasPlaque && Bay == CentralBay)
			{
				const double PlaqueHalf = 0.5 * ArchT + 0.12 * ArchT;
				const double Inset = 0.18 * (X1 - X0);
				AppendBox(Mesh,
					FVector3d(X0 + Inset, CenterY - PlaqueHalf, LowerTop + 0.12 * Gap),
					FVector3d(X1 - Inset, CenterY + PlaqueHalf, Top - 0.12 * Gap));
			}
		}

		SetMaterialIDForTrianglesFrom(Mesh, BodyFirstTri, BodySlot);

		if (!P.bHasRoofs)
		{
			return;
		}

		// 樓, one over each bay.
		const int32 RoofFirstTri = Mesh.MaxTriangleID();
		const double O = FMath::Max(P.RoofOverhang, 0.0);
		const double Rise = P.GetRoofRise();
		const double FasciaDrop = FMath::Max(P.EaveFasciaDepth, 0.0);

		for (int32 Bay = 0; Bay < N; ++Bay)
		{
			double X0 = BoundaryX(Bay);
			double X1 = BoundaryX(Bay + 1);
			if (X1 - X0 < 2.0 * ColR) continue;

			const double EaveZ = BayTopZ(Bay) + ArchD;

			// Side roofs run into the central bay's column so no two eaves meet at a plane.
			if (Bay < CentralBay)  X1 += ColR;
			if (Bay > CentralBay)  X0 -= ColR;
			// The central roof always overhangs its columns, so its ends are clear anyway.
			if (Bay == CentralBay) { X0 -= ColR; X1 += ColR; }

			// A 樓 is 廡殿, not a prism: it hips on all four sides and its corners sweep up.
			FHipRoofSpec Hip;
			Hip.Width = X1 - X0;
			Hip.Depth = Dep + 2.0 * O;
			// Equal pitch on all four sides puts the hips at 45° in plan, which leaves exactly this much ridge.
			Hip.RidgeLength = FMath::Max(Hip.Width - Hip.Depth, 0.0);
			Hip.Rise = Rise;
			// A 樓 is a miniature roof: two steps read as 舉架 at this size.
			Hip.Section = Jiajia::MakeSection(EHutongPurlins::Five, 0.5 * Hip.Depth, 0.0, 0.0);
			// A 牌樓 is an official monument — it is put up by permission — so its 樓 are 筒瓦 without anyone choosing.
			Hip.bEaveCaps = !P.bPlainTileForDetail;
			Hip.TileRowSpacing = P.TileRowSpacing;
			Hip.FlareRise = FMath::Max(P.RoofFlareRise, 0.0);
			Hip.FlareRun = FMath::Max(P.RoofFlareRun, 0.0);
			// Far enough back that the sweep reads as a curve.
			Hip.FlareLength = 0.4 * FMath::Min(Hip.Width, Hip.Depth);
			// 勾頭滴水 is the solid's own lower edge here.
			Hip.FasciaDrop = FasciaDrop;
			Hip.EaveSegments = P.RoofEaveSegments;
			Hip.SlopeSegments = P.RoofSlopeSegments;

			AppendHippedRoof(Mesh, FVector3d(X0, -O, EaveZ), Hip);
		}

		SetMaterialIDForTrianglesFrom(Mesh, RoofFirstTri, MatSlot_Roof);
	}
}
