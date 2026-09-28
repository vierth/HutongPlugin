#include "Generation/WallGenerator.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongActorSpawn.h"
#include "Generation/HutongDoor.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongShell.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	namespace
	{
		using namespace HutongMeshUtils;

		// 什錦窗 moulded surround and 欞條, after the masonry.
		void AppendWindowTrim(FDynamicMesh3& Mesh, const FHutongWallParams& P, double Cx,
			double WinHalf, double WinOuter, double WinSurW, double WinSurP,
			double WinCz, int32 WinSteps, double T)
		{
			// Ring following the outline, same walk as the masonry so both step round the shape together.
			if (WinSurW > 0.0)
			{
				FSlotScope SurTag(Mesh, MatSlot_BaseCourse);
				const double Lap = FMath::Min(2.0, 0.25 * WinSurW);
				for (int32 r = 0; r < WinSteps * 2; ++r)
				{
					const double Za = WinCz - WinOuter + 2.0 * WinOuter * r / double(WinSteps * 2);
					const double Zb = WinCz - WinOuter + 2.0 * WinOuter * (r + 1) / double(WinSteps * 2);
					// At the row's widest point, not its midpoint.
					const double Zw = FMath::Clamp(WinCz, Za, Zb);

					const double Outer = P.GetWindowHalfWidthFraction((Zw - WinCz) / WinOuter) * WinOuter;
					if (Outer <= 0.0) continue;

					const double InnerRaw = (FMath::Abs(Zw - WinCz) < WinHalf)
						? P.GetWindowHalfWidthFraction((Zw - WinCz) / WinHalf) * WinHalf - Lap
						: 0.0;
					const double Inner = FMath::Max(InnerRaw, 0.0);

					if (Inner <= 0.0)
					{
						AppendBox(Mesh, FVector3d(Cx - Outer, -WinSurP, Za),
							FVector3d(Cx + Outer, T + WinSurP, Zb));
					}
					else
					{
						AppendBox(Mesh, FVector3d(Cx - Outer, -WinSurP, Za),
							FVector3d(Cx - Inner, T + WinSurP, Zb));
						AppendBox(Mesh, FVector3d(Cx + Inner, -WinSurP, Za),
							FVector3d(Cx + Outer, T + WinSurP, Zb));
					}
				}
				// Dressed brick like the 下鹼.
				SurTag.Close();
			}

			// 欞條 span the opening's bounding square, buried in masonry outside the shape.
			const int32 Bars = FMath::Clamp(P.WindowLatticeBars, 0, 8);
			if (Bars <= 0) return;

			FSlotScope BarTag(Mesh, MatSlot_Lattice);
			const double BarT = FMath::Clamp(P.WindowLatticeThickness, 1.0, 0.5 * T);
			const double BarY = 0.5 * (T - BarT);
			for (int32 b = 1; b <= Bars; ++b)
			{
				const double F = b / double(Bars + 1);
				const double X = Cx - WinHalf + 2.0 * WinHalf * F;
				AppendBox(Mesh, FVector3d(X - 0.5 * BarT, BarY, WinCz - WinHalf),
					FVector3d(X + 0.5 * BarT, BarY + BarT, WinCz + WinHalf));
				// Crossing bars a little deeper.
				const double Z = WinCz - WinHalf + 2.0 * WinHalf * F;
				AppendBox(Mesh, FVector3d(Cx - WinHalf, BarY - 0.45 * BarT, Z - 0.5 * BarT),
					FVector3d(Cx + WinHalf, BarY + 0.55 * BarT, Z + 0.5 * BarT));
			}
			BarTag.Close();
		}

		// 牆垣式垂花門: 垂花門 ornament on a frameless doorway; the wall is the frame.
		void AppendDoorwayDressing(FDynamicMesh3& Mesh, const FHutongWallParams& P,
			double Cx, double T, double BandZ0, double BandZ1)
		{
			const double HalfW = 0.5 * P.GetDoorwayWidth();
			const double PostR = FMath::Max(0.5 * P.DoorwayPostDiameter, 2.0);
			const double SurP = FMath::Max(P.DoorwaySurroundProjection, 0.0);
			const double CapO = FMath::Max(P.GetCapOverhang(), 0.0);

			// Proud of the surround (else coplanar) and under the cap overhang, in the eave's shadow.
			const double Proj = FMath::Max(
				FMath::Min(FMath::Max(P.DoorwayDressProjection, 1.0), FMath::Max(CapO, 1.0)),
				SurP + 1.0);

			const double Band = BandZ1 - BandZ0;
			const double BeamH = FMath::Clamp(0.45 * Band, 8.0, 26.0);
			const double BeamTop = BandZ0 + BeamH;
			// Beam overhangs far enough for a post off each end to clear the reveal.
			const double BeamHalf = HalfW + FMath::Max(P.DoorwaySurroundWidth, 0.0) + 2.0 * PostR;

			FSlotScope PaintTag(Mesh, MatSlot_Paint);

			AppendBox(Mesh,
				FVector3d(Cx - BeamHalf, -Proj,    BandZ0),
				FVector3d(Cx + BeamHalf, T + Proj, BeamTop));

			// 花板 narrower in Y than its beam.
			if (BandZ1 - 1.0 > BeamTop)
			{
				AppendBox(Mesh,
					FVector3d(Cx - BeamHalf, -0.55 * Proj,     BeamTop),
					FVector3d(Cx + BeamHalf, T + 0.55 * Proj,  BandZ1 - 1.0));
			}

			// 雀替 in the opening's top corners, stepped, not scrolled (as the shopfront's 花牙子).
			const double Reach = FMath::Min(0.22 * (2.0 * HalfW), 22.0);
			const int32 Steps = 3;
			// Brackets run from the wall face to short of the beam's outer face.
			const TPair<double, double> Faces[] = { { -0.7 * Proj, 0.0 }, { T, T + 0.7 * Proj } };
			for (const TPair<double, double>& Face : Faces)
			{
				for (int32 s = 0; s < Steps; ++s)
				{
					const double A = Reach * (Steps - s) / double(Steps);
					const double Drop = 0.5 * Reach * (s + 1) / double(Steps);
					AppendBox(Mesh, FVector3d(Cx - HalfW,     Face.Key, BandZ0 - Drop),
						FVector3d(Cx - HalfW + A, Face.Value, BandZ0));
					AppendBox(Mesh, FVector3d(Cx + HalfW - A, Face.Key, BandZ0 - Drop),
						FVector3d(Cx + HalfW,     Face.Value, BandZ0));
				}
			}

			PaintTag.Close();

			// 垂蓮柱 off each beam end, one per face.
			FSlotScope PostTag(Mesh, MatSlot_Wood);
			const double Drop = FMath::Max(P.DoorwayPostDrop, 4.0 * PostR);
			for (double PostX : { Cx - BeamHalf + PostR + 1.0, Cx + BeamHalf - PostR - 1.0 })
			{
				Frame::AppendHangingPost(Mesh, PostX, -Proj + PostR, BandZ0, Drop, PostR, 0.45);
				Frame::AppendHangingPost(Mesh, PostX, T + Proj - PostR, BandZ0, Drop, PostR, 0.45);
			}
			PostTag.Close();
		}

		// 月亮門 and kin: shaped opening walked through, not looked through.
		void AppendGardenDoorway(FDynamicMesh3& Mesh, const FHutongWallParams& P,
			double Cx, double T, double H, double BaseH, double BaseP)
		{
			const double HalfW = 0.5 * P.GetDoorwayWidth();
			const double Top = P.GetDoorwayHeight();
			const double Sill = FMath::Clamp(P.DoorwaySillHeight, 0.0, 40.0);
			const double SurW = FMath::Max(P.DoorwaySurroundWidth, 0.0);
			const double SurP = FMath::Max(P.DoorwaySurroundProjection, 0.0);
			const int32 Steps = FMath::Clamp(P.DoorwayOutlineSteps, 3, 48);
			const double X0 = Cx - HalfW;
			const double X1 = Cx + HalfW;

			// Outline at a height, and the same grown by the surround.
			auto ShapeHalf = [&](double Z, double Half, double Height, double Bottom)
			{
				const double t = 2.0 * (Z - Bottom) / FMath::Max(Height, 1.0) - 1.0;
				return P.GetDoorwayHalfWidthFraction(t) * Half;
			};
			// Outline continues below ground by the bury so the sill cut is walkable.
			const double Bury = P.GetDoorwayBury();
			auto OpeningHalf = [&](double Z) { return ShapeHalf(Z, HalfW, Top + Bury, -Bury); };
			auto OuterHalf = [&](double Z) { return ShapeHalf(Z, HalfW + SurW, Top + Bury + 2.0 * SurW, -Bury - SurW); };

			// Outline's widest point across a row, which masonry and surround must clear.
			auto WidestIn = [&](double Za, double Zb)
			{
				const double Bulge = FMath::Clamp(0.5 * Top, Za, Zb);
				return FMath::Max3(OpeningHalf(Za), OpeningHalf(Zb), OpeningHalf(Bulge));
			};

			// A band of courses, each stepped to its row's widest point so none intrudes on the shape.
			auto Rows = [&](double Za, double Zb, double Proj)
			{
				if (Zb - Za <= 0.0) return;
				const int32 N = FMath::Max(FMath::RoundToInt32(Steps * (Zb - Za) / Top), 1);
				for (int32 r = 0; r < N; ++r)
				{
					const double A = Za + (Zb - Za) * r / double(N);
					const double B = Za + (Zb - Za) * (r + 1) / double(N);
					const double Hw = WidestIn(A, B);
					AppendBox(Mesh, FVector3d(X0, -Proj, A), FVector3d(Cx - Hw, T + Proj, B));
					AppendBox(Mesh, FVector3d(Cx + Hw, -Proj, A), FVector3d(X1, T + Proj, B));
				}
			};

			// 下鹼 runs to the opening edge, stepping round the shape too.
			const double BaseTop = FMath::Clamp(BaseH, Sill, Top);
			// Flush is not absent: with no projection there is no separate course, and the body must take the
			// band back or the masonry beside the opening is a hole.
			const bool bProudCourse = BaseP > 0.0 && BaseTop > Sill;
			if (bProudCourse)
			{
				const int32 First = Mesh.MaxTriangleID();
				Rows(Sill, BaseTop, BaseP);
				SetMaterialIDForTriangleRange(Mesh, First, Mesh.MaxTriangleID(), MatSlot_BaseCourse);
			}
			Rows(bProudCourse ? BaseTop : Sill, Top, 0.0);

			// Solid over the head to the wall top.
			if (H > Top)
			{
				AppendBox(Mesh, FVector3d(X0, 0.0, Top), FVector3d(X1, T, H));
			}

			// Stone sill the shape is cut at, stepped over like every 門檻.
			if (Sill > 0.0)
			{
				const int32 First = Mesh.MaxTriangleID();
				AppendBox(Mesh, FVector3d(X0, -BaseP, 0.0), FVector3d(X1, T + BaseP, Sill));
				SetMaterialIDForTriangleRange(Mesh, First, Mesh.MaxTriangleID(), MatSlot_Stone);
			}

			// Moulded surround, lapping the reveal.
			if (SurW <= 0.0) return;

			const int32 SurFirstTri = Mesh.MaxTriangleID();
			const double Lap = FMath::Min(2.0, 0.25 * SurW);
			const int32 N = Steps * 2;
			const double SurZ1 = Top + SurW;
			for (int32 r = 0; r < N; ++r)
			{
				const double Za = Sill + (SurZ1 - Sill) * r / double(N);
				const double Zb = Sill + (SurZ1 - Sill) * (r + 1) / double(N);

				// Both edges at the row's widest point, as the masonry.
				const double Outer = FMath::Max(OuterHalf(Za), OuterHalf(Zb));
				if (Outer <= 0.0) continue;

				const double Inner = (Za < Top) ? FMath::Max(WidestIn(Za, Zb) - Lap, 0.0) : 0.0;
				if (Inner <= 0.0)
				{
					AppendBox(Mesh, FVector3d(Cx - Outer, -SurP, Za), FVector3d(Cx + Outer, T + SurP, Zb));
				}
				else
				{
					AppendBox(Mesh, FVector3d(Cx - Outer, -SurP, Za), FVector3d(Cx - Inner, T + SurP, Zb));
					AppendBox(Mesh, FVector3d(Cx + Inner, -SurP, Za), FVector3d(Cx + Outer, T + SurP, Zb));
				}
			}
			// Dressed brick, like the 下鹼 and window surround.
			SetMaterialIDForTriangleRange(Mesh, SurFirstTri, Mesh.MaxTriangleID(), MatSlot_BaseCourse);
		}
	}

	void BuildWall(FDynamicMesh3& Mesh, const FHutongWallParams& P)
	{
		using namespace HutongMeshUtils;

		const double L = FMath::Max(P.Length, 1.0);
		const double T = P.GetThickness();
		const double H = FMath::Max(P.GetHeight(), 1.0);

		// Real run extent: drawn rect plus each end's miter into its neighbour.
		const double RunX0 = -FMath::Clamp(P.StartExtend, 0.0, 3.0 * T);
		const double RunX1 = L + FMath::Clamp(P.EndExtend, 0.0, 3.0 * T);

		const double BaseH = FMath::Clamp(P.GetBaseCourseHeight(), 0.0, H * 0.6);
		const double BaseP = FMath::Max(P.BaseCourseProjection, 0.0);
		const bool bBaseCourse = (BaseH > 0.0 && BaseP > 0.0);

		// Gate first: all masonry below stops at it.
		const double GateFrameT = FMath::Clamp(P.GateFrameThickness, 2.0, T);
		const double GateW = FMath::Clamp(P.GateWidth, 40.0, FMath::Max(L - 4.0 * GateFrameT, 40.0));
		const double GateHead = FMath::Clamp(P.GetGateHeadHeight(), 80.0, H - 10.0);

		const double GateCentre = FMath::Clamp(
			P.GatePosition * L,
			0.5 * GateW + GateFrameT,
			L - 0.5 * GateW - GateFrameT);

		const double ClearX0 = GateCentre - 0.5 * GateW;
		const double ClearX1 = GateCentre + 0.5 * GateW;
		const double MasonryX0 = ClearX0 - GateFrameT;
		const double MasonryX1 = ClearX1 + GateFrameT;

		// Head on the jambs; masonry above continues to the wall top.
		const double JambTop = FMath::Min(GateHead + GateFrameT, H);

		const bool bGate = P.bHasGate && (MasonryX1 - MasonryX0) > 1.0 && MasonryX0 > 0.0 && MasonryX1 < L;

		// Garden doorway, same approach.
		const double DoorHalfW = 0.5 * P.GetDoorwayWidth();
		const double DoorMargin = DoorHalfW + FMath::Max(P.DoorwaySurroundWidth, 0.0);
		const double DoorCentre = P.GetDoorwayCentre(L);
		const double DoorX0 = DoorCentre - DoorHalfW;
		const double DoorX1 = DoorCentre + DoorHalfW;

		// Dropped, not overlapped, where a 牆垣式門 would collide with the gate.
		const bool bDoorwayClearOfGate = !bGate
			|| DoorX1 + DoorMargin - DoorHalfW < MasonryX0
			|| DoorX0 - DoorMargin + DoorHalfW > MasonryX1;
		const bool bDoorway = P.HasDecorativeDoorway(L) && bDoorwayClearOfGate
			&& DoorX0 > RunX0 && DoorX1 < RunX1;

		// No 散水 on a wall: a brick cap sheds a trickle, not a roof's worth.

		// 什錦窗, alongside the gate.
		const double WinHalf = 0.5 * FMath::Max(P.WindowSize, 10.0);
		const double WinSurW = FMath::Max(P.WindowSurroundWidth, 0.0);
		const double WinSurP = FMath::Max(P.WindowSurroundProjection, 0.0);
		const double WinOuter = WinHalf + WinSurW;
		const double WinCz = P.WindowCentreHeight;
		const int32 WinSteps = FMath::Clamp(P.WindowOutlineSteps, 2, 32);

		TArray<double> WindowX;
		{
			const int32 Count = P.GetWindowCount(L);
			// Clear of the wall top and the 下鹼.
			const bool bFits = (WinCz - WinOuter > BaseH + 4.0) && (WinCz + WinOuter < H - 8.0);
			for (int32 i = 0; i < (bFits ? Count : 0); ++i)
			{
				const double Cx = (i + 0.5) * L / double(Count);
				// Never through the gate or its piers.
				if (bGate)
				{
					const double Keep = FMath::Max(P.GatePierWidth, P.GateWingWidth) + WinOuter;
					if (Cx > MasonryX0 - Keep && Cx < MasonryX1 + Keep) continue;
				}
				// Nor the garden doorway.
				if (bDoorway)
				{
					const double Keep = WinOuter + FMath::Max(P.DoorwaySurroundWidth, 0.0);
					if (Cx > DoorX0 - Keep && Cx < DoorX1 + Keep) continue;
				}
				if (Cx - WinOuter <= RunX0 || Cx + WinOuter >= RunX1) continue;
				WindowX.Add(Cx);
			}
		}

		// Wall body Z0..Z1, cut for windows in the span.
		auto AppendBodySpan = [&](double X0, double X1, double Z0, double Z1)
		{
			if (X1 - X0 <= 0.0 || Z1 - Z0 <= 0.0) return;

			TArray<double> Here;
			for (double Cx : WindowX)
			{
				if (Cx - WinHalf > X0 && Cx + WinHalf < X1
					&& WinCz - WinHalf > Z0 && WinCz + WinHalf < Z1)
				{
					Here.Add(Cx);
				}
			}
			if (Here.Num() == 0)
			{
				AppendBox(Mesh, FVector3d(X0, 0.0, Z0), FVector3d(X1, T, Z1));
				return;
			}
			Here.Sort();

			const double WinZ0 = WinCz - WinHalf;
			const double WinZ1 = WinCz + WinHalf;
			double Cursor = X0;
			for (double Cx : Here)
			{
				const double SX0 = Cx - WinHalf;
				const double SX1 = Cx + WinHalf;
				AppendBox(Mesh, FVector3d(Cursor, 0.0, Z0), FVector3d(SX0, T, Z1));
				AppendBox(Mesh, FVector3d(SX0, 0.0, Z0), FVector3d(SX1, T, WinZ0));
				AppendBox(Mesh, FVector3d(SX0, 0.0, WinZ1), FVector3d(SX1, T, Z1));

				for (int32 r = 0; r < WinSteps; ++r)
				{
					const double Za = WinZ0 + (WinZ1 - WinZ0) * r / double(WinSteps);
					const double Zb = WinZ0 + (WinZ1 - WinZ0) * (r + 1) / double(WinSteps);
					// Row's widest point.
					const double Zw = FMath::Clamp(WinCz, Za, Zb);
					const double Hw = P.GetWindowHalfWidthFraction((Zw - WinCz) / WinHalf) * WinHalf;
					AppendBox(Mesh, FVector3d(SX0, 0.0, Za), FVector3d(Cx - Hw, T, Zb));
					AppendBox(Mesh, FVector3d(Cx + Hw, 0.0, Za), FVector3d(SX1, T, Zb));
				}
				Cursor = SX1;
			}
			AppendBox(Mesh, FVector3d(Cursor, 0.0, Z0), FVector3d(X1, T, Z1));
		};

		// Length along X, thickness along Y; 下鹼 a separate box proud on both faces.
		auto AppendWallRun = [&](double X0, double X1)
		{
			if (X1 - X0 <= 0.0) return;

			if (bBaseCourse)
			{
				FSlotScope BaseTag(Mesh, MatSlot_BaseCourse);
				AppendBox(Mesh, FVector3d(X0, -BaseP, 0.0), FVector3d(X1, T + BaseP, BaseH));
				BaseTag.Close();
				AppendBodySpan(X0, X1, BaseH, H);
			}
			else
			{
				AppendBodySpan(X0, X1, 0.0, H);
			}
		};

		// Stretches the masonry stops at, in run order.
		TArray<TPair<double, double>> Gaps;
		if (bGate)
		{
			Gaps.Add({ MasonryX0, MasonryX1 });
		}
		if (bDoorway)
		{
			Gaps.Add({ DoorX0, DoorX1 });
		}
		Gaps.Sort([](const TPair<double, double>& A, const TPair<double, double>& B)
			{ return A.Key < B.Key; });

		double Cursor = RunX0;
		for (const TPair<double, double>& Gap : Gaps)
		{
			AppendWallRun(Cursor, Gap.Key);
			Cursor = FMath::Max(Cursor, Gap.Value);
		}
		AppendWallRun(Cursor, RunX1);

		// Masonry over the gate head, jamb top to wall top.
		if (bGate && H > JambTop)
		{
			AppendBox(Mesh, FVector3d(MasonryX0, 0.0, JambTop), FVector3d(MasonryX1, T, H));
		}

		if (bDoorway)
		{
			AppendGardenDoorway(Mesh, P, DoorCentre, T, H, BaseH, bBaseCourse ? BaseP : 0.0);

			// Band from surround top to cap underside.
			if (P.HasDoorwayChuihua(L))
			{
				const double BandZ0 =
					P.GetDoorwayHeight() + FMath::Max(P.DoorwaySurroundWidth, 0.0);
				AppendDoorwayDressing(Mesh, P, DoorCentre, T, BandZ0, H);
			}
		}

		for (double Cx : WindowX)
		{
			AppendWindowTrim(Mesh, P, Cx, WinHalf, WinOuter, WinSurW, WinSurP, WinCz, WinSteps, T);
		}

		// 門垛: brick pilasters flanking the opening, proud of both faces.
		const bool bDoorPiers = bGate && P.bHasDoorPiers;
		const double PierFaceP = bDoorPiers
			? FMath::Max(P.GatePierProjection, bBaseCourse ? BaseP + 1.0 : 0.0)
			: (bBaseCourse ? BaseP : 0.0);

		if (bDoorPiers && PierFaceP > 0.0)
		{
			const double PierW = FMath::Clamp(P.GatePierWidth, 5.0, FMath::Max(MasonryX0, 5.0));
			AppendBox(Mesh,
				FVector3d(MasonryX0 - PierW, -PierFaceP,     0.0),
				FVector3d(MasonryX0,          T + PierFaceP, H));
			AppendBox(Mesh,
				FVector3d(MasonryX1,          -PierFaceP,    0.0),
				FVector3d(MasonryX1 + PierW,   T + PierFaceP, H));
		}

		// Raised pier under the hood.
		const double PierX0 = bGate ? FMath::Max(MasonryX0 - FMath::Max(P.GateWingWidth, 0.0), RunX0) : 0.0;
		const double PierX1 = bGate ? FMath::Min(MasonryX1 + FMath::Max(P.GateWingWidth, 0.0), RunX1) : 0.0;
		const double GateRise = bGate ? FMath::Max(P.GateRise, 0.0) : 0.0;
		const bool bGateRoof = bGate && GateRise > 0.0 && PierX1 > PierX0;

		if (bGateRoof)
		{
			AppendBox(Mesh, FVector3d(PierX0, 0.0, H), FVector3d(PierX1, T, H + GateRise));
		}

		// Cap from here up: brick corbelling, then the tiled 牆帽 (tags itself).
		const double CapO = FMath::Max(P.GetCapOverhang(), 0.0);
		const double CorniceH = FMath::Max(P.CapSlabHeight, 0.0);
		const int32 Courses = FMath::Clamp(P.GetCapCorbelCourses(), 1, 8);
		const double RidgeH = FMath::Max(P.CapRidgeHeight, 0.0);

		// 磚檐: corbelled courses to the full overhang (brick, body slot), then the tiled cap (roof).
		TArray<TPair<int32, int32>> BrickRanges;
		auto AppendCap = [&](double X0, double X1, double BaseZ)
		{
			if (X1 - X0 <= 0.0) return;

			double Z = BaseZ;
			if (CorniceH > 0.0)
			{
				const int32 BrickFirst = Mesh.MaxTriangleID();
				const double CourseH = CorniceH / Courses;
				for (int32 i = 0; i < Courses; ++i)
				{
					const double Proj = CapO * double(i + 1) / double(Courses);
					AppendBox(Mesh,
						FVector3d(X0, -Proj,     Z + i * CourseH),
						FVector3d(X1, T + Proj,  Z + (i + 1) * CourseH));
				}
				Z += CorniceH;
				BrickRanges.Emplace(BrickFirst, Mesh.MaxTriangleID());
			}

			if (RidgeH > 0.0)
			{
				// 瓦頂 牆帽: miniature house roof, one 步架 a side over the corbelling, with drip course, tile
				// courses and, if peaked, a 眉子 ridge.
				Shell::FRoofParams Cap;
				const double CapEave = CapO + HutongCanon::Wall::CapDripProjectionCm;
				Cap.FrontOverhang = CapEave;
				Cap.RearOverhang = CapEave;
				Cap.Section = Jiajia::MakeSection(EHutongPurlins::Three, 0.5 * T, CapEave, P.CapRidgeRoll);
				Cap.Rise = RidgeH;
				Cap.SlopeSegments = 6;
				Cap.FasciaDepth = HutongCanon::Wall::CapDripDepthCm;
				Cap.FasciaWidth = HutongCanon::Wall::CapDripProjectionCm;
				Cap.RafterSection = 0.0;
				Cap.RakeDepth = 0.0;
				Cap.Tile = EHutongRoofTile::He;
				Cap.bTileRuns = P.bHasTileRuns;
				Cap.bHasRidgeCourse = P.CapRidgeRoll <= 0.01;
				Cap.RidgeCourseHeight = HutongCanon::Wall::CapRidgeCourseHeightCm;
				Cap.RidgeCourseWidth = HutongCanon::Wall::CapRidgeCourseWidthCm;

				const int32 Mark = Mesh.MaxVertexID();
				Shell::AppendGableRoof(Mesh, X1 - X0, T, Z, Cap);
				TransformVerticesFrom(Mesh, Mark, FTransform(FVector(X0, 0.0, 0.0)));
			}
		};

		if (bGateRoof)
		{
			// Wall cap stops either side of the pier.
			AppendCap(RunX0, PierX0, H);
			AppendCap(PierX1, RunX1, H);
			AppendCap(PierX0, PierX1, H + GateRise);
		}
		else
		{
			AppendCap(RunX0, RunX1, H);
		}

		for (const TPair<int32, int32>& Range : BrickRanges)
		{
			SetMaterialIDForTriangleRange(Mesh, Range.Key, Range.Value, MatSlot_Body);
		}

		// 椽頭 under the hood only; the wall cap is corbelling, never rafters.
		const double HoodRSec = FMath::Max(P.GateRafterSection, 0.0);
		if (bGateRoof && HoodRSec > 0.0)
		{
			FSlotScope RafterTag(Mesh, MatSlot_Wood);
			const double Reach = 3.0 * HoodRSec;
			const double TopZ = H + GateRise;
			Shell::AppendRafterEnds(Mesh, PierX0, PierX1, -CapO, -CapO + Reach,
				TopZ, HoodRSec, P.GateRafterSpacing);
			Shell::AppendRafterEnds(Mesh, PierX0, PierX1, T + CapO - Reach, T + CapO,
				TopZ, HoodRSec, P.GateRafterSpacing);
			RafterTag.Close();
		}

		if (!bGate) return;

		FSlotScope WoodTag(Mesh, MatSlot_Wood);

		FHutongDoorAssembly Door;
		Door.OpeningX0 = ClearX0;
		Door.OpeningX1 = ClearX1;
		// Frame fills the full wall depth including base course projection.
		Door.FrontY = bBaseCourse ? -BaseP : 0.0;
		Door.BackY = bBaseCourse ? T + BaseP : T;
		Door.BottomZ = 0.0;
		Door.LeafTopZ = GateHead;
		Door.JambTopZ = JambTop;
		Door.FrameThickness = GateFrameT;
		Door.ThresholdHeight = P.GateThresholdHeight;
		// Inward to the courtyard, never flat onto the lane.
		Door.StoneReveal = P.DoorStones.bEnabled
			? DoorStoneReveal(GateFrameT, GateW) : 0.0;
		Door.bUseLeafAngles = P.bGateLeavesOpen;
		Door.LeftLeafAngleDeg = -P.GateLeafAngleDeg;
		Door.RightLeafAngleDeg = -P.GateLeafAngleDeg;
		Door.bLeavesOpen = P.bGateLeavesOpen;
		Door.PegCount = P.GatePegCount;
		Door.SwingClearance = FMath::Max(FMath::Min(MasonryX0, L - MasonryX1), 0.0);

		AppendDoorAssembly(Mesh, Door);

		WoodTag.Close();

		// 門墩 and threshold slab keep the doorway from looking like a cut-out.
		FSlotScope StoneTag(Mesh, MatSlot_Stone);

		AppendDoorStonePair(Mesh, P.DoorStones, ClearX0, ClearX1,
			Door.FrontY, Door.BackY, 0.0, GateFrameT,
			/*MaxOutward*/ FMath::Max(MasonryX0 - RunX0, 4.0),
			/*MinProjection*/ PierFaceP - (bBaseCourse ? BaseP : 0.0) + 1.0);

		const double StepD = FMath::Max(P.GateStepDepth, 0.0);
		const double StepH = FMath::Max(P.GateStepHeight, 0.0);
		if (StepD > 0.0 && StepH > 0.0)
		{
			AppendBox(Mesh,
				FVector3d(MasonryX0, Door.FrontY - StepD, 0.0),
				FVector3d(MasonryX1, Door.FrontY,         StepH));
		}

		StoneTag.Close();
	}
}
