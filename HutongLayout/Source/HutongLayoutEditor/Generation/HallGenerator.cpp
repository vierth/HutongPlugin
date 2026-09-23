#include "Generation/HallGenerator.h"
#include "Generation/HutongDoor.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongShell.h"
#include "Generation/SiheyuanGenerator.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	void BuildHall(FDynamicMesh3& Mesh, const FHutongHallParams& P)
	{
		using namespace HutongMeshUtils;

		const double W = FMath::Max(P.Width, 1.0);
		const double D = FMath::Max(P.Depth, 1.0);
		const double Eave = P.GetEaveHeight();
		const double T = FMath::Clamp(P.WallThickness, 1.0, FMath::Min(W, D) * 0.2);
		const double Floor = FMath::Clamp(P.FloorHeight, 0.0, Eave * 0.35);
		const double ColH = Eave - Floor;
		const double ColR = P.GetColumnRadiusFor(W, D);

		const int32 N = (P.BayCountOverride > 0)
			? P.BayCountOverride
			: ComputeBayCount(W, P.MinBayWidth, P.MaxBayWidth);
		const int32 DoorIdx = N / 2;
		auto BayBoundaryX = [&](int32 i) { return P.GetBayBoundary(i, N, W, ColR); };

		const double BaseH = FMath::Clamp(P.GetBaseCourseHeight(), 0.0, (Eave - Floor) * 0.6);
		const double BaseP = FMath::Max(P.BaseCourseProjection, 0.0);
		const bool bBaseCourse = (BaseH > 0.0 && BaseP > 0.0);
		const double WallBottom = bBaseCourse ? Floor + BaseH : Floor;

		// 1) 臺基, with the 踏跺 under the 明間.
		const double PlatO = FMath::Max(P.PlatformOverhang, ColR);
		const double PlatSide = bBaseCourse ? BaseP : 0.0;
		Shell::AppendPlatform(Mesh, W, D, Floor, PlatO, PlatSide,
			P.StepCount, P.StepTread, BayBoundaryX(DoorIdx), BayBoundaryX(DoorIdx + 1));

		// 2) 下鹼 and the three closed walls.
		{
			const int32 BaseFirstTri = Mesh.MaxTriangleID();
			Shell::AppendBaseCourseU(Mesh, W, D, T, Floor, BaseH, BaseP);
			SetMaterialIDForTrianglesFrom(Mesh, BaseFirstTri, MatSlot_BaseCourse);
		}

		AppendBox(Mesh, FVector3d(T, D - T, WallBottom), FVector3d(W - T, D, Eave));
		AppendBox(Mesh, FVector3d(0.0, 0.0, WallBottom), FVector3d(T, D, Eave));
		AppendBox(Mesh, FVector3d(W - T, 0.0, WallBottom), FVector3d(W, D, Eave));

		// No 墀頭: it is the corbelled head of a gable wall, and this roof has no gable ends.

		// --- The frame ---
		const double ColTopR = Frame::TaperedTopRadius(ColR, ColH, P.ColumnTaperRatio);
		const double ArchSection = FMath::Max(2.0 * ColR * 0.85, 10.0);
		const double ArchBottom = Eave - ArchSection;

		// 3) The 檐柱 row on the facade line.
		const int32 ColumnFirstTri = Mesh.MaxTriangleID();
		Frame::AppendColumnRow(Mesh, BayBoundaryX, N, 0.0, ColR, ColTopR, Floor, Eave, 1.0, 20);
		SetMaterialIDForTrianglesFrom(Mesh, ColumnFirstTri, MatSlot_Wood);

		// The 額枋 and 雀替 are tagged 彩畫.
		const int32 PaintFirstTri = Mesh.MaxTriangleID();
		Frame::AppendArchitrave(Mesh, BayBoundaryX(0), BayBoundaryX(N), 0.0,
			ArchSection, ArchBottom, Eave);

		// 雀替 in the top corners of every bay.
		if (P.bHasBrackets && ArchBottom > Floor + 60.0)
		{
			const double Reach = FMath::Clamp(P.BracketReach, 5.0,
				0.35 * (BayBoundaryX(1) - BayBoundaryX(0)));
			const double Drop = FMath::Min(0.55 * Reach, 0.25 * ColH);
			const double BY = 0.62 * ColR;
			for (int32 i = 0; i < N; ++i)
			{
				const double X0 = BayBoundaryX(i);
				const double X1 = BayBoundaryX(i + 1);
				// Each is a wedge read as two steps.
				for (int32 s = 0; s < 2; ++s)
				{
					const double F = (s + 1) / 2.0;
					const double R = Reach * (1.0 - 0.5 * s);
					AppendBox(Mesh, FVector3d(X0, -BY, ArchBottom - Drop * F),
						FVector3d(X0 + R, BY, ArchBottom - Drop * (F - 0.5)));
					AppendBox(Mesh, FVector3d(X1 - R, -BY, ArchBottom - Drop * F),
						FVector3d(X1, BY, ArchBottom - Drop * (F - 0.5)));
				}
			}
		}

		SetMaterialIDForTrianglesFrom(Mesh, PaintFirstTri, MatSlot_Paint);

		// The bays: 隔扇 across the 明間, 檻牆 with a latticed panel over it in the others.
		int32 LeafFirstTri = MAX_int32;
		int32 LeafEndTri = MAX_int32;
		const double Sill = Floor + FMath::Clamp(P.SillHeightAboveFloor, 20.0, 0.6 * ColH);
		const double DoorHead = FMath::Clamp(
			Floor + FMath::Clamp(P.DoorHeadRatio, 0.2, 0.95) * ColH,
			Floor + 100.0, ArchBottom - 5.0);
		const double BarT = FMath::Clamp(P.LatticeBarThickness, 1.0, 0.5 * T);

		for (int32 i = 0; i < N; ++i)
		{
			const double X0 = BayBoundaryX(i);
			const double X1 = BayBoundaryX(i + 1);

			if (i == DoorIdx)
			{
				const int32 DoorFirstTri = Mesh.MaxTriangleID();
				const double BayW = X1 - X0;
				const double FrameT = FMath::Clamp(P.DoorFrameThickness, 2.0, T);
				const double DoorW = BayW * FMath::Clamp(P.DoorWidthFraction, 0.2, 1.0);
				const double JambL = X0 + 0.5 * (BayW - DoorW);
				const double JambR = JambL + DoorW;

				FHutongDoorAssembly Door;
				Door.OpeningX0 = JambL;
				Door.OpeningX1 = JambR;
				Door.FrontY = 0.0;
				Door.BackY = T;
				Door.BottomZ = Floor;
				Door.LeafTopZ = DoorHead;
				Door.JambTopZ = ArchBottom;
				Door.FrameThickness = FrameT;
				Door.ThresholdHeight = P.ThresholdHeight;
				Door.PegCount = P.DoorPegCount;
				// Both leaves folded flat, as on the 垂花門 and for the same reason.
				Door.bUseLeafAngles = true;
				Door.LeftLeafAngleDeg = P.bDoorLeavesOpen ? -90.0 : 0.0;
				Door.RightLeafAngleDeg = P.bDoorLeavesOpen ? -90.0 : 0.0;
				AppendDoorAssembly(Mesh, Door, &LeafFirstTri);
				LeafEndTri = Mesh.MaxTriangleID();

				// 餘塞板 from each jamb out to the flanking column, and a board over the head.
				if (JambL - FrameT > X0)
				{
					AppendBox(Mesh, FVector3d(X0, 0.2 * T, Floor),
						FVector3d(JambL - FrameT, 0.8 * T, DoorHead));
				}
				if (X1 > JambR + FrameT)
				{
					AppendBox(Mesh, FVector3d(JambR + FrameT, 0.2 * T, Floor),
						FVector3d(X1, 0.8 * T, DoorHead));
				}
				if (ArchBottom > DoorHead)
				{
					AppendBox(Mesh, FVector3d(X0, 0.2 * T, DoorHead),
						FVector3d(X1, 0.8 * T, ArchBottom));
				}

				// The whole bay is joinery, then the leaves alone take the lacquer.
				SetMaterialIDForTrianglesFrom(Mesh, DoorFirstTri, MatSlot_Wood);
				SetMaterialIDForTriangleRange(Mesh, LeafFirstTri, LeafEndTri, MatSlot_DoorPaint);
				continue;
			}

			// 檻牆: one box per run, not one per bay.
			if (Sill > Floor)
			{
				AppendBox(Mesh, FVector3d(X0, 0.0, Floor), FVector3d(X1, T, Sill));
			}

			// The lattice above it.
			const double Top = ArchBottom;
			if (Top - Sill > 3.0 * BarT && X1 - X0 > 3.0 * BarT)
			{
				const double MullY0 = 0.18 * T;
				const double RailY0 = 0.34 * T;

				// 窗紙 first, so the bars stand in front of it.
				if (P.bHasWindowPaper)
				{
					const int32 PaperFirstTri = Mesh.MaxTriangleID();
					const double PaperY = RailY0 + 1.4 * BarT;
					AppendBox(Mesh, FVector3d(X0, PaperY, Sill),
						FVector3d(X1, PaperY + 0.25 * BarT, Top));
					SetMaterialIDForTrianglesFrom(Mesh, PaperFirstTri, MatSlot_Paper);
				}

				const int32 LatticeFirstTri = Mesh.MaxTriangleID();
				const int32 Mullions = FMath::Clamp(P.LatticeMullions, 0, 24);
				for (int32 j = 1; j <= Mullions; ++j)
				{
					const double A = X0 + (X1 - X0) * j / double(Mullions + 1);
					AppendBox(Mesh, FVector3d(A - 0.5 * BarT, MullY0, Sill),
						FVector3d(A + 0.5 * BarT, MullY0 + BarT, Top));
				}
				const int32 Rails = FMath::Clamp(P.LatticeRails, 0, 16);
				for (int32 j = 1; j <= Rails; ++j)
				{
					const double Z = Sill + (Top - Sill) * j / double(Rails + 1);
					AppendBox(Mesh, FVector3d(X0, RailY0, Z - 0.5 * BarT),
						FVector3d(X1, RailY0 + BarT, Z + 0.5 * BarT));
				}

				SetMaterialIDForTrianglesFrom(Mesh, LatticeFirstTri, MatSlot_Lattice);
			}
		}

		// 6) The roof, overhanging on all four sides.
		const double O = FMath::Max(P.RoofOverhang, 0.0);
		const int32 RoofFirstTri = Mesh.MaxTriangleID();
		UE::Geometry::FIndex2i MainRidge(0, 0);

		const double RoofW = W + 2.0 * O;
		const double RoofD = D + 2.0 * O;
		// 舉架 over the building's own depth, with the eave overhang as the outermost run.
		const HutongGen::FHutongRoofSection Section =
			HutongGen::Jiajia::MakeSection(P.Purlins, 0.5 * D, O, P.RoofApexRoll);
		const double Rise = P.GetRoofRise(D);
		const FVector3d RoofMin(-O, -O, Eave);

		if (P.RoofType == EHutongRoofType::Xieshan)
		{
			FXieshanRoofSpec Xie;
			Xie.Width = RoofW;
			Xie.Depth = RoofD;
			Xie.Rise = Rise;
			Xie.Section = Section;
			Xie.bEaveCaps = HutongGen::RoofTile::HasEaveCaps(P.RoofTile);
			Xie.TileRowSpacing = P.TileRowSpacing;
			Xie.ShouInset = FMath::Max(P.ShouInset, 1.0);
			Xie.FlareRise = FMath::Max(P.RoofFlareRise, 0.0);
			Xie.FlareRun = FMath::Max(P.RoofFlareRun, 0.0);
			Xie.FlareLength = 0.35 * FMath::Min(RoofW, RoofD);
			Xie.FasciaDrop = FMath::Max(P.EaveFasciaDepth, 0.0);
			Xie.RidgeWidth = FMath::Max(P.RidgeCourseWidth, 0.0);
			Xie.RidgeHeight = FMath::Max(P.RidgeCourseHeight, 0.0);
			Xie.BargeThickness = FMath::Max(P.BargeBoardThickness, 0.0);
			Xie.BargeDepth = FMath::Max(P.BargeBoardDepth, 0.0);
			Xie.EaveSegments = P.RoofEaveSegments;
			Xie.SlopeSegments = FMath::Max(P.RoofSlopeSegments, 3);
			AppendXieshanRoof(Mesh, RoofMin, Xie, &MainRidge);
		}
		else
		{
			FHipRoofSpec Hip;
			Hip.Width = RoofW;
			Hip.Depth = RoofD;
			Hip.RidgeLength = (P.RoofType == EHutongRoofType::Wudian)
				? FMath::Max(RoofW - RoofD, 0.0) : 0.0;
			Hip.Rise = Rise;
			Hip.Section = Section;
			Hip.bEaveCaps = HutongGen::RoofTile::HasEaveCaps(P.RoofTile);
			Hip.TileRowSpacing = P.TileRowSpacing;
			Hip.FlareRise = FMath::Max(P.RoofFlareRise, 0.0);
			Hip.FlareRun = FMath::Max(P.RoofFlareRun, 0.0);
			Hip.FlareLength = 0.35 * FMath::Min(RoofW, RoofD);
			Hip.FasciaDrop = FMath::Max(P.EaveFasciaDepth, 0.0);
			Hip.EaveSegments = P.RoofEaveSegments;
			Hip.SlopeSegments = P.RoofSlopeSegments;
			AppendHippedRoof(Mesh, RoofMin, Hip);
		}

		SetMaterialIDForTrianglesFrom(Mesh, RoofFirstTri, MatSlot_Roof);
		SetMaterialIDForTriangleRange(Mesh, MainRidge.A, MainRidge.B, MatSlot_Ridge);
	}
}
