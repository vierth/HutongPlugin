#include "Generation/SiheyuanGenerator.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongActorSpawn.h"
#include "Generation/HutongDoor.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongShell.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	using namespace HutongMeshUtils;

	using FBayBoundary = TFunctionRef<double(int32)>;

namespace
{
	// 舉架 over the drawn depth, with the eave overhang as its outermost run. A 廊 is one of the
	// section's 步架, never added outside it: 前廊後無廊 framed the other way is the 撅尾巴房, rear eave
	// a 金檁 high and ridge off the middle, which the 鑽金柱 frame exists to avoid (四合院建築及其構造 p.85).
	Shell::FRoofParams MakeRoofParams(const FHutongSiheyuanParams& P, double Depth)
	{
		Shell::FRoofParams Roof;
		Roof.FrontOverhang = P.GetRoofOverhang();
		Roof.RearOverhang = P.GetRearRoofOverhang();
		Roof.RearSlopeTrim = (P.RearEave == EHutongRearEave::Lane)
			? FMath::Max(Roof.FrontOverhang - Roof.RearOverhang, 0.0)
			: 0.0;
		Roof.Section = Jiajia::MakeSection(
			P.Purlins, 0.5 * Depth, Roof.FrontOverhang, P.RoofApexRoll);
		// Zero leaves the section's own rise standing, which is the canonical answer.
		Roof.Rise = P.bDeriveProportions ? 0.0 : P.GetRoofRise();
		Roof.SlopeSegments = P.RoofSlopeSegments;
		Roof.FasciaDepth = P.EaveFasciaDepth;
		Roof.FasciaWidth = P.EaveFasciaWidth;
		Roof.RafterSection = P.RafterEndSection;
		Roof.RafterSpacing = P.RafterEndSpacing;
		Roof.bFlyingRafters = P.bHasFlyingRafters;
		Roof.bHasRidgeCourse = P.bHasRidgeCourse;
		Roof.RidgeCourseHeight = P.RidgeCourseHeight;
		Roof.RidgeCourseWidth = P.RidgeCourseWidth;
		Roof.RidgeEndKick = P.RidgeEndKick;
		if (!P.bHasGableRake) Roof.RakeDepth = 0.0;
		Roof.Tile = P.RoofTile;
		Roof.TileRowSpacing = P.TileRowSpacing;
		return Roof;
	}

	// A run of wall with rectangular openings in it, laid as three bands.
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

	// One 高窗 to a bay, centred in it.
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
				const int32 PaperFirstTri = Mesh.MaxTriangleID();
				const double PaperY = OuterY - 1.4 * BarT;
				AppendBox(Mesh,
					FVector3d(X0, PaperY - 0.25 * BarT, Sill),
					FVector3d(X1, PaperY,               Head));
				SetMaterialIDForTrianglesFrom(Mesh, PaperFirstTri, MatSlot_Paper);
			}

			if (!P.bHasWindowLattice) continue;

			const int32 LatticeFirstTri = Mesh.MaxTriangleID();
			// Two uprights and one rail is the whole of it at this size; a full 支摘窗 grid in a 70 cm opening reads as a smudge and costs the same as one in a bay-wide window.
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
			SetMaterialIDForTrianglesFrom(Mesh, LatticeFirstTri, MatSlot_Lattice);
		}
	}

	// 白灰 on the three closed walls and 板壁 across the interior bay boundaries.
	void AppendInterior(FDynamicMesh3& Mesh, const FHutongSiheyuanParams& P,
		double W, double T, double Floor, double Eave, double RoomY0, double RoomY1,
		int32 N, const FBayBoundary& BayBoundaryX,
		double RearWinSill, double RearWinHead, const TArray<TPair<double, double>>& RearWins)
	{
		const double PlasterT = P.bHasInteriorPlaster
			? FMath::Clamp(P.InteriorPlasterThickness, 0.5, 0.4 * T) : 0.0;
		if (PlasterT > 0.0 && RoomY1 > RoomY0 && Eave > Floor)
		{
			const int32 PlasterFirstTri = Mesh.MaxTriangleID();
			// Sides run the room's full depth and the back sits between them, the same tiling the 下鹼 uses so no two of the three meet at a shared plane.
			AppendBox(Mesh, FVector3d(T, RoomY0, Floor),
				FVector3d(T + PlasterT, RoomY1, Eave));
			AppendBox(Mesh, FVector3d(W - T - PlasterT, RoomY0, Floor),
				FVector3d(W - T, RoomY1, Eave));
			if (W - 2.0 * (T + PlasterT) > 0.0)
			{
				// Laid to the same splits as the brickwork it skims, or the lime seals the 高窗 the wall just left.
				AppendPiercedWall(Mesh, T + PlasterT, W - T - PlasterT,
					RoomY1 - PlasterT, RoomY1, Floor, Eave,
					RearWinSill, RearWinHead, RearWins);
			}
			SetMaterialIDForTrianglesFrom(Mesh, PlasterFirstTri, MatSlot_Plaster);
		}

		// 板壁 is joinery, not brickwork: the frame carries the roof (牆倒屋不塌), so a partition has no load to take.
		if (!P.bHasInteriorPartitions || N <= 1 || RoomY1 <= RoomY0)
		{
			return;
		}

		const int32 PartFirstTri = Mesh.MaxTriangleID();
		const double HalfT = 0.5 * FMath::Clamp(P.InteriorPartitionThickness, 2.0, T);
		const double DoorW = FMath::Clamp(P.InteriorDoorWidth, 0.0, RoomY1 - RoomY0);
		const double HeadZ = FMath::Min(
			FMath::Max(P.GetDoorLeafTopHeight(), Passage::MinHeadZ(Floor, 0.0)), Eave);
		// Centred in the depth: the 碧紗櫥 this stands in for is symmetrical about the room's axis.
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
		SetMaterialIDForTrianglesFrom(Mesh, PartFirstTri, MatSlot_Partition);
	}

	// 窗紙 and 支摘窗 in every bay but the door's.
	void AppendBayWindows(FDynamicMesh3& Mesh, const FHutongSiheyuanParams& P,
		double W, double T, double PT, double FY, double Sill, double WinTop,
		int32 N, int32 DoorIdx, const FBayBoundary& BayBoundaryX)
	{
		const double BarT = FMath::Clamp(P.WindowLatticeThickness, 1.0, T * 0.5);

		// 抱框 at every interior bay boundary, full wall depth.
		if (WinTop > Sill && N > 1)
		{
			const int32 PostFirstTri = Mesh.MaxTriangleID();
			for (int32 i = 1; i < N; ++i)
			{
				const double B = BayBoundaryX(i);
				AppendBox(Mesh,
					FVector3d(B - 0.5 * PT, FY,     Sill),
					FVector3d(B + 0.5 * PT, FY + T, WinTop));
			}
			SetMaterialIDForTrianglesFrom(Mesh, PostFirstTri, MatSlot_Wood);
		}

		for (int32 i = 0; i < N; ++i)
		{
			if (P.bHasFrontDoorCenter && i == DoorIdx)
			{
				continue;   // the opening runs from the floor to the lintel
			}

			const double LeftPostRight = (i == 0)     ? T       : (BayBoundaryX(i) + 0.5 * PT);
			const double RightPostLeft = (i == N - 1) ? (W - T) : (BayBoundaryX(i + 1) - 0.5 * PT);
			const double OpenW = RightPostLeft - LeftPostRight;
			const double OpenH = WinTop - Sill;
			if (OpenW <= BarT || OpenH <= BarT) continue;

			if (P.bHasWindowPaper)
			{
				const int32 PaperFirstTri = Mesh.MaxTriangleID();
				const double PaperY = FY + 0.2 * T + 1.4 * BarT;
				AppendBox(Mesh,
					FVector3d(LeftPostRight, PaperY,               Sill),
					FVector3d(RightPostLeft, PaperY + 0.25 * BarT, WinTop));
				SetMaterialIDForTrianglesFrom(Mesh, PaperFirstTri, MatSlot_Paper);
			}

			if (!P.bHasWindowLattice) continue;

			const int32 LatticeFirstTri = Mesh.MaxTriangleID();
			const double MullionY0 = FY + 0.2 * T;
			const double RailY0 = MullionY0 + 0.4 * BarT;

			const int32 Mullions = FMath::Clamp(P.WindowMullions, 0, 24);
			for (int32 j = 1; j <= Mullions; ++j)
			{
				const double CX = LeftPostRight + OpenW * j / double(Mullions + 1);
				AppendBox(Mesh,
					FVector3d(CX - 0.5 * BarT, MullionY0,        Sill),
					FVector3d(CX + 0.5 * BarT, MullionY0 + BarT, WinTop));
			}

			const int32 Rails = FMath::Clamp(P.WindowRails, 0, 16);
			for (int32 j = 1; j <= Rails; ++j)
			{
				const double CZ = Sill + OpenH * j / double(Rails + 1);
				AppendBox(Mesh,
					FVector3d(LeftPostRight, RailY0,        CZ - 0.5 * BarT),
					FVector3d(RightPostLeft, RailY0 + BarT, CZ + 0.5 * BarT));
			}

			SetMaterialIDForTrianglesFrom(Mesh, LatticeFirstTri, MatSlot_Lattice);
		}
	}

	// 抱框: jambs, head, 門檻, leaves, fixed flanking panels and a latticed transom.
	void AppendDoorFrame(FDynamicMesh3& Mesh, const FHutongSiheyuanParams& P,
		double FY, double T, double Floor, double Sill, double LintelBottom, double ColR,
		double DoorX0, double DoorX1)
	{
		const double BayOpenW = DoorX1 - DoorX0;
		const double FrameT = FMath::Clamp(P.DoorFrameThickness, 2.0, FMath::Max(T, 2.0));
		const double DoorW = BayOpenW * FMath::Clamp(P.DoorWidthFraction, 0.15, 1.0);
		const double JambL = DoorX0 + 0.5 * (BayOpenW - DoorW);
		const double JambR = JambL + DoorW;

		const double LeafTop = FMath::Clamp(P.GetDoorLeafTopHeight(),
			Floor + 50.0, FMath::Max(Floor + 60.0, LintelBottom - FrameT));
		const double BarT = FMath::Clamp(P.WindowLatticeThickness, 1.0, T * 0.5);

		const int32 DoorFirstTri = Mesh.MaxTriangleID();

		// The flanking panels are woodwork, not brick.
		if (Sill > Floor)
		{
			if (JambL - FrameT > DoorX0)
			{
				AppendBox(Mesh, FVector3d(DoorX0, FY, Floor),
					FVector3d(JambL - FrameT, FY + T, Sill));
			}
			if (DoorX1 > JambR + FrameT)
			{
				AppendBox(Mesh, FVector3d(JambR + FrameT, FY, Floor),
					FVector3d(DoorX1, FY + T, Sill));
			}
		}

		// Swing clearance is the run from the jamb to the flanking column.
		FHutongDoorAssembly Door;
		Door.OpeningX0 = JambL;
		Door.OpeningX1 = JambR;
		Door.FrontY = FY;
		Door.BackY = FY + T;
		Door.BottomZ = Floor;
		Door.LeafTopZ = LeafTop;
		Door.JambTopZ = LintelBottom;
		Door.FrameThickness = FrameT;
		Door.ThresholdHeight = P.DoorThresholdHeight;
		Door.bLeavesOpen = P.bDoorLeavesOpen;
		Door.SwingClearance = FMath::Max(
			FMath::Min(JambL - (DoorX0 + ColR), (DoorX1 - ColR) - JambR), 0.0);

		int32 LeafFirstTri = MAX_int32;
		AppendDoorAssembly(Mesh, Door, &LeafFirstTri);
		const int32 LeafEndTri = Mesh.MaxTriangleID();

		// Transom over the door and matching bars in the flanking panels, so the bay reads as glazing above a solid base.
		const int32 Mullions = FMath::Clamp(P.DoorTransomMullions, 0, 24);
		const double MullionY0 = FY + 0.2 * T;

		auto AppendBars = [&](double X0, double X1, double Z0, double Z1)
		{
			if (X1 - X0 < 2.0 * BarT || Z1 - Z0 < BarT || Mullions <= 0) return;
			for (int32 j = 1; j <= Mullions; ++j)
			{
				const double CX = X0 + (X1 - X0) * j / double(Mullions + 1);
				AppendBox(Mesh,
					FVector3d(CX - 0.5 * BarT, MullionY0,        Z0),
					FVector3d(CX + 0.5 * BarT, MullionY0 + BarT, Z1));
			}
		};

		const int32 BarsFirstTri = Mesh.MaxTriangleID();
		AppendBars(JambL, JambR, LeafTop + FrameT, LintelBottom);
		AppendBars(DoorX0, JambL - FrameT, FMath::Max(Sill, Floor), LintelBottom);
		AppendBars(JambR + FrameT, DoorX1, FMath::Max(Sill, Floor), LintelBottom);

		SetMaterialIDForTrianglesFrom(Mesh, DoorFirstTri, MatSlot_Wood);
		// 門漆 over the frame timber, bounded at both ends.
		SetMaterialIDForTriangleRange(Mesh, LeafFirstTri, LeafEndTri, MatSlot_DoorPaint);
		SetMaterialIDForTrianglesFrom(Mesh, BarsFirstTri, MatSlot_Lattice);
	}
}

	void BuildSiheyuan(FDynamicMesh3& Mesh, const FHutongSiheyuanParams& P)
	{

		const double W = FMath::Max(P.Width, 1.0);
		const double D = FMath::Max(P.Depth, 1.0);
		// GetEaveHeight, not the raw field.
		const double Eave = FMath::Max(P.GetEaveHeight(), 10.0);
		const double Floor = FMath::Clamp(P.GetFloorHeight(), 0.0, Eave * 0.5);
		const double T = FMath::Clamp(P.WallThickness, 1.0, FMath::Min(W, D) * 0.2);
		const double PT = FMath::Clamp(P.PostThickness, 1.0, T * 1.5);
		const double LintelBottom = FMath::Clamp(P.GetDoorTopHeight(), Floor + 10.0, Eave - 10.0);
		const double Sill = FMath::Clamp(P.GetWindowSillHeight(), Floor, LintelBottom - 20.0);
		const double WinTop = FMath::Clamp(P.GetWindowTopHeight(), Sill + 10.0, LintelBottom);

		const int32 N = (P.BayCountOverride > 0)
			? P.BayCountOverride
			: ComputeBayCount(W, P.MinBayWidth, P.MaxBayWidth);
		const int32 DoorIdx = P.GetDoorBayIndex(N);

		const double ColR = P.GetColumnRadius();

		// Bays are not evenly spaced — 明間 is wider than the 次間 either side.
		auto BayBoundaryX = [&](int32 i) { return P.GetBayBoundary(i, N, W, ColR); };

		// 前廊: the wall retreats by one 廊步 while the 檐柱 stay on the footprint edge. 後廊: the back
		// wall stays on the 後檐柱 line and the rear 金柱 stand one 廊步 inside it.
		double FY, RY;
		P.GetBuiltVerandaDepths(D, T, FY, RY);
		const bool bVeranda = (FY > 0.0);

		const double BaseH = FMath::Clamp(P.GetBaseCourseHeight(), 0.0, (Eave - Floor) * 0.6);
		const double BaseP = FMath::Max(P.BaseCourseProjection, 0.0);
		const double WallBottom = (BaseH > 0.0 && BaseP > 0.0) ? Floor + BaseH : Floor;

		// 臺基, projecting in front of the facade only, with the 踏跺 under the door rather than the middle.
		const double PlatO = FMath::Max(P.PlatformOverhang, ColR);
		const double PlatSide = 0.0;
		Shell::AppendPlatform(Mesh, W, D, Floor, PlatO, PlatSide,
			P.bHasFrontDoorCenter ? P.StepCount : 0, P.StepTread,
			BayBoundaryX(DoorIdx), BayBoundaryX(DoorIdx + 1));

		// 散水 on the platform's own rectangle, so the band starts where the masonry meets the ground.
		if (P.Apron.bEnabled)
		{
			Shell::AppendApron(Mesh, 0.0, -PlatO, W, D,
				P.Apron.GetWidth(P.GetRoofOverhang()),
				P.Apron.GetWidth(P.GetRearRoofOverhang()),
				/*SideWidth*/ 0.0,
				P.Apron.Thickness);
		}

		// 廊門筒子: the gable walls open across the 前廊 between its two columns, head at the least
		// clear height a doorway takes.
		double EndDoorY0 = 0.0, EndDoorY1 = 0.0;
		const double EndDoorHead = HutongGen::Passage::MinHeadZ(Floor, 0.0);
		if (P.bHasVerandaEndDoorways && bVeranda && EndDoorHead < Eave - 20.0)
		{
			// Column face to column face: a 前出廊 wing's 廊步 is under a metre, and any margin
			// leaves a slot rather than a way through.
			EndDoorY0 = ColR + 1.0;
			EndDoorY1 = FY - ColR - 1.0;
			if (EndDoorY1 - EndDoorY0 < 50.0) EndDoorY0 = EndDoorY1 = 0.0;
		}

		// 下鹼 round three sides; the front stays flush or it would foul the facade columns.
		{
			const int32 BaseFirstTri = Mesh.MaxTriangleID();
			Shell::AppendBaseCourseU(Mesh, W, D, T, Floor, BaseH, BaseP,
				/*bIncludeRear*/ true, /*bToGround*/ true, EndDoorY0, EndDoorY1);
			SetMaterialIDForTrianglesFrom(Mesh, BaseFirstTri, MatSlot_BaseCourse);
		}

		// 高窗 up under the eave of the 封護檐 back wall, one to a bay.
		TArray<TPair<double, double>> RearWins;
		double RearWinSill = 0.0, RearWinHead = 0.0;
		if (P.bHasRearHighWindows)
		{
			RearWinHead = Eave - FMath::Clamp(P.RearWindowHeadDrop, 5.0, (Eave - WallBottom) * 0.6);
			RearWinSill = RearWinHead
				- FMath::Clamp(P.RearWindowHeight, 10.0, FMath::Max(RearWinHead - WallBottom - 40.0, 10.0));
			if (RearWinSill > WallBottom + 30.0)
			{
				RearWins = RearWindowSpans(P, W, T, N, BayBoundaryX);
			}
		}

		AppendPiercedWall(Mesh, T, W - T, D - T, D, WallBottom, Eave,
			RearWinSill, RearWinHead, RearWins);

		const Shell::FRoofParams Roof = MakeRoofParams(P, D);

		// 封護檐: the rear slope terminates over the wall, and because a slope descends outward, terminating earlier means terminating higher.
		const double RearLift = Shell::RearEaveLift(Roof, D);
		if (RearLift > 0.0)
		{
			const double BandP = FMath::Max(PlatSide, 2.0);
			AppendBox(Mesh,
				FVector3d(-BandP,    D - T,     Eave),
				FVector3d(W + BandP, D + BandP, Eave + RearLift));
		}

		for (const double X0 : { 0.0, W - T })
		{
			if (EndDoorY1 > EndDoorY0)
			{
				AppendBox(Mesh, FVector3d(X0, 0.0, WallBottom), FVector3d(X0 + T, EndDoorY0, Eave));
				AppendBox(Mesh, FVector3d(X0, EndDoorY0, EndDoorHead), FVector3d(X0 + T, EndDoorY1, Eave));
				AppendBox(Mesh, FVector3d(X0, EndDoorY1, WallBottom), FVector3d(X0 + T, D, Eave));
			}
			else
			{
				AppendBox(Mesh, FVector3d(X0, 0.0, WallBottom), FVector3d(X0 + T, D, Eave));
			}
		}

		AppendInterior(Mesh, P, W, T, Floor, Eave, /*RoomY0*/ FY + T, /*RoomY1*/ D - T,
			N, BayBoundaryX, RearWinSill, RearWinHead, RearWins);

		if (P.bHasChitou)
		{
			Shell::AppendChitou(Mesh, W, T, Floor, Eave,
				P.GetChitouProjection(), P.ChitouCorbelSteps);
		}

		// Lintel, columns and 額枋 are one wood range.
		const int32 WoodFirstTri = Mesh.MaxTriangleID();
		if (P.bHasFrontDoorCenter)
		{
			AppendBox(Mesh,
				FVector3d(BayBoundaryX(DoorIdx),     FY,     LintelBottom),
				FVector3d(BayBoundaryX(DoorIdx + 1), FY + T, Eave));
		}

		// 收分 against 柱高, not the drawn height.
		const double ColOvershoot = 1.0;
		const double ColTopR = Frame::TaperedTopRadius(ColR, P.GetColumnHeight(), P.ColumnTaperRatio);

		// 金柱 in the wall plane.
		Frame::AppendColumnRow(Mesh, BayBoundaryX, N, FY, ColR, ColTopR, 0.0, Eave, ColOvershoot);

		if (bVeranda)
		{
			// 檐柱 free-standing on the platform, carrying the eave, with the 額枋 along them and a 穿插枋 tying each back to its 金柱.
			Frame::AppendColumnRow(Mesh, BayBoundaryX, N, 0.0, ColR, ColTopR, 0.0, Eave, ColOvershoot);
			Frame::AppendArchitrave(Mesh, BayBoundaryX(0), BayBoundaryX(N), 0.0, PT,
				LintelBottom, Eave);
			Frame::AppendTieBeams(Mesh, BayBoundaryX, N, 0.0, FY + 0.5 * T, PT,
				LintelBottom, Eave, ColOvershoot);
		}

		if (RY > 0.0)
		{
			// 後金柱 free-standing in the room, tied back to the 後檐柱 buried in the wall.
			Frame::AppendColumnRow(Mesh, BayBoundaryX, N, D - RY, ColR, ColTopR, 0.0, Eave, ColOvershoot);
			Frame::AppendTieBeams(Mesh, BayBoundaryX, N, D - RY, D - 0.5 * T, PT,
				LintelBottom, Eave, ColOvershoot);
		}

		SetMaterialIDForTrianglesFrom(Mesh, WoodFirstTri, MatSlot_Wood);

		// The 檻牆 and the header above the windows are each one box per run of bays, passing behind the intermediate columns.
		auto AppendInfillRun = [&](int32 FirstBay, int32 LastBay)
		{
			if (LastBay < FirstBay) return;

			const double X0 = (FirstBay == 0)    ? T       : BayBoundaryX(FirstBay);
			const double X1 = (LastBay == N - 1) ? (W - T) : BayBoundaryX(LastBay + 1);
			if (X1 <= X0) return;

			if (Sill > Floor)
			{
				AppendBox(Mesh, FVector3d(X0, FY, Floor), FVector3d(X1, FY + T, Sill));
			}
			if (Eave > WinTop)
			{
				AppendBox(Mesh, FVector3d(X0, FY, WinTop), FVector3d(X1, FY + T, Eave));
			}
		};

		if (P.bHasFrontDoorCenter)
		{
			AppendInfillRun(0, DoorIdx - 1);
			AppendInfillRun(DoorIdx + 1, N - 1);
		}
		else
		{
			AppendInfillRun(0, N - 1);
		}

		AppendBayWindows(Mesh, P, W, T, PT, FY, Sill, WinTop, N, DoorIdx, BayBoundaryX);

		AppendRearWindows(Mesh, P, D, T, RearWinSill, RearWinHead, RearWins);

		if (P.bHasFrontDoorCenter && P.bHasDoorFrame)
		{
			AppendDoorFrame(Mesh, P, FY, T, Floor, Sill, LintelBottom, ColR,
				BayBoundaryX(DoorIdx), BayBoundaryX(DoorIdx + 1));
		}

		// AppendGableRoof owns its own material tagging, so the roof cannot come out brick-coloured.
		Shell::AppendGableRoof(Mesh, W, D, Eave, Roof);
	}
}
