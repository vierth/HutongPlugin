#include "Generation/StoreyGenerator.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongShell.h"
#include "Generation/HutongShopBay.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	namespace
	{
		// 腰檐: a tiled band falling from the wall to its outer edge, built as the 散水 is — a
		// prism with its ridge pushed hard against the wall it dies into, so the vertical face is
		// buried rather than sharing a plane with the masonry.
		void AppendSkirtRoof(FDynamicMesh3& Mesh, double X0, double X1, double FaceY,
			double TopZ, double Projection, double Drop)
		{
			using namespace HutongMeshUtils;
			if (X1 - X0 < 1.0 || Projection < 1.0 || Drop < 0.5) return;

			// Overshoot into the wall so the back of the band is inside the masonry.
			const double Bury = 6.0;
			AppendTriPrism(Mesh,
				FVector3d(X0, FaceY - Projection, TopZ - Drop),
				X1 - X0, Projection + Bury, Drop, EAxis2D::X, /*ApexFraction*/ 1.0);
		}

		// The railing: posts on the bay boundaries, a top and bottom rail, and balusters between.
		void AppendRailing(FDynamicMesh3& Mesh, const TFunctionRef<double(int32)>& BoundaryX,
			int32 BayCount, double OuterY, double DeckTopZ, double Height,
			double Section, double Spacing)
		{
			using namespace HutongMeshUtils;
			if (Height < 20.0 || Section < 0.5) return;

			const double Y0 = OuterY - Section;
			const double Y1 = OuterY;
			const double TopZ = DeckTopZ + Height;

			// Top rail and the lower rail the balusters stand on, run the whole front.
			const double RunX0 = BoundaryX(0);
			const double RunX1 = BoundaryX(BayCount);
			AppendBox(Mesh,
				FVector3d(RunX0, Y0 - 0.4 * Section, TopZ - 1.4 * Section),
				FVector3d(RunX1, Y1 + 0.4 * Section, TopZ));
			AppendBox(Mesh,
				FVector3d(RunX0, Y0, DeckTopZ + 0.30 * Height),
				FVector3d(RunX1, Y1, DeckTopZ + 0.30 * Height + Section));

			// Posts at the bay boundaries, standing proud of the rails both faces.
			for (int32 i = 0; i <= BayCount; ++i)
			{
				const double CX = BoundaryX(i);
				AppendBox(Mesh,
					FVector3d(CX - Section, Y0 - 0.6 * Section, DeckTopZ),
					FVector3d(CX + Section, Y1 + 0.6 * Section, TopZ + 0.9 * Section));
			}

			// Balusters between the rails, bay by bay so they never cross a post.
			const double BalTop = TopZ - 1.4 * Section;
			const double BalBottom = DeckTopZ + 0.30 * Height + Section;
			if (BalTop - BalBottom < 4.0) return;

			for (int32 i = 0; i < BayCount; ++i)
			{
				const double X0 = BoundaryX(i) + Section;
				const double X1 = BoundaryX(i + 1) - Section;
				const double Span = X1 - X0;
				if (Span < 3.0 * Spacing) continue;

				const int32 Count = FMath::Max(FMath::RoundToInt32(Span / FMath::Max(Spacing, 4.0)) - 1, 1);
				for (int32 b = 1; b <= Count; ++b)
				{
					const double CX = X0 + Span * b / double(Count + 1);
					AppendBox(Mesh,
						FVector3d(CX - 0.5 * Section, Y0 + 0.15 * Section, BalBottom),
						FVector3d(CX + 0.5 * Section, Y1 - 0.15 * Section, BalTop));
				}
			}
		}
	}

	void BuildStorey(FDynamicMesh3& Mesh, const FHutongStoreyParams& P)
	{
		using namespace HutongMeshUtils;

		const double W = FMath::Max(P.Width, 1.0);
		const double D = FMath::Max(P.Depth, 1.0);
		const double T = FMath::Clamp(P.WallThickness, 1.0, FMath::Min(W, D) * 0.2);
		const double Floor = FMath::Clamp(P.FloorHeight, 0.0, 120.0);

		// The two storeys, and the line between them the whole type is about.
		const double StoreyLine = P.GetStoreyLineHeight();
		const double Eave = P.GetEaveHeight();
		const double OpenTop = FMath::Min(P.GetOpeningTopHeight(), StoreyLine - 20.0);

		const double ColR = P.GetColumnRadiusFor(W, D);
		const int32 N = (P.BayCountOverride > 0)
			? P.BayCountOverride
			: ComputeBayCount(W, P.MinBayWidth, P.MaxBayWidth);
		auto BayBoundaryX = [&](int32 i) { return P.GetBayBoundary(i, N, W, ColR); };

		const double BaseH = FMath::Clamp(P.GetBaseCourseHeight(), 0.0, (StoreyLine - Floor) * 0.6);
		const double BaseP = FMath::Max(P.BaseCourseProjection, 0.0);
		const double WallBottom = (BaseH > 0.0 && BaseP > 0.0) ? Floor + BaseH : Floor;

		// 1) 臺基.
		const double PlatO = FMath::Max(P.PlatformOverhang, ColR);
		Shell::AppendPlatform(Mesh, W, D, Floor, PlatO, /*SideProjection*/ 0.0, 0, 30.0, 0.0, 0.0);

		// 2) 下鹼 and the three closed walls, which run the full two storeys.
		{
			const int32 BaseFirstTri = Mesh.MaxTriangleID();
			Shell::AppendBaseCourseU(Mesh, W, D, T, Floor, BaseH, BaseP,
				/*bIncludeRear*/ true, /*bToGround*/ true);
			SetMaterialIDForTrianglesFrom(Mesh, BaseFirstTri, MatSlot_BaseCourse);
		}

		AppendBox(Mesh, FVector3d(T, D - T, WallBottom), FVector3d(W - T, D, Eave));
		AppendBox(Mesh, FVector3d(0.0, 0.0, WallBottom), FVector3d(T, D, Eave));
		AppendBox(Mesh, FVector3d(W - T, 0.0, WallBottom), FVector3d(W, D, Eave));

		// 3) The shop below: columns the height of the ground storey, a header over the opening,
		// and the same 排板門 and 櫃檯 a 鋪面房 presents.
		{
			const int32 WoodFirstTri = Mesh.MaxTriangleID();
			const double ColTopR = Frame::TaperedTopRadius(ColR, StoreyLine - Floor, P.ColumnTaperRatio);
			Frame::AppendColumnRow(Mesh, BayBoundaryX, N, 0.0, ColR, ColTopR, Floor, StoreyLine, 1.0, 16);
			if (StoreyLine > OpenTop)
			{
				AppendBox(Mesh, FVector3d(T, 0.0, OpenTop), FVector3d(W - T, T, StoreyLine));
			}

			ShopBay::FFront Front;
			// A hair proud of the wall plane: the boards' run starts inside the side walls, and on
			// the wall's own front plane their faces fought it for the strip at each corner.
			Front.FaceY = -0.5;
			Front.BoardWidth = P.BoardWidth;
			Front.BoardThickness = P.BoardThickness;
			Front.SillZ = Floor;
			Front.HeadZ = OpenTop;
			Front.OpenBayCount = P.OpenBayCount;
			Front.bHasCounter = P.bHasCounter;
			Front.CounterHeight = P.CounterHeight;
			Front.CounterDepth = P.CounterDepth;
			ShopBay::AppendFront(Mesh, BayBoundaryX, N, ColR, Front);

			SetMaterialIDForTrianglesFrom(Mesh, WoodFirstTri, MatSlot_Wood);
		}

		// 4) 腰檐 across the front, and its 椽頭 under it. Tiled, because it is a roof: it is what
		// says the building has two storeys rather than one tall one.
		const double SkirtProj = FMath::Max(P.SkirtProjection, 10.0);
		const double SkirtDrop = FMath::Max(P.SkirtDrop, 4.0);
		if (P.bHasSkirtRoof)
		{
			const int32 SkirtFirstTri = Mesh.MaxTriangleID();
			AppendSkirtRoof(Mesh, -0.5 * T, W + 0.5 * T, 0.0, StoreyLine, SkirtProj, SkirtDrop);
			SetMaterialIDForTrianglesFrom(Mesh, SkirtFirstTri, MatSlot_Roof);

			if (P.bHasSkirtRafters && P.RafterEndSection > 0.0)
			{
				const int32 RafterFirstTri = Mesh.MaxTriangleID();
				const double Sec = FMath::Max(P.RafterEndSection, 1.0);
				// Behind the band's outer edge and under it, the way an eave's own row sits back
				// behind the fascia: flush is a shared plane.
				Shell::AppendRafterEnds(Mesh, T, W - T,
					-SkirtProj + 0.35 * Sec, -0.25 * SkirtProj,
					StoreyLine - SkirtDrop, Sec, FMath::Max(P.RafterEndSpacing, 4.0));
				SetMaterialIDForTrianglesFrom(Mesh, RafterFirstTri, MatSlot_Wood);
			}
		}

		// 5) The gallery: a deck over the skirt and a railing at its edge. The deck is held inside
		// the skirt's own projection, so the balcony stands on the band rather than out past it.
		const double GalleryDepth = P.bHasGallery
			? FMath::Clamp(P.GalleryDepth, 20.0, P.bHasSkirtRoof ? SkirtProj : 140.0)
			: 0.0;
		const double DeckT = FMath::Max(P.DeckThickness, 3.0);
		if (P.bHasGallery)
		{
			const int32 GalleryFirstTri = Mesh.MaxTriangleID();
			AppendBox(Mesh,
				FVector3d(-0.5 * T, -GalleryDepth, StoreyLine),
				FVector3d(W + 0.5 * T, 0.5 * T,    StoreyLine + DeckT));

			AppendRailing(Mesh, BayBoundaryX, N, -GalleryDepth + FMath::Max(P.RailSection, 1.0),
				StoreyLine + DeckT, FMath::Max(P.RailHeight, 40.0),
				FMath::Max(P.RailSection, 1.0), FMath::Max(P.BalusterSpacing, 6.0));
			SetMaterialIDForTrianglesFrom(Mesh, GalleryFirstTri, MatSlot_Wood);
		}

		// 6) The upper front: 檻牆 of boarding under the sill, windows over it, header to the eave.
		const double DeckZ = StoreyLine + (P.bHasGallery ? DeckT : 0.0);
		const double SillZ = DeckZ + FMath::Clamp(P.UpperSillHeight, 0.0, 90.0);
		const double HeadZ = Eave - FMath::Clamp(P.UpperHeaderDrop, 5.0, 90.0);
		{
			const int32 UpperWoodFirstTri = Mesh.MaxTriangleID();
			const double ColTopR = Frame::TaperedTopRadius(ColR, Eave - DeckZ, P.ColumnTaperRatio);
			Frame::AppendColumnRow(Mesh, BayBoundaryX, N, 0.0, ColR, ColTopR, DeckZ, Eave, 1.0, 16);

			// 額枋 under the eave, and the header band between it and the window heads.
			Frame::AppendArchitrave(Mesh, BayBoundaryX(0), BayBoundaryX(N), 0.0, 0.8 * T,
				Eave - 1.1 * T, Eave);
			if (Eave - 1.1 * T > HeadZ)
			{
				AppendBox(Mesh, FVector3d(T, 0.0, HeadZ), FVector3d(W - T, 0.7 * T, Eave - 1.1 * T));
			}
			// Boarding under the sill, which is what the railing stands in front of.
			if (SillZ > DeckZ + 2.0)
			{
				AppendBox(Mesh, FVector3d(T, 0.0, DeckZ), FVector3d(W - T, 0.7 * T, SillZ));
			}

			// 抱框 at every bay boundary, or the window band is a hole through the front at each column.
			for (int32 i = 1; i < N; ++i)
			{
				const double CX = BayBoundaryX(i);
				AppendBox(Mesh,
					FVector3d(CX - ColR, 0.0,      SillZ),
					FVector3d(CX + ColR, 0.7 * T,  HeadZ));
			}
			SetMaterialIDForTrianglesFrom(Mesh, UpperWoodFirstTri, MatSlot_Wood);
		}

		// 7) The windows themselves: 窗紙 behind 欞條, per bay, tagged per bay.
		if (HeadZ - SillZ > 10.0)
		{
			const double BarT = FMath::Clamp(P.WindowLatticeThickness, 1.0, 0.5 * T);
			for (int32 i = 0; i < N; ++i)
			{
				const double X0 = BayBoundaryX(i) + ColR;
				const double X1 = BayBoundaryX(i + 1) - ColR;
				if (X1 - X0 < 20.0) continue;

				if (P.bHasWindowPaper)
				{
					const int32 PaperFirstTri = Mesh.MaxTriangleID();
					AppendBox(Mesh,
						FVector3d(X0, 1.4 * BarT,        SillZ),
						FVector3d(X1, 1.4 * BarT + 0.25 * BarT, HeadZ));
					SetMaterialIDForTrianglesFrom(Mesh, PaperFirstTri, MatSlot_Paper);
				}

				if (!P.bHasWindowLattice || P.WindowLatticeBars <= 0) continue;

				const int32 LatticeFirstTri = Mesh.MaxTriangleID();
				const int32 Bars = FMath::Clamp(P.WindowLatticeBars, 1, 16);
				for (int32 j = 1; j <= Bars; ++j)
				{
					const double CX = X0 + (X1 - X0) * j / double(Bars + 1);
					AppendBox(Mesh,
						FVector3d(CX - 0.5 * BarT, 0.0,   SillZ),
						FVector3d(CX + 0.5 * BarT, BarT,  HeadZ));
				}
				// The crossing bars sit deeper than the uprights, as every window here does.
				const double MidZ = 0.5 * (SillZ + HeadZ);
				AppendBox(Mesh,
					FVector3d(X0, 0.4 * BarT, MidZ - 0.5 * BarT),
					FVector3d(X1, 1.4 * BarT, MidZ + 0.5 * BarT));
				SetMaterialIDForTrianglesFrom(Mesh, LatticeFirstTri, MatSlot_Lattice);
			}
		}

		// 8) 匾額, hung on the gallery rail where the street can read it.
		if (P.bHasSignboard && N > 0 && P.bHasGallery)
		{
			const int32 SignFirstTri = Mesh.MaxTriangleID();
			int32 OpenLo = 0, OpenHi = -1;
			ShopBay::OpenBayRange(N, P.OpenBayCount, OpenLo, OpenHi);
			const int32 SignBay = (P.OpenBayCount > 0)
				? FMath::Clamp((OpenLo + OpenHi) / 2, 0, N - 1) : (N / 2);

			const double SX0 = BayBoundaryX(SignBay);
			const double SX1 = BayBoundaryX(SignBay + 1);
			const double SH = FMath::Clamp(P.SignboardHeight, 10.0, FMath::Max(P.RailHeight, 40.0));
			const double Inset = 0.12 * (SX1 - SX0);
			const double FaceY = -GalleryDepth + FMath::Max(P.RailSection, 1.0);
			if (SX1 - SX0 > 3.0 * Inset)
			{
				AppendBox(Mesh,
					FVector3d(SX0 + Inset, FaceY - 4.0, StoreyLine + DeckT + 0.10 * P.RailHeight),
					FVector3d(SX1 - Inset, FaceY + 1.0, StoreyLine + DeckT + 0.10 * P.RailHeight + SH));
				SetMaterialIDForTrianglesFrom(Mesh, SignFirstTri, MatSlot_Wood);
			}
		}

		// 9) Roof — 硬山, the same as everything else on the lane. Nothing about a commercial 樓
		// entitles it to more, and it stands in a terrace.
		Shell::FRoofParams Roof;
		Roof.FrontOverhang = FMath::Max(P.RoofOverhang, 0.0);
		Roof.RearOverhang = P.GetRearRoofOverhang();
		Roof.RearSlopeTrim = (P.RearEave == EHutongRearEave::Lane)
			? FMath::Max(Roof.FrontOverhang - Roof.RearOverhang, 0.0)
			: 0.0;
		Roof.Rise = P.GetRoofRise();
		Roof.Section = Jiajia::MakeSection(
			EHutongPurlins::Five, 0.5 * D, Roof.FrontOverhang, P.RoofApexRoll);
		Roof.Tile = P.RoofTile;
		Roof.TileRowSpacing = P.TileRowSpacing;
		Roof.bHasRidgeCourse = P.bHasRidgeCourse;
		Roof.RidgeCourseHeight = P.RidgeCourseHeight;
		Roof.RidgeCourseWidth = P.RidgeCourseWidth;
		Roof.RidgeEndKick = P.RidgeEndKick;
		Roof.SlopeSegments = 8;
		Roof.FasciaDepth = P.EaveFasciaDepth;
		Roof.FasciaWidth = P.EaveFasciaWidth;
		Roof.RafterSection = P.RafterEndSection;
		Roof.RafterSpacing = P.RafterEndSpacing;

		// 封護檐 on the lane variant, exactly as on a house or a shop.
		const double RearLift = Shell::RearEaveLift(Roof, D);
		if (RearLift > 0.0)
		{
			AppendBox(Mesh,
				FVector3d(-2.0,    D - T, Eave),
				FVector3d(W + 2.0, D + 2.0, Eave + RearLift));
		}

		Shell::AppendGableRoof(Mesh, W, D, Eave, Roof);
	}
}
