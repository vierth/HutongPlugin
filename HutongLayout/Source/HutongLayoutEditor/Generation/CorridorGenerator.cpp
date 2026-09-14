#include "Generation/CorridorGenerator.h"
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
		const double ColR = P.GetColumnRadius();
		const double Floor = FMath::Clamp(P.FloorHeight, 0.0, Eave * 0.3);
		const double FloorO = FMath::Max(P.FloorOverhang, 0.0);
		const double Walk = FMath::Max(P.Width, 20.0);
		// Two separate questions: Closed is whether there is a colonnade behind the walk, WallT whether this corridor supplies the masonry it is closed against.
		const bool bClosed = P.bClosedSide;
		const double WallT = (bClosed && P.bBuildBackWall) ? FMath::Max(P.WallThickness, 1.0) : 0.0;

		// Laid out across the rect: the platform lip, then the open column line, the walk, and the back wall if there is one.
		const double OpenY = FloorO + ColR;
		const double BackY = OpenY + Walk;
		const double Depth = OpenY + Walk + WallT + FloorO;

		const int32 Bays = P.GetBayCount(L);
		auto BoundaryX = [&](int32 i) { return P.GetBayBoundary(i, Bays, L); };

		// 1) 臺基, running the whole footprint.
		if (Floor > 0.0)
		{
			AppendBox(Mesh, FVector3d(0.0, 0.0, 0.0), FVector3d(L, Depth, Floor));
		}

		// 2) The back wall, when there is one.
		if (WallT > 0.0)
		{
			AppendBox(Mesh,
				FVector3d(0.0, BackY, Floor),
				FVector3d(L,   BackY + WallT, Eave));
		}

		// --- Woodwork ---
		const int32 WoodFirstTri = Mesh.MaxTriangleID();
		const double ColTopR = Frame::TaperedTopRadius(ColR, Eave - Floor, P.ColumnTaperRatio);
		const double PT = FMath::Max(2.0 * ColR * 0.85, 6.0);
		const double LintelBottom = Eave - 1.6 * PT;

		// 3) The open column line, and a second one against the back wall when the corridor is open on both sides.
		Frame::AppendColumnRow(Mesh, BoundaryX, Bays, OpenY, ColR, ColTopR, Floor, Eave, 1.0, 12);
		if (!bClosed)
		{
			Frame::AppendColumnRow(Mesh, BoundaryX, Bays, BackY, ColR, ColTopR, Floor, Eave, 1.0, 12);
		}

		// 4) 額枋 along each column line, ends buried in the end posts.
		Frame::AppendArchitrave(Mesh, BoundaryX(0), BoundaryX(Bays), OpenY, PT, LintelBottom, Eave);
		if (!bClosed)
		{
			Frame::AppendArchitrave(Mesh, BoundaryX(0), BoundaryX(Bays), BackY, PT, LintelBottom, Eave);
		}

		// 倒掛楣子 hanging under the 額枋 in each bay: a head rail with a row of short drops off it.
		const double BarT = FMath::Max(P.FriezeBarSection, 1.0);
		if (P.bHasFrieze && P.FriezeBars > 0 && P.FriezeDrop > BarT)
		{
			const double Drop = FMath::Min(P.FriezeDrop, LintelBottom - Floor - 20.0);
			auto AppendFrieze = [&](double CY)
			{
				if (Drop <= BarT) return;
				// The rail hangs directly off the 額枋's underside.
				const double RailTop = LintelBottom;
				for (int32 b = 0; b < Bays; ++b)
				{
					const double X0 = BoundaryX(b), X1 = BoundaryX(b + 1);
					if (X1 - X0 < 4.0 * BarT) continue;

					AppendBox(Mesh,
						FVector3d(X0, CY - 0.5 * BarT, RailTop - BarT),
						FVector3d(X1, CY + 0.5 * BarT, RailTop));

					// The drops are wider than the rail in Y.
					const int32 Count = FMath::Clamp(P.FriezeBars, 1, 24);
					for (int32 j = 1; j <= Count; ++j)
					{
						const double CX = X0 + (X1 - X0) * j / double(Count + 1);
						AppendBox(Mesh,
							FVector3d(CX - 0.5 * BarT, CY - 0.9 * BarT, RailTop - Drop),
							FVector3d(CX + 0.5 * BarT, CY + 0.9 * BarT, RailTop - 0.4 * BarT));
					}
				}
			};
			AppendFrieze(OpenY);
			if (!bClosed) AppendFrieze(BackY);
		}

		// 6) 坐凳楣子: the bench along the open side.
		if (P.bHasBench && Floor >= 0.0)
		{
			const double BenchZ = Floor + FMath::Max(P.BenchHeight, 10.0);
			const double BenchD = FMath::Max(P.BenchDepth, 5.0);
			const double Seat = FMath::Max(0.22 * BenchD, 4.0);

			// The break, if this run has one.
			const double GapC = (P.BenchGapAt >= 0.0) ? FMath::Clamp(P.BenchGapAt, 0.0, 1.0) * L : -1.0;
			const double GapH = 0.5 * FMath::Max(P.BenchGapWidth, 1.0);

			for (int32 b = 0; b < Bays; ++b)
			{
				const double X0 = BoundaryX(b), X1 = BoundaryX(b + 1);
				if (X1 - X0 < 4.0 * ColR) continue;
				if (GapC >= 0.0 && X1 > GapC - GapH && X0 < GapC + GapH) continue;
				AppendBox(Mesh,
					FVector3d(X0, OpenY, BenchZ - Seat),
					FVector3d(X1, OpenY + BenchD, BenchZ));
			}
		}

		SetMaterialIDForTrianglesFrom(Mesh, WoodFirstTri, MatSlot_Wood);

		// 7) Roof.
		Shell::FRoofParams Roof;
		Roof.FrontOverhang = FMath::Max(P.RoofOverhang, 0.0);
		Roof.RearOverhang = FMath::Max(P.RoofOverhang, 0.0);
		Roof.GableOverhang = FMath::Max(P.GableOverhang, 0.0);
		Roof.Rise = P.GetRoofRise();
		// 三檁: a 遊廊 is one 步架 wide, so its roof is a single straight slope each side.
		Roof.Section = Jiajia::MakeSection(
			EHutongPurlins::Three, 0.5 * Depth, Roof.FrontOverhang, P.RoofApexRoll);
		Roof.SlopeSegments = 8;
		Roof.FasciaDepth = P.EaveFasciaDepth;
		Roof.FasciaWidth = P.EaveFasciaWidth;
		Roof.RafterSection = P.RafterEndSection;
		Roof.RafterSpacing = P.RafterEndSpacing;

		// The roof spans column line to column line.
		const double RoofSpan = (WallT > 0.0) ? (BackY + WallT - OpenY) : (BackY - OpenY);
		const int32 RoofFirstVert = Mesh.MaxVertexID();
		Shell::AppendGableRoof(Mesh, L, RoofSpan, Eave, Roof);
		TransformVerticesFrom(Mesh, RoofFirstVert,
			FTransform(FQuat::Identity, FVector(0.0, OpenY, 0.0)));
	}
}
