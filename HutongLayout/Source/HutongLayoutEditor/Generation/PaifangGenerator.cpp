#include "Generation/PaifangGenerator.h"
#include "Generation/HutongBays.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongShell.h"

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

		// Stone on a 牌坊; on a 牌樓 the posts are timber and the beams and 匾額 carry the 彩畫.
		const int32 ColumnSlot = P.bHasRoofs ? MatSlot_Wood : MatSlot_Stone;
		const int32 BeamSlot = P.bHasRoofs ? MatSlot_Paint : MatSlot_Stone;
		FSlotScope ColumnTag(Mesh, ColumnSlot);

		// Wider 明間; the params own the arithmetic so plan and build cannot drift.
		const int32 CentralBay = N / 2;
		auto BoundaryX = [&](int32 i) { return P.GetBayBoundary(i, N, L, ColR); };

		const double CenterY = 0.5 * Dep;
		const double ArchT = FMath::Max(P.ArchitraveThickness, 5.0);
		const double ArchD = FMath::Max(P.ArchitraveDepth, 8.0);

		// Side-bay lintels sit lower than the central one.
		const double Drop = FMath::Clamp(P.SideBayDrop, 0.0, 0.6) * H;
		auto BayTopZ = [&](int32 Bay) { return (Bay == CentralBay) ? H : H - Drop; };

		// Columns, ground to own height; on a 衝天式 they continue past the roofs.
		const double ColumnExtra = P.bColumnsThroughRoof ? (1.6 * ArchD + P.GetRoofRise()) : 0.0;
		for (int32 i = 0; i <= N; ++i)
		{
			// Height follows the taller adjacent bay.
			double Top = BayTopZ(FMath::Clamp(i, 0, N - 1));
			if (i > 0) Top = FMath::Max(Top, BayTopZ(FMath::Clamp(i - 1, 0, N - 1)));
			Top += ArchD + ColumnExtra;

			// Shared helper so the post gets the same 收分 as every column.
			Frame::AppendColumn(Mesh, BoundaryX(i), CenterY, ColR,
				Frame::TaperedTopRadius(ColR, Top, P.ColumnTaperRatio),
				0.0, Top, 0.0, 24);
		}

		ColumnTag.Close();

		// 夾杆石 at each column foot; stone even on a 牌樓.
		const double PlinthH = FMath::Max(P.PlinthHeight, 0.0);
		const double Spread = FMath::Max(P.PlinthSpread, 0.0);
		if (PlinthH > 0.0)
		{
			FSlotScope StoneTag(Mesh, MatSlot_Stone);
			for (int32 i = 0; i <= N; ++i)
			{
				const double CX = BoundaryX(i);
				const double R = ColR + Spread;
				AppendBox(Mesh,
					FVector3d(CX - R, CenterY - R, 0.0),
					FVector3d(CX + R, CenterY + R, PlinthH));
			}
			StoneTag.Close();
		}

		// Lintels per bay: side bays sit lower, so one beam would need steps.
		FSlotScope BeamTag(Mesh, BeamSlot);
		for (int32 Bay = 0; Bay < N; ++Bay)
		{
			const double X0 = BoundaryX(Bay);
			const double X1 = BoundaryX(Bay + 1);
			if (X1 - X0 < 2.0 * ColR) continue;

			const double Top = BayTopZ(Bay);

			AppendBox(Mesh,
				FVector3d(X0, CenterY - 0.5 * ArchT, Top),
				FVector3d(X1, CenterY + 0.5 * ArchT, Top + ArchD));

			if (!P.bHasLowerArchitrave) continue;

			// 小額枋 below, gap between for the 花板 / 匾額.
			const double Gap = FMath::Clamp(P.PanelGap, 5.0, Top * 0.5);
			const double LowerD = 0.6 * ArchD;
			const double LowerTop = Top - Gap;
			AppendBox(Mesh,
				FVector3d(X0, CenterY - 0.5 * ArchT, LowerTop - LowerD),
				FVector3d(X1, CenterY + 0.5 * ArchT, LowerTop));

			// 匾額: central bay only, proud of the beams on both faces so it reads as applied.
			if (P.bHasPlaque && Bay == CentralBay)
			{
				const double PlaqueHalf = 0.5 * ArchT + 0.12 * ArchT;
				const double Inset = 0.18 * (X1 - X0);
				AppendBox(Mesh,
					FVector3d(X0 + Inset, CenterY - PlaqueHalf, LowerTop + 0.12 * Gap),
					FVector3d(X1 - Inset, CenterY + PlaqueHalf, Top - 0.12 * Gap));
			}
		}

		BeamTag.Close();

		if (!P.bHasRoofs)
		{
			return;
		}

		// 樓, one per bay.
		FSlotScope RoofTag(Mesh, MatSlot_Roof);
		struct FLouRoof { TArray<FRoofPanel> Panels; };
		TArray<FLouRoof> Lous;
		const double O = FMath::Max(P.RoofOverhang, 0.0);
		const double Rise = P.GetRoofRise();
		const double FasciaDrop = FMath::Max(P.EaveFasciaDepth, 0.0);

		for (int32 Bay = 0; Bay < N; ++Bay)
		{
			double X0 = BoundaryX(Bay);
			double X1 = BoundaryX(Bay + 1);
			if (X1 - X0 < 2.0 * ColR) continue;

			const double EaveZ = BayTopZ(Bay) + ArchD;

			// Side roofs run into the central column so no two eaves meet at a plane.
			if (Bay < CentralBay)  X1 += ColR;
			if (Bay > CentralBay)  X0 -= ColR;
			// The central roof overhangs its columns, so its ends are clear.
			if (Bay == CentralBay) { X0 -= ColR; X1 += ColR; }

			// 樓 is 廡殿: hipped all four sides, corners sweep up.
			FHipRoofSpec Hip;
			Hip.Width = X1 - X0;
			Hip.Depth = Dep + 2.0 * O;
			// Equal pitch puts the hips at 45° in plan, leaving this much ridge.
			Hip.RidgeLength = FMath::Max(Hip.Width - Hip.Depth, 0.0);
			Hip.Rise = Rise;
			// Miniature roof: two steps read as 舉架 at this size.
			Hip.Section = Jiajia::MakeSection(EHutongPurlins::Five, 0.5 * Hip.Depth, 0.0, 0.0);
			// A 牌樓 is an official monument (by permission): 筒瓦 by rank.
			Hip.bEaveCaps = !P.bPlainTileForDetail && !P.bHasTileRuns;
			Hip.TileRowSpacing = P.TileRowSpacing;
			Hip.FlareRise = FMath::Max(P.RoofFlareRise, 0.0);
			Hip.FlareRun = FMath::Max(P.RoofFlareRun, 0.0);
			// Far enough back for the sweep to read as a curve.
			Hip.FlareLength = 0.4 * FMath::Min(Hip.Width, Hip.Depth);
			// 勾頭滴水 is the solid's own lower edge.
			Hip.FasciaDrop = FasciaDrop;
			Hip.EaveOverhang = O;
			Hip.EaveSegments = P.RoofEaveSegments;
			Hip.SlopeSegments = P.RoofSlopeSegments;

			if (P.bHasTileRuns) Shell::DressedRoofSegments(Hip.EaveSegments, Hip.SlopeSegments, FMath::Max(Hip.Width, Hip.Depth),
				FMath::Sqrt(FMath::Square(0.5 * FMath::Min(Hip.Width, Hip.Depth)) + FMath::Square(Hip.Rise)), P.TileRowSpacing);
			FLouRoof& Lou = Lous.AddDefaulted_GetRef();
			AppendHippedRoof(Mesh, FVector3d(X0, -O, EaveZ), Hip, &Lou.Panels);
		}

		RoofTag.Close();

		// Each 樓 dressed like any roof: 壟, 椽頭, 垂脊 and 正脊.
		for (const FLouRoof& Lou : Lous)
		{
			Shell::FRoofDressing Dress;
			Dress.Tile = P.bPlainTileForDetail ? EHutongRoofTile::He : EHutongRoofTile::Tong;
			Dress.TileRowSpacing = P.TileRowSpacing;
			Dress.FasciaDrop = FasciaDrop;
			Dress.bTileRuns = P.bHasTileRuns;
			Dress.RafterSection = P.RafterEndSection;
			Dress.RafterSpacing = P.RafterEndSpacing;
			Dress.EaveOverhang = O;
			Dress.RidgeWidth = 1.2 * FasciaDrop;
			Dress.RidgeHeight = 1.4 * FasciaDrop;
			Shell::AppendRoofDressing(Mesh, Lou.Panels, Dress);
		}
	}
}
