#include "Generation/CorridorGenerator.h"
#include "Generation/FrameGenerator.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongShell.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	void BuildCorridor(FDynamicMesh3& Mesh, const FHutongCorridorParams& P)
	{
		using namespace HutongMeshUtils;

		const double L = FMath::Max(P.Length, 1.0);
		const double Eave = P.GetEaveHeight();
		// Roof on the frame's line; the back wall to the ceiling under it; posts at the column top.
		const double RoofZ = Eave + P.GetRoofLift();
		const double CeilZ = RoofZ + P.GetUndersideRise();
		const double ColR = P.GetColumnRadius();
		const double Floor = FMath::Clamp(P.FloorHeight, 0.0, Eave * 0.3);
		const double FloorO = FMath::Max(P.FloorOverhang, 0.0);
		const double Walk = FMath::Max(P.Width, 20.0);
		// Separate: bClosed = colonnade behind the walk; WallT = this corridor supplies the masonry.
		const bool bClosed = P.bClosedSide;
		const double WallT = (bClosed && P.bBuildBackWall) ? FMath::Max(P.WallThickness, 1.0) : 0.0;

		// Across the rect: platform lip, open column line, walk, back wall if any.
		const double OpenY = FloorO + ColR;
		const double BackY = OpenY + Walk;
		const double Depth = OpenY + Walk + WallT + FloorO;

		const int32 Bays = P.GetBayCount(L);
		auto BoundaryX = [&](int32 i) { return P.GetBayBoundary(i, Bays, L); };

		// 1) 臺基, whole footprint.
		if (Floor > 0.0)
		{
			AppendBox(Mesh, FVector3d(0.0, 0.0, 0.0), FVector3d(L, Depth, Floor));
		}

		// 2) Back wall.
		if (WallT > 0.0)
		{
			AppendBox(Mesh,
				FVector3d(0.0, BackY, Floor),
				FVector3d(L,   BackY + WallT, CeilZ));
		}

		FSlotScope WoodTag(Mesh, MatSlot_Wood);
		const double ColTopR = Frame::TaperedTopRadius(ColR, Eave - Floor, P.ColumnTaperRatio);
		const double PT = FMath::Max(2.0 * ColR * 0.7, 6.0);  // inside the 方柱 face at its tapered top
		const double LintelBottom = Eave - 1.6 * PT;

		// 3) Open column line, plus one at the back wall when open both sides.
		auto AppendPosts = [&](double CY)
		{
			for (int32 i = 0; i <= Bays; ++i)
			{
				if ((i == 0 && P.bOmitLowEndPost) || (i == Bays && P.bOmitHighEndPost)) continue;
				Frame::AppendSquareColumn(Mesh, BoundaryX(i), CY, Frame::SquareHalfWidth(ColR), Frame::SquareHalfWidth(ColTopR), Floor, Eave);
			}
		};
		AppendPosts(OpenY);
		if (!bClosed) AppendPosts(BackY);

		// 4) 額枋 per column line, ends buried in the end posts.
		Frame::AppendArchitrave(Mesh, BoundaryX(0), BoundaryX(Bays), OpenY, PT, LintelBottom, Eave);
		if (!bClosed)
		{
			Frame::AppendArchitrave(Mesh, BoundaryX(0), BoundaryX(Bays), BackY, PT, LintelBottom, Eave);
		}

		// 倒掛楣子 under the 額枋 per bay: head rail with short drops.
		const double BarT = FMath::Max(P.FriezeBarSection, 1.0);
		if (P.bHasFrieze && P.FriezeBars > 0 && P.FriezeDrop > BarT)
		{
			const double Drop = FMath::Min(P.FriezeDrop, LintelBottom - Floor - 20.0);
			auto AppendFrieze = [&](double CY)
			{
				for (int32 b = 0; b < Bays; ++b)
				{
					Frame::AppendHangingFrieze(Mesh, BoundaryX(b), BoundaryX(b + 1), CY, LintelBottom, Drop, BarT, P.FriezeBars);
				}
			};
			AppendFrieze(OpenY);
			if (!bClosed) AppendFrieze(BackY);
		}

		// 6) 坐凳楣子 on the open side: lattice panel in the column line, seat if asked.
		if ((P.bHasBench || P.bHasBenchLattice) && Floor >= 0.0)
		{
			const double BenchZ = Floor + FMath::Max(P.BenchHeight, 10.0);
			const double BenchD = FMath::Max(P.BenchDepth, 5.0);

			const double GapC = (P.BenchGapAt >= 0.0) ? FMath::Clamp(P.BenchGapAt, 0.0, 1.0) * L : -1.0;
			const double GapH = 0.5 * FMath::Max(P.BenchGapWidth, 1.0);

			for (int32 b = 0; b < Bays; ++b)
			{
				const double X0 = BoundaryX(b), X1 = BoundaryX(b + 1);
				if (X1 - X0 < 4.0 * ColR) continue;
				if (GapC >= 0.0 && X1 > GapC - GapH && X0 < GapC + GapH) continue;
				if ((b == 0 && P.bNoBenchAtLowEnd) || (b == Bays - 1 && P.bNoBenchAtHighEnd)) continue;

				Frame::AppendBenchRail(Mesh, X0, X1, OpenY, Floor, BenchZ, BenchD,
					Frame::SquareHalfWidth(ColR), P.bHasBench, P.bHasBenchLattice);
			}
		}

		WoodTag.Close();

		// 墊板 and 檐檁 on each column line under the lifted roof.
		if (RoofZ > Eave)
		{
			// 懸山: each 檁 out to the 博縫板 (1 cm into it), a 燕尾枋 under the overhang.
			const double Reach = FMath::Max(P.GableOverhang - 0.5, 0.0);
			const double BoardH = HutongCanon::Frame::BoardHeight, PurlinD = HutongCanon::Frame::PurlinDiameter;
			Frame::AppendEaveStack(Mesh, 0.0, L, OpenY, FMath::Max(P.ColumnDiameter, 2.0), Eave, BoardH, PurlinD, Reach, true);
			if (!bClosed) Frame::AppendEaveStack(Mesh, 0.0, L, BackY, FMath::Max(P.ColumnDiameter, 2.0), Eave, BoardH, PurlinD, Reach, true);
		}

		// 7) Roof.
		Shell::FRoofParams Roof;
		Roof.FrontOverhang = FMath::Max(P.RoofOverhang, 0.0);
		Roof.RearOverhang = FMath::Max(P.RoofOverhang, 0.0);
		Roof.UndersideRise = P.GetUndersideRise();
		if (P.bExposedFrame) Roof.ShellCover = HutongCanon::Frame::RoofCover * FMath::Max(P.ColumnDiameter, 2.0);
		Roof.GableOverhang = FMath::Max(P.GableOverhang, 0.0);
		// Timber 懸山: 山花 boarded, 博縫板 with 梅花釘 down each rake, as on the 垂花門.
		Roof.bWoodenGable = true;
		Roof.BargeboardDepth = HutongCanon::Roof::BargeboardInD * FMath::Max(P.ColumnDiameter, 2.0);
		Roof.BargeboardThickness = HutongCanon::Roof::BargeboardThicknessInD * FMath::Max(P.ColumnDiameter, 2.0);
		Roof.Rise = P.GetRoofRise();
		// 三檁: one 步架 wide, so a single straight slope each side.
		Roof.Section = Jiajia::MakeSection(
			EHutongPurlins::Three, 0.5 * Depth, Roof.FrontOverhang, P.GetRoofRoll());
		Roof.SlopeSegments = 8;
		Roof.FasciaDepth = P.EaveFasciaDepth;
		Roof.FasciaWidth = P.EaveFasciaWidth;
		Roof.RafterSection = P.RafterEndSection;
		Roof.RafterSpacing = P.RafterEndSpacing;

		// Roof spans column line to column line.
		const double RoofSpan = (WallT > 0.0) ? (BackY + WallT - OpenY) : (BackY - OpenY);
		const int32 RoofFirstVert = Mesh.MaxVertexID();
		// Open colonnade bearing into its neighbours: no 山牆, no rake.
		Roof.RakeDepth = 0.0;
		Roof.bTileRuns = P.bHasTileRuns;
		Shell::AppendGableRoof(Mesh, L, RoofSpan, RoofZ, Roof);
		// 徹上明造 on every post line, under the shell. Rolled: 四檁卷棚 (圖18, 則例 卷十二) — 四架梁 with its
		// 隨梁枋, a 瓜柱 and 角背 under each 頂檁, the 月梁 across them; 檐步 0.4 and 頂步 0.2 of the depth. Sharp:
		// a 三架梁 and 脊瓜柱.
		if (P.bExposedFrame)
		{
			const double ColD = FMath::Max(P.ColumnDiameter, 2.0);
			TArray<double> FrameX;
			for (int32 i = 0; i <= Bays; ++i) FrameX.Add(BoundaryX(i));
			FRoofFrameOptions Options;
			Options.bSkipEaveLines = true;
			Options.GableReach = FMath::Max(P.GableOverhang - 0.5, 0.0);
			if (WallT > 0.0) Options.RearLimit = RoofSpan - 1.0;
			Options.Underside = [&](double Y) { return Shell::UndersideAt(Roof, RoofSpan, RoofZ, Y); };
			FrameLayout::FLayout Layout = FrameLayout::Make(ColD, Eave, Floor, RoofSpan, EHutongPurlins::Three, false, false);
			if (P.RoofApexRoll > 0.0)
			{
				// Heights come from the shell (Underside); the layout only places the lines.
				const double Eaves = 0.4 * RoofSpan;
				Layout.Y = { 0.0, Eaves, RoofSpan - Eaves, RoofSpan };
				Layout.Support = { Eave, Eave + RoofSpan, Eave + RoofSpan, Eave };
				Layout.Step = Eaves;
				Layout.Front = 0;
				Layout.Rear = 3;
				Options.bTieUnderMainBeam = true;
			}
			AppendRoofFrame(Mesh, Layout, FrameX, 0.0, L, Options);
		}
		TransformVerticesFrom(Mesh, RoofFirstVert,
			FTransform(FQuat::Identity, FVector(0.0, OpenY, 0.0)));
	}
}
