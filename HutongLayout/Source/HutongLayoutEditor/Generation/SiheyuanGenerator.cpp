#include "Generation/SiheyuanGenerator.h"
#include "Generation/FrameGenerator.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongActorSpawn.h"
#include "Generation/HutongDoor.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongShell.h"
#include "Generation/HutongJoinery.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	using namespace HutongMeshUtils;

	using FBayBoundary = TFunctionRef<double(int32)>;

namespace
{
	// 舉架 over the drawn depth, eave overhang as outermost run. A 廊 is one of the section's 步架, never
	// added outside: 前廊後無廊 framed otherwise is the 撅尾巴房 (rear eave a 金檁 high, ridge off centre),
	// which the 鑽金柱 frame avoids (四合院建築及其構造 p.85).
	Shell::FRoofParams MakeRoofParams(const FHutongSiheyuanParams& P, double Depth)
	{
		Shell::FRoofParams Roof;
		Roof.FrontOverhang = P.GetRoofOverhang();
		Roof.UndersideRise = P.GetUndersideRise();
		if (P.bExposedFrame) Roof.ShellCover = HutongCanon::Frame::RoofCover * P.GetColumnDiameter();
		Roof.RearOverhang = P.GetRearRoofOverhang();
		Roof.SealedCornice = P.RearCornice;
		Roof.RearSlopeTrim = (P.RearEave == EHutongRearEave::Lane)
			? FMath::Max(Roof.FrontOverhang - Roof.RearOverhang, 0.0)
			: 0.0;
		Roof.Section = Jiajia::MakeSection(
			P.Purlins, 0.5 * Depth, Roof.FrontOverhang, P.RoofApexRoll);
		// Zero keeps the section's own (canonical) rise.
		Roof.Rise = P.bDeriveProportions ? 0.0 : P.GetRoofRise();
		Roof.SlopeSegments = P.RoofSlopeSegments;
		Roof.FasciaDepth = P.EaveFasciaDepth;
		Roof.FasciaWidth = P.EaveFasciaWidth;
		Roof.RafterSection = P.RafterEndSection;
		Roof.RafterSpacing = P.RafterEndSpacing;
		Roof.RafterEndInset = P.bHasChitou ? P.GetGableWallThickness() : 0.0;
		Roof.bFlyingRafters = P.bHasFlyingRafters;
		Roof.bHasRidgeCourse = P.bHasRidgeCourse;
		Roof.RidgeCourseHeight = P.RidgeCourseHeight;
		Roof.RidgeCourseWidth = P.RidgeCourseWidth;
		Roof.RidgeEndKick = P.RidgeEndKick;
		if (!P.bHasGableRake) Roof.RakeDepth = 0.0;
		Roof.Tile = P.RoofTile;
		Roof.TileRowSpacing = P.TileRowSpacing;
		Roof.bTileRuns = P.bHasTileRuns;
		Roof.bOpenLowEnd = P.bRoofRunsOnLow;
		Roof.bOpenHighEnd = P.bRoofRunsOnHigh;
		return Roof;
	}

	// Wall run with rectangular openings, laid as three bands.
	void AppendPiercedWall(FDynamicMesh3& Mesh, double X0, double X1, double Y0, double Y1,
		double Bottom, double Top, double Sill, double Head,
		const TArray<TPair<double, double>>& Openings)
	{
		if (X1 <= X0 || Top <= Bottom) return;

		if (Openings.Num() == 0 || Head <= Sill || Sill <= Bottom || Head >= Top)
		{
			AppendBox(Mesh, FVector3d(X0, Y0, Bottom), FVector3d(X1, Y1, Top));
			return;
		}

		AppendBox(Mesh, FVector3d(X0, Y0, Bottom), FVector3d(X1, Y1, Sill));
		AppendBox(Mesh, FVector3d(X0, Y0, Head),   FVector3d(X1, Y1, Top));

		double Cursor = X0;
		for (const TPair<double, double>& Gap : Openings)
		{
			if (Gap.Key > Cursor)
			{
				AppendBox(Mesh, FVector3d(Cursor, Y0, Sill), FVector3d(Gap.Key, Y1, Head));
			}
			Cursor = FMath::Max(Cursor, Gap.Value);
		}
		if (X1 > Cursor)
		{
			AppendBox(Mesh, FVector3d(Cursor, Y0, Sill), FVector3d(X1, Y1, Head));
		}
	}

	// One 高窗 per bay, centred.
	TArray<TPair<double, double>> RearWindowSpans(const FHutongSiheyuanParams& P,
		double W, double T, int32 N, const FBayBoundary& BayBoundaryX)
	{
		TArray<TPair<double, double>> Spans;
		for (int32 i = 0; i < N; ++i)
		{
			const double A = BayBoundaryX(i);
			const double B = BayBoundaryX(i + 1);
			const double Half = FMath::Min(0.5 * FMath::Max(P.RearWindowWidth, 20.0), 0.35 * (B - A));
			if (Half <= 5.0) continue;

			const double C = 0.5 * (A + B);
			const double X0 = FMath::Max(C - Half, T + 10.0);
			const double X1 = FMath::Min(C + Half, W - T - 10.0);
			if (X1 - X0 > 10.0) Spans.Add({ X0, X1 });
		}
		return Spans;
	}

	// 窗紙 and 欞條 in the 高窗.
	void AppendRearWindows(FDynamicMesh3& Mesh, const FHutongSiheyuanParams& P,
		double D, double T, double Sill, double Head,
		const TArray<TPair<double, double>>& Spans)
	{
		const double BarT = FMath::Clamp(P.WindowLatticeThickness, 1.0, T * 0.5);
		const double OuterY = D - 0.2 * T;

		for (const TPair<double, double>& Span : Spans)
		{
			const double X0 = Span.Key, X1 = Span.Value;

			if (P.bHasWindowPaper)
			{
				FSlotScope PaperTag(Mesh, MatSlot_Paper);
				const double PaperY = OuterY - 1.4 * BarT;
				AppendBox(Mesh,
					FVector3d(X0, PaperY - 0.25 * BarT, Sill),
					FVector3d(X1, PaperY,               Head));
				PaperTag.Close();
			}

			if (!P.bHasWindowLattice) continue;

			FSlotScope LatticeTag(Mesh, MatSlot_Lattice);
			// Two uprights and one rail: a full 支摘窗 grid in a 70 cm opening reads as a smudge at full cost.
			for (int32 j = 1; j <= 2; ++j)
			{
				const double CX = X0 + (X1 - X0) * j / 3.0;
				AppendBox(Mesh,
					FVector3d(CX - 0.5 * BarT, OuterY - BarT, Sill),
					FVector3d(CX + 0.5 * BarT, OuterY,        Head));
			}
			const double CZ = 0.5 * (Sill + Head);
			AppendBox(Mesh,
				FVector3d(X0, OuterY - 1.4 * BarT, CZ - 0.5 * BarT),
				FVector3d(X1, OuterY - 0.4 * BarT, CZ + 0.5 * BarT));
			LatticeTag.Close();
		}
	}

	// 白灰 on the three closed walls, 板壁 across interior bay boundaries.
	void AppendInterior(FDynamicMesh3& Mesh, const FHutongSiheyuanParams& P,
		double W, double T, double Floor, double Eave, double PlasterTop, double RoomY0, double RoomY1,
		int32 N, const FBayBoundary& BayBoundaryX,
		double RearWinSill, double RearWinHead, const TArray<TPair<double, double>>& RearWins)
	{
		const double PlasterT = P.bHasInteriorPlaster
			? FMath::Clamp(P.InteriorPlasterThickness, 0.5, 0.4 * T) : 0.0;
		if (PlasterT > 0.0 && RoomY1 > RoomY0 && Eave > Floor)
		{
			FSlotScope PlasterTag(Mesh, MatSlot_Plaster);
			// Sides full depth, back between them (the 下鹼 tiling), so no two meet at a shared plane.
			AppendBox(Mesh, FVector3d(T, RoomY0, Floor),
				FVector3d(T + PlasterT, RoomY1, PlasterTop));
			AppendBox(Mesh, FVector3d(W - T - PlasterT, RoomY0, Floor),
				FVector3d(W - T, RoomY1, PlasterTop));
			if (W - 2.0 * (T + PlasterT) > 0.0)
			{
				// Same splits as the brickwork, or the lime seals the 高窗.
				AppendPiercedWall(Mesh, T + PlasterT, W - T - PlasterT,
					RoomY1 - PlasterT, RoomY1, Floor, PlasterTop,
					RearWinSill, RearWinHead, RearWins);
			}
			PlasterTag.Close();
		}

		// 板壁 is joinery: the frame carries the roof (牆倒屋不塌), partitions take no load.
		if (!P.bHasInteriorPartitions || N <= 1 || RoomY1 <= RoomY0)
		{
			return;
		}

		FSlotScope PartTag(Mesh, MatSlot_Partition);
		const double HalfT = 0.5 * FMath::Clamp(P.InteriorPartitionThickness, 2.0, T);
		const double DoorW = FMath::Clamp(P.InteriorDoorWidth, 0.0, RoomY1 - RoomY0);
		const double HeadZ = FMath::Min(
			FMath::Max(P.GetDoorLeafTopHeight(), Passage::MinHeadZ(Floor, 0.0)), Eave);
		// Centred in depth: the 碧紗櫥 it stands for is symmetric about the room axis.
		const double DoorY0 = 0.5 * (RoomY0 + RoomY1) - 0.5 * DoorW;
		const double DoorY1 = DoorY0 + DoorW;

		for (int32 i = 1; i < N; ++i)
		{
			const double Xc = BayBoundaryX(i);
			const double X0 = Xc - HalfT, X1 = Xc + HalfT;
			if (DoorW <= 0.0 || HeadZ <= Floor)
			{
				AppendBox(Mesh, FVector3d(X0, RoomY0, Floor), FVector3d(X1, RoomY1, Eave));
				continue;
			}
			AppendBox(Mesh, FVector3d(X0, RoomY0, Floor), FVector3d(X1, DoorY0, Eave));
			AppendBox(Mesh, FVector3d(X0, DoorY1, Floor), FVector3d(X1, RoomY1, Eave));
			if (Eave > HeadZ)
			{
				AppendBox(Mesh, FVector3d(X0, DoorY0, HeadZ), FVector3d(X1, DoorY1, Eave));
			}
		}
		PartTag.Close();
	}

	// 裝修 on the facade plane (圖5-4-5): 檻牆 under a 榻板, 抱框 at the columns, 支摘窗 in the 次間, 隔扇 with
	// 簾架 and 風門 in the 明間, 中檻, 橫陂 and 上檻 over every bay, the 枋 face up to the ceiling.
	void AppendFacade(FDynamicMesh3& Mesh, const FHutongSiheyuanParams& P, const Shell::FRoofParams& Roof, double D, double RoofZ,
		double W, double TG, double TF, double PT, double FY, double Floor, double Sill, double WinTop,
		double LintelBottom, double CeilZ, double GoldTop, int32 N, int32 DoorIdx, const FBayBoundary& BayBoundaryX)
	{
		namespace J = HutongCanon::Joinery;
		const bool bDoor = P.bHasFrontDoorCenter;
		const bool bDoorJoinery = bDoor && P.bHasDoorFrame;
		const double LeafTop = bDoor ? FMath::Clamp(P.GetDoorLeafTopHeight(), WinTop, LintelBottom - 4.0) : WinTop;
		// 表十三 in 柱徑: every 檻 ³⁄₁₀ thick (at least the leaves' thickness), 榻板 1½ wide and ⅜ thick, 風檻 ½ high.
		const double ColD = P.GetColumnDiameter();
		const double RailY0 = FY + 0.5;
		// Deep enough to hold the 隔扇 (表十四: 3/20 of a leaf's width).
		const double RailY1 = RailY0 + FMath::Clamp(FMath::Max(J::SillThicknessInD * ColD, J::GeshanDepthOfWidth * P.GetGeshanWidth() + 2.0),
			J::LeafThicknessCm + 2.0, TF - 1.0);
		const double LeafT = FMath::Min(J::LeafThicknessCm, RailY1 - RailY0 - 2.0);
		const double SashY = RailY0 + 0.5 * (RailY1 - RailY0 - LeafT);
		const double BoardZ = FMath::Max(Sill - J::SillBoardThicknessInD * ColD, Floor);
		const double BoardLap = FMath::Max(0.5 * (J::SillBoardWidthInD * ColD - TF), 1.0);
		const double WindowRail = FMath::Min(J::WindowSillRailInD * ColD, 0.3 * FMath::Max(WinTop - Sill, 0.0));

		Joinery::FLattice Lattice;
		Lattice.Mullions = P.WindowMullions;
		Lattice.Rails = P.WindowRails;
		Lattice.BarWidth = FMath::Clamp(P.WindowLatticeThickness, 1.0, TF * 0.5);
		Lattice.bBars = P.bHasWindowLattice;
		Lattice.bPaper = P.bHasWindowPaper;

		auto Left = [&](int32 i) { return (i == 0) ? TG : BayBoundaryX(i) + 0.5 * PT; };
		auto Right = [&](int32 i) { return (i == N - 1) ? W - TG : BayBoundaryX(i + 1) - 0.5 * PT; };
		auto IsDoor = [&](int32 i) { return bDoor && i == DoorIdx; };

		// 檻牆 and its 榻板, one run per stretch of window bays, behind intermediate columns.
		auto SillRun = [&](int32 First, int32 Last)
		{
			if (Last < First) return;
			const double X0 = (First == 0) ? TG : BayBoundaryX(First);
			const double X1 = (Last == N - 1) ? W - TG : BayBoundaryX(Last + 1);
			if (X1 <= X0 || Sill <= Floor) return;
			if (BoardZ > Floor) AppendBox(Mesh, FVector3d(X0, FY, Floor), FVector3d(X1, FY + TF, BoardZ));
			FSlotScope BoardTag(Mesh, MatSlot_Wood);
			AppendBox(Mesh, FVector3d(X0, FY - BoardLap, BoardZ), FVector3d(X1, FY + TF + BoardLap, Sill));
		};
		if (bDoor) { SillRun(0, DoorIdx - 1); SillRun(DoorIdx + 1, N - 1); }
		else SillRun(0, N - 1);

		// 海棠池子 (圖5-3-10.1) on each window bay's street face between the columns: 大枋子, 線枋子, diagonal 墻心.
		if (P.SillWallFinish == EHutongSillWall::Pool && BoardZ > Floor)
		{
			const double ColR = P.GetColumnRadius();
			for (int32 i = 0; i < N; ++i)
			{
				if (IsDoor(i)) continue;
				const double X0 = (i == 0) ? TG : BayBoundaryX(i) + ColR;
				const double X1 = (i == N - 1) ? W - TG : BayBoundaryX(i + 1) - ColR;
				const double Z0 = Floor, Z1 = BoardZ;
				const double B = FMath::Min(J::SillPoolBorderCm, 0.25 * FMath::Min(X1 - X0, Z1 - Z0));
				const double L = J::SillPoolLineCm;
				if (X1 - X0 < 2.0 * (B + L) + 10.0 || Z1 - Z0 < 2.0 * (B + L) + 10.0) continue;
				auto Ring = [&](double Inset, double Width, double Proud)
				{
					const double A0 = X0 + Inset, A1 = X1 - Inset, C0 = Z0 + Inset, C1 = Z1 - Inset;
					const double Y0 = FY - Proud, Y1 = FY + 1.0;
					AppendBox(Mesh, FVector3d(A0, Y0, C0), FVector3d(A1, Y1, C0 + Width));
					AppendBox(Mesh, FVector3d(A0, Y0, C1 - Width), FVector3d(A1, Y1, C1));
					AppendBox(Mesh, FVector3d(A0, Y0, C0 + Width), FVector3d(A0 + Width, Y1, C1 - Width));
					AppendBox(Mesh, FVector3d(A1 - Width, Y0, C0 + Width), FVector3d(A1, Y1, C1 - Width));
				};
				Ring(0.0, B, J::SillPoolBorderProudCm);
				Ring(B, L, J::SillPoolLineProudCm);
				const int32 FieldTri = Mesh.MaxTriangleID();
				{
					FSlotScope FieldTag(Mesh, MatSlot_Floor);
					AppendBox(Mesh, FVector3d(X0 + B + L, FY - J::SillPoolFieldProudCm, Z0 + B + L),
						FVector3d(X1 - B - L, FY + 1.0, Z1 - B - L));
				}
				SetDiagonalPaverUVs(Mesh, FieldTri, FVector3d::XAxisVector, FVector3d::ZAxisVector,
					FVector3d(0.5 * (X0 + X1), FY, 0.5 * (Z0 + Z1)));
			}
		}

		// Over the 隔扇 and windows, one rail at the 檐枋's line (上檻, or 中檻 where 橫陂 follow). Behind a 前廊
		// the 金柱 rise past that line (GoldTop): the 橫陂 fill the height up to an 上檻 under the 金枋 (p.104).
		const double HeadH = FMath::Max(LintelBottom - WinTop, 1.0);
		const double BeamH = ColD;
		// The 橫陂's own 上檻 under the 金枋, ½ 柱徑.
		const double UpperH = P.bDeriveProportions ? J::UpperSillInD * ColD : HeadH;
		const double TransomTop = GoldTop - BeamH - UpperH;
		const bool bTransom = FY > 0.0 && P.TransomPanes > 0 && TransomTop - LintelBottom >= J::MinTransomCm;
		const double BeamZ = bTransom ? TransomTop + UpperH : LintelBottom;

		FSlotScope WoodTag(Mesh, MatSlot_Wood);
		// The 枋 face across the whole front, up to the ceiling, or to the underside where it is open over the
		// 廊 or a 徹上明造 shell.
		if (Roof.VerandaEnd > 0.0 || Roof.ShellCover > 0.0)
		{
			Shell::AppendWallToUnderside(Mesh, Roof, D, RoofZ, TG, W - TG, FY, FY + TF, BeamZ);
		}
		else if (CeilZ > BeamZ)
		{
			AppendBox(Mesh, FVector3d(TG, FY, BeamZ), FVector3d(W - TG, FY + TF, CeilZ));
		}

		// 抱框 at every interior bay boundary, full wall depth; the door's side carried down to the floor.
		for (int32 i = 1; i < N; ++i)
		{
			const double B = BayBoundaryX(i);
			AppendBox(Mesh, FVector3d(B - 0.5 * PT, FY, Sill), FVector3d(B + 0.5 * PT, FY + TF, BeamZ));
			if (IsDoor(i) && Sill > Floor) AppendBox(Mesh, FVector3d(B, FY, Floor), FVector3d(B + 0.5 * PT, FY + TF, Sill));
			if (IsDoor(i - 1) && Sill > Floor) AppendBox(Mesh, FVector3d(B - 0.5 * PT, FY, Floor), FVector3d(B, FY + TF, Sill));
		}
		WoodTag.Close();

		for (int32 i = 0; i < N; ++i)
		{
			const double X0 = Left(i), X1 = Right(i);
			if (X1 - X0 < 10.0) continue;
			const double Rail = IsDoor(i) ? LeafTop : WinTop;
			if (IsDoor(i) && !bDoorJoinery)
			{
				// An open bay: the 枋 face comes down to the doorway's head.
				if (bTransom)
				{
					FSlotScope FaceTag(Mesh, MatSlot_Wood);
					AppendBox(Mesh, FVector3d(X0 - 1.0, FY, LintelBottom), FVector3d(X1 + 1.0, FY + TF, BeamZ));
				}
				continue;
			}
			{
				FSlotScope RailTag(Mesh, MatSlot_Wood);
				AppendBox(Mesh, FVector3d(X0 - 1.0, RailY0, Rail), FVector3d(X1 + 1.0, RailY1, LintelBottom));
				if (bTransom) AppendBox(Mesh, FVector3d(X0 - 1.0, RailY0, TransomTop), FVector3d(X1 + 1.0, RailY1, BeamZ));
			}
			if (bTransom)
			{
				Joinery::AppendTransom(Mesh, X0, X1, LintelBottom, TransomTop, SashY, LeafT,
					RailY0, RailY1, P.TransomPanes, Lattice);
			}

			if (IsDoor(i))
			{
				Joinery::FDoorBay Bay;
				Bay.X0 = X0; Bay.X1 = X1;
				Bay.FloorZ = Floor;
				Bay.SillTopZ = Floor + P.GetLowerSillHeight();
				Bay.RailZ = Rail;
				Bay.RailTopZ = LintelBottom;
				Bay.SplitZ = FMath::Max(Sill, Bay.SillTopZ + 20.0);
				Bay.RailY0 = RailY0; Bay.RailY1 = RailY1;
				Bay.bCurtain = P.bHasCurtainFrame;
				Bay.bOpen = P.bDoorLeavesOpen;
				Bay.MinDoorWidth = 2.0 * HutongCanon::Openings::WalkerRadiusCm + 10.0;
				Bay.MinClearHeight = HutongGen::Passage::MinClearHeight;
				Bay.DoorClearHeight = J::FengmenClearCm;
				Joinery::AppendDoorBay(Mesh, Bay, Lattice);
			}
			else if (WinTop > Sill + WindowRail)
			{
				// 風檻 on the 榻板 under the sashes.
				if (WindowRail > 0.0)
				{
					FSlotScope SillRailTag(Mesh, MatSlot_Wood);
					AppendBox(Mesh, FVector3d(X0 - 1.0, RailY0, Sill), FVector3d(X1 + 1.0, RailY1, Sill + WindowRail));
				}
				Joinery::AppendWindow(Mesh, X0, X1, Sill + WindowRail, WinTop, SashY, LeafT, RailY0, RailY1, Lattice);
			}
		}
	}
}

	void BuildSiheyuan(FDynamicMesh3& Mesh, const FHutongSiheyuanParams& P)
	{

		const double W = FMath::Max(P.Width, 1.0);
		const double D = FMath::Max(P.Depth, 1.0);
		const double Eave = FMath::Max(P.GetEaveHeight(), 10.0);
		// The roof and every wall that meets it stand on the frame's line; columns and 額枋 at the column top.
		const double RoofZ = Eave + P.GetRoofLift();
		// Walls rise to the ceiling under it, the rafters' tops on the 檐檁 (buried in the roof where it is lower).
		const double CeilZ = RoofZ + P.GetUndersideRise();
		const double Floor = FMath::Clamp(P.GetFloorHeight(), 0.0, Eave * 0.5);
		// 表十二: 山牆 (TG), the facade's 檻牆 and walls (TF), the 檐牆 behind (TR).
		const double WallCap = FMath::Min(W, D) * 0.2;
		const double TG = FMath::Clamp(P.GetGableWallThickness(), 1.0, WallCap);
		const double TF = FMath::Clamp(P.GetFacadeWallThickness(), 1.0, WallCap);
		const double TR = FMath::Clamp(P.GetRearWallThickness(), 1.0, WallCap);
		const double PT = FMath::Clamp(P.GetPostThickness(), 1.0, TF * 1.5);
		const double LintelBottom = FMath::Clamp(P.GetDoorTopHeight(), Floor + 10.0, Eave - 10.0);
		const double Sill = FMath::Clamp(P.GetWindowSillHeight(), Floor, LintelBottom - 20.0);
		const double WinTop = FMath::Clamp(P.GetWindowTopHeight(), Sill + 10.0, LintelBottom);

		const int32 N = (P.BayCountOverride > 0)
			? P.BayCountOverride
			: ComputeBayCount(W, P.MinBayWidth, P.MaxBayWidth);
		const int32 DoorIdx = P.GetDoorBayIndex(N);

		const double ColR = P.GetColumnRadius();

		// 明間 wider than the 次間.
		auto BayBoundaryX = [&](int32 i) { return P.GetBayBoundary(i, N, W, ColR); };

		// 前廊: wall retreats one 廊步, 檐柱 stay on the footprint edge. 後廊: back wall on the 後檐柱 line,
		// rear 金柱 one 廊步 inside.
		double FY, RY;
		P.GetBuiltVerandaDepths(D, TF, FY, RY);
		// The frame builds only the 廊 the house does (a 徹上明造 house's 廊 are already on its lines).
		FHutongSiheyuanParams Framed = P;
		Framed.Width = W;
		Framed.Depth = D;
		Framed.bHasFrontVeranda = FY > 0.0;
		Framed.bHasRearVeranda = RY > 0.0;
		const FrameLayout::FLayout FrameL = FrameLayout::Make(Framed);
		const bool bVeranda = (FY > 0.0);

		const double BaseH = FMath::Clamp(P.GetBaseCourseHeight(), 0.0, (Eave - Floor) * 0.6);
		const double BaseP = FMath::Max(P.BaseCourseProjection, 0.0);
		const double WallBottom = (BaseH > 0.0 && BaseP > 0.0) ? Floor + BaseH : Floor;

		// 臺基 projects in front only; 踏跺 under the door, not the middle.
		const double PlatO = FMath::Max(P.PlatformOverhang, ColR);
		const double PlatSide = 0.0;
		Shell::AppendPlatform(Mesh, W, D, Floor, PlatO, PlatSide,
			P.bHasFrontDoorCenter ? P.StepCount : 0, P.StepTread,
			BayBoundaryX(DoorIdx), BayBoundaryX(DoorIdx + 1));

		// 廊門筒子: gable walls open across the 前廊 between its columns. Walked upright along the veranda:
		// standing head height held under the eave, not a low room's crouch.
		double EndDoorY0 = 0.0, EndDoorY1 = 0.0;
		const double EndDoorHead = FMath::Max(HutongGen::Passage::MinHeadZ(Floor, 0.0),
			FMath::Min(Floor + HutongCanon::Openings::WalkerHeightCm + 20.0, Eave - 20.0));
		if (P.bHasVerandaEndDoorways && bVeranda && EndDoorHead < Eave - 20.0)
		{
			// Column face to column face: a 前出廊 wing's 廊步 is under a metre; any margin leaves a slot.
			EndDoorY0 = ColR + 1.0;
			EndDoorY1 = FY - ColR - 1.0;
			if (EndDoorY1 - EndDoorY0 < 50.0) EndDoorY0 = EndDoorY1 = 0.0;
		}

		// 下鹼 on three sides; front stays flush to clear the facade columns.
		{
			FSlotScope BaseTag(Mesh, MatSlot_BaseCourse);
			Shell::AppendBaseCourseU(Mesh, W, D, TG, Floor, BaseH, BaseP,
				/*bIncludeRear*/ true, /*bToGround*/ true, EndDoorY0, EndDoorY1, TR);
			BaseTag.Close();
		}

		// 高窗 under the 封護檐 back wall eave, one per bay.
		TArray<TPair<double, double>> RearWins;
		double RearWinSill = 0.0, RearWinHead = 0.0;
		if (P.bHasRearHighWindows)
		{
			RearWinHead = Eave - FMath::Clamp(P.RearWindowHeadDrop, 5.0, (Eave - WallBottom) * 0.6);
			RearWinSill = RearWinHead
				- FMath::Clamp(P.RearWindowHeight, 10.0, FMath::Max(RearWinHead - WallBottom - 40.0, 10.0));
			if (RearWinSill > WallBottom + 30.0)
			{
				RearWins = RearWindowSpans(P, W, TG, N, BayBoundaryX);
			}
		}

		AppendPiercedWall(Mesh, TG, W - TG, D - TR, D, WallBottom, CeilZ,
			RearWinSill, RearWinHead, RearWins);

		Shell::FRoofParams Roof = MakeRoofParams(P, D);
		Roof.GableWallThickness = TG;
		Roof.SealedCorniceFloorZ = RearWins.Num() > 0 ? RearWinHead + 5.0 : WallBottom + 20.0;
		// 前廊: the 檐椽 open over the 廊 up to the 金檁, and the 金柱 as much taller than the 檐柱 as the roof
		// climbs over the 廊 (四合院建築及其構造 圖5-3-1/5-3-2; p.104's 橫陂 fill the difference). A 徹上明造
		// house has its shell already; its 金柱 rise to the frame's own 金檁 support.
		if (bVeranda && !P.bExposedFrame) Roof.VerandaEnd = FY + TF;
		double GoldTop = Eave, RearGoldTop = Eave;
		if (Roof.VerandaEnd > 0.0)
		{
			GoldTop = FMath::Max(Shell::UndersideAt(Roof, D, RoofZ, FY) - (CeilZ - Eave), Eave);
		}
		else if (bVeranda && P.bExposedFrame)
		{
			// Front and rear 金柱 up to the frame's own supports, under its main beams.
			if (FrameL.bFrontVeranda) GoldTop = FMath::Max(FrameL.Support[FrameL.Front], Eave);
			if (FrameL.bRearVeranda) RearGoldTop = FMath::Max(FrameL.Support[FrameL.Rear], Eave);
		}

		// Over the 廊 the 山牆 rise to the open underside.
		if (Roof.VerandaEnd > 0.0)
		{
			for (const double X0 : { 0.0, W - TG }) Shell::AppendWallToUnderside(Mesh, Roof, D, RoofZ, X0, X0 + TG, 0.0, Roof.VerandaEnd, CeilZ);
		}

		// 封護檐: the roof builds the wall carried up under it, cornice and drip course.
		for (const double X0 : { 0.0, W - TG })
		{
			if (EndDoorY1 > EndDoorY0)
			{
				AppendBox(Mesh, FVector3d(X0, 0.0, WallBottom), FVector3d(X0 + TG, EndDoorY0, CeilZ));
				AppendBox(Mesh, FVector3d(X0, EndDoorY0, EndDoorHead), FVector3d(X0 + TG, EndDoorY1, CeilZ));
				AppendBox(Mesh, FVector3d(X0, EndDoorY1, WallBottom), FVector3d(X0 + TG, D, CeilZ));
			}
			else
			{
				AppendBox(Mesh, FVector3d(X0, 0.0, WallBottom), FVector3d(X0 + TG, D, CeilZ));
			}
		}

		// Lime to the ceiling; 板壁 under the beams.
		AppendInterior(Mesh, P, W, TG, Floor, Eave, CeilZ, /*RoomY0*/ FY + TF, /*RoomY1*/ D - TR,
			N, BayBoundaryX, RearWinSill, RearWinHead, RearWins);

		if (P.bHasChitou)
		{
			Shell::AppendChitou(Mesh, W, D, TG, Floor, RoofZ,
				P.GetChitouProjection(), P.ChitouCorbelSteps, Roof, (BaseH > 0.0 && BaseP > 0.0) ? BaseH : 0.0, BaseP);
		}

		// Lintel, columns and 額枋: one wood range.
		FSlotScope WoodTag(Mesh, MatSlot_Wood);

		// 清式營造則例 表四 檐枋 ⅘, 表二 穿插枋 ¾ of the 柱徑 thick.
		const double EaveTieT = HutongCanon::Frame::EaveTieThickness * P.GetColumnDiameter();
		const double ThroughTieT = HutongCanon::Frame::ThroughTieThickness * P.GetColumnDiameter();
				// 收分 against 柱高, not the drawn height.
		const double ColOvershoot = 1.0;
		const double ColTopR = Frame::TaperedTopRadius(ColR, P.GetColumnHeight(), P.ColumnTaperRatio);

		// 金柱 in the wall plane, up to the 金檁's 墊板 behind a 前廊.
		Frame::AppendColumnRow(Mesh, BayBoundaryX, N, FY, ColR, ColTopR, 0.0, GoldTop, ColOvershoot);

		if (bVeranda)
		{
			// 檐柱 free-standing on the platform carrying the eave, 額枋 along them, 穿插枋 back to each 金柱.
			Frame::AppendColumnRow(Mesh, BayBoundaryX, N, 0.0, ColR, ColTopR, 0.0, Eave, ColOvershoot);
			Frame::AppendArchitrave(Mesh, BayBoundaryX(0), BayBoundaryX(N), 0.0, EaveTieT,
				LintelBottom, Eave);
			Frame::AppendTieBeams(Mesh, BayBoundaryX, N, 0.0, FY + 0.5 * TF, ThroughTieT,
				LintelBottom, Eave, ColOvershoot);
		}

		if (RY > 0.0)
		{
			// 後金柱 free-standing in the room, tied back to the 後檐柱 in the wall.
			Frame::AppendColumnRow(Mesh, BayBoundaryX, N, D - RY, ColR, ColTopR, 0.0, RearGoldTop, ColOvershoot);
			Frame::AppendTieBeams(Mesh, BayBoundaryX, N, D - RY, D - 0.5 * TR, ThroughTieT,
				LintelBottom, Eave, ColOvershoot);
		}

		WoodTag.Close();

		// 墊板 and 檐檁 on the front columns, under the lifted roof; ends buried in the gable walls.
		if (RoofZ > Eave) Frame::AppendEaveStack(Mesh, 0.5 * TG, W - 0.5 * TG, 0.0, P.GetColumnDiameter(), Eave);
		// The 金墊板 and 金檁 on the 金柱, open over the 廊.
		if (GoldTop > Eave + 1.0 && !P.bExposedFrame) Frame::AppendEaveStack(Mesh, 0.5 * TG, W - 0.5 * TG, FY, P.GetColumnDiameter(), GoldTop);

		// 徹上明造: the 梁架 of 圖5-3-1 on each interior column line and the 檁 over them, under the shell.
		if (P.bExposedFrame)
		{
			TArray<double> FrameX;
			for (int32 i = 1; i < N; ++i) FrameX.Add(BayBoundaryX(i));
			FRoofFrameOptions Options;
			Options.bSkipEaveLines = true;
			// Beam heads and 出頭 end in the back wall, not through it.
			Options.RearLimit = D - 1.0;
			Options.Underside = [&](double Y) { return Shell::UndersideAt(Roof, D, RoofZ, Y); };
			AppendRoofFrame(Mesh, FrameL, FrameX, 0.5 * TG, W - 0.5 * TG, Options);
		}

		AppendFacade(Mesh, P, Roof, D, RoofZ, W, TG, TF, PT, FY, Floor, Sill, WinTop, LintelBottom, CeilZ, GoldTop, N, DoorIdx, BayBoundaryX);

		// Under a 徹上明造 shell the back wall carries on up to it (the facade's own call above).
		if (Roof.ShellCover > 0.0) Shell::AppendWallToUnderside(Mesh, Roof, D, RoofZ, TG, W - TG, D - TR, D, CeilZ);

		AppendRearWindows(Mesh, P, D, TR, RearWinSill, RearWinHead, RearWins);

		// AppendGableRoof tags its own material.
		Shell::AppendGableRoof(Mesh, W, D, RoofZ, Roof);
	}

	void AppendHouseRoofRun(FDynamicMesh3& Mesh, const FHutongSiheyuanParams& P,
		double X0, double X1, bool bOpenLow, bool bOpenHigh, double RowPhase)
	{
		if (X1 - X0 <= 1.0) return;
		const double D = FMath::Max(P.Depth, 1.0);
		const double Eave = FMath::Max(P.GetEaveHeight(), 10.0) + P.GetRoofLift();
		Shell::FRoofParams Roof = MakeRoofParams(P, D);
		Roof.bOpenLowEnd = bOpenLow;
		Roof.bOpenHighEnd = bOpenHigh;
		Roof.bHasTileRowPhase = true;
		Roof.TileRowPhase = RowPhase - X0;
		const int32 V0 = Mesh.MaxVertexID();
		// The run's closed end is a corner: its 墀頭 carries the 博縫 foot and closes the eave (without it
		// the 博縫 hung in air and the soffit showed).
		if (P.bHasChitou)
		{
			HutongMeshUtils::FSlotScope PierTag(Mesh, MatSlot_Body);
			const double T = FMath::Clamp(P.GetGableWallThickness(), 1.0, FMath::Min(X1 - X0, D) * 0.2);
			Shell::AppendChitou(Mesh, X1 - X0, D, T, P.GetFloorHeight(), Eave,
				P.GetChitouProjection(), P.ChitouCorbelSteps, Roof,
				FMath::Max(P.GetBaseCourseHeight(), 0.0), FMath::Max(P.BaseCourseProjection, 0.0));
		}
		Shell::AppendGableRoof(Mesh, X1 - X0, D, Eave, Roof);
		HutongMeshUtils::TransformVerticesFrom(Mesh, V0, FTransform(FVector(X0, 0.0, 0.0)));
	}
}
