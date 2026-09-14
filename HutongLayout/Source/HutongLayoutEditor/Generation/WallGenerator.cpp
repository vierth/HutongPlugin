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

		// The 什錦窗's moulded surround and its 欞條, once the masonry they sit in is down.
		void AppendWindowTrim(FDynamicMesh3& Mesh, const FHutongWallParams& P, double Cx,
			double WinHalf, double WinOuter, double WinSurW, double WinSurP,
			double WinCz, int32 WinSteps, double T)
		{
			// A ring following the outline, built by the same walk the masonry used so the two step round the shape together.
			if (WinSurW > 0.0)
			{
				const int32 SurFirstTri = Mesh.MaxTriangleID();
				const double Lap = FMath::Min(2.0, 0.25 * WinSurW);
				for (int32 r = 0; r < WinSteps * 2; ++r)
				{
					const double Za = WinCz - WinOuter + 2.0 * WinOuter * r / double(WinSteps * 2);
					const double Zb = WinCz - WinOuter + 2.0 * WinOuter * (r + 1) / double(WinSteps * 2);
					// Taken at the row's widest point rather than its midpoint.
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
				// Dressed brick, like the 下鹼: a moulded surround is the same clay laid and cut better.
				SetMaterialIDForTrianglesFrom(Mesh, SurFirstTri, MatSlot_BaseCourse);
			}

			// 欞條 run the opening's full bounding square and are simply buried in the masonry where they pass outside the shape.
			const int32 Bars = FMath::Clamp(P.WindowLatticeBars, 0, 8);
			if (Bars <= 0) return;

			const int32 BarFirstTri = Mesh.MaxTriangleID();
			const double BarT = FMath::Clamp(P.WindowLatticeThickness, 1.0, 0.5 * T);
			const double BarY = 0.5 * (T - BarT);
			for (int32 b = 1; b <= Bars; ++b)
			{
				const double F = b / double(Bars + 1);
				const double X = Cx - WinHalf + 2.0 * WinHalf * F;
				AppendBox(Mesh, FVector3d(X - 0.5 * BarT, BarY, WinCz - WinHalf),
					FVector3d(X + 0.5 * BarT, BarY + BarT, WinCz + WinHalf));
				// Crossing bars sit a little deeper.
				const double Z = WinCz - WinHalf + 2.0 * WinHalf * F;
				AppendBox(Mesh, FVector3d(Cx - WinHalf, BarY - 0.45 * BarT, Z - 0.5 * BarT),
					FVector3d(Cx + WinHalf, BarY + 0.55 * BarT, Z + 0.5 * BarT));
			}
			SetMaterialIDForTrianglesFrom(Mesh, BarFirstTri, MatSlot_Lattice);
		}

		// 牆垣式垂花門: the ornament of a 垂花門 hung on a doorway that has no frame of its own, because the wall is the frame.
		void AppendDoorwayDressing(FDynamicMesh3& Mesh, const FHutongWallParams& P,
			double Cx, double T, double BandZ0, double BandZ1)
		{
			const double HalfW = 0.5 * P.GetDoorwayWidth();
			const double PostR = FMath::Max(0.5 * P.DoorwayPostDiameter, 2.0);
			const double SurP = FMath::Max(P.DoorwaySurroundProjection, 0.0);
			const double CapO = FMath::Max(P.GetCapOverhang(), 0.0);

			// Proud of the surround or the two meet flush on one plane, and under the cap's own overhang so the ornament sits in the eave's shadow.
			const double Proj = FMath::Max(
				FMath::Min(FMath::Max(P.DoorwayDressProjection, 1.0), FMath::Max(CapO, 1.0)),
				SurP + 1.0);

			const double Band = BandZ1 - BandZ0;
			const double BeamH = FMath::Clamp(0.45 * Band, 8.0, 26.0);
			const double BeamTop = BandZ0 + BeamH;
			// The beam overhangs the opening far enough for a post to hang off each end clear of the reveal.
			const double BeamHalf = HalfW + FMath::Max(P.DoorwaySurroundWidth, 0.0) + 2.0 * PostR;

			const int32 FirstTri = Mesh.MaxTriangleID();

			AppendBox(Mesh,
				FVector3d(Cx - BeamHalf, -Proj,    BandZ0),
				FVector3d(Cx + BeamHalf, T + Proj, BeamTop));

			// 花板, held narrower in Y than the beam it sits on.
			if (BandZ1 - 1.0 > BeamTop)
			{
				AppendBox(Mesh,
					FVector3d(Cx - BeamHalf, -0.55 * Proj,     BeamTop),
					FVector3d(Cx + BeamHalf, T + 0.55 * Proj,  BandZ1 - 1.0));
			}

			// 雀替 in the two top corners of the opening, stepped rather than scrolled for the reason the shopfront's 花牙子 are.
			const double Reach = FMath::Min(0.22 * (2.0 * HalfW), 22.0);
			const int32 Steps = 3;
			// Each face's brackets run from the wall face out to short of the beam's own outer face.
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

			SetMaterialIDForTrianglesFrom(Mesh, FirstTri, MatSlot_Paint);

			// 垂蓮柱 off each end of the beam, one per face.
			const int32 PostFirstTri = Mesh.MaxTriangleID();
			const double Drop = FMath::Max(P.DoorwayPostDrop, 4.0 * PostR);
			for (double PostX : { Cx - BeamHalf + PostR + 1.0, Cx + BeamHalf - PostR - 1.0 })
			{
				Frame::AppendHangingPost(Mesh, PostX, -Proj + PostR, BandZ0, Drop, PostR, 0.45);
				Frame::AppendHangingPost(Mesh, PostX, T + Proj - PostR, BandZ0, Drop, PostR, 0.45);
			}
			SetMaterialIDForTrianglesFrom(Mesh, PostFirstTri, MatSlot_Wood);
		}

		// 月亮門 and its relatives: a shaped opening walked through rather than looked through.
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

			// The outline at a height, and the same outline grown by the surround.
			auto ShapeHalf = [&](double Z, double Half, double Height, double Bottom)
			{
				const double t = 2.0 * (Z - Bottom) / FMath::Max(Height, 1.0) - 1.0;
				return P.GetDoorwayHalfWidthFraction(t) * Half;
			};
			// The outline carries on below the ground by the bury, so the cut at the sill is wide enough to walk.
			const double Bury = P.GetDoorwayBury();
			auto OpeningHalf = [&](double Z) { return ShapeHalf(Z, HalfW, Top + Bury, -Bury); };
			auto OuterHalf = [&](double Z) { return ShapeHalf(Z, HalfW + SurW, Top + Bury + 2.0 * SurW, -Bury - SurW); };

			// The outline's widest point anywhere across a row, which is what both the masonry and the surround have to clear.
			auto WidestIn = [&](double Za, double Zb)
			{
				const double Bulge = FMath::Clamp(0.5 * Top, Za, Zb);
				return FMath::Max3(OpeningHalf(Za), OpeningHalf(Zb), OpeningHalf(Bulge));
			};

			// One band of courses, each stepping to the outline's widest point across its own row so no course ever intrudes on the shape it is stepping round.
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

			// The 下鹼 carries on to the edge of the opening, so its band steps round the shape too.
			const double BaseTop = FMath::Clamp(BaseH, Sill, Top);
			// Flush is not absent: with no projection there is no separate course, and the body has
			// to take the band back or the masonry beside the opening is a hole through the wall.
			const bool bProudCourse = BaseP > 0.0 && BaseTop > Sill;
			if (bProudCourse)
			{
				const int32 First = Mesh.MaxTriangleID();
				Rows(Sill, BaseTop, BaseP);
				SetMaterialIDForTriangleRange(Mesh, First, Mesh.MaxTriangleID(), MatSlot_BaseCourse);
			}
			Rows(bProudCourse ? BaseTop : Sill, Top, 0.0);

			// Solid over the head, up to the wall's top.
			if (H > Top)
			{
				AppendBox(Mesh, FVector3d(X0, 0.0, Top), FVector3d(X1, T, H));
			}

			// The stone sill the shape is cut at, stepped over like every other 門檻 here.
			if (Sill > 0.0)
			{
				const int32 First = Mesh.MaxTriangleID();
				AppendBox(Mesh, FVector3d(X0, -BaseP, 0.0), FVector3d(X1, T + BaseP, Sill));
				SetMaterialIDForTriangleRange(Mesh, First, Mesh.MaxTriangleID(), MatSlot_Stone);
			}

			// The moulded surround, lapping over the reveal.
			if (SurW <= 0.0) return;

			const int32 SurFirstTri = Mesh.MaxTriangleID();
			const double Lap = FMath::Min(2.0, 0.25 * SurW);
			const int32 N = Steps * 2;
			const double SurZ1 = Top + SurW;
			for (int32 r = 0; r < N; ++r)
			{
				const double Za = Sill + (SurZ1 - Sill) * r / double(N);
				const double Zb = Sill + (SurZ1 - Sill) * (r + 1) / double(N);

				// Both edges taken at the row's widest point, the same rule the masonry follows.
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
			// Dressed brick, like the 下鹼 and the window's own surround.
			SetMaterialIDForTriangleRange(Mesh, SurFirstTri, Mesh.MaxTriangleID(), MatSlot_BaseCourse);
		}
	}

	void BuildWall(FDynamicMesh3& Mesh, const FHutongWallParams& P)
	{
		using namespace HutongMeshUtils;

		const double L = FMath::Max(P.Length, 1.0);
		const double T = P.GetThickness();
		// GetHeight, not the raw field.
		const double H = FMath::Max(P.GetHeight(), 1.0);

		// The run's real extent: the drawn rect plus whatever miter each end needs to reach into its neighbour.
		const double RunX0 = -FMath::Clamp(P.StartExtend, 0.0, 3.0 * T);
		const double RunX1 = L + FMath::Clamp(P.EndExtend, 0.0, 3.0 * T);

		const double BaseH = FMath::Clamp(P.GetBaseCourseHeight(), 0.0, H * 0.6);
		const double BaseP = FMath::Max(P.BaseCourseProjection, 0.0);
		const bool bBaseCourse = (BaseH > 0.0 && BaseP > 0.0);

		// The gate comes first because every run of masonry below has to stop at it.
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

		// The head sits on the jambs, and the masonry above it carries on to the wall top.
		const double JambTop = FMath::Min(GateHead + GateFrameT, H);

		const bool bGate = P.bHasGate && (MasonryX1 - MasonryX0) > 1.0 && MasonryX0 > 0.0 && MasonryX1 < L;

		// The garden doorway, worked out the same way and for the same reason.
		const double DoorHalfW = 0.5 * P.GetDoorwayWidth();
		const double DoorMargin = DoorHalfW + FMath::Max(P.DoorwaySurroundWidth, 0.0);
		const double DoorCentre = P.GetDoorwayCentre(L);
		const double DoorX0 = DoorCentre - DoorHalfW;
		const double DoorX1 = DoorCentre + DoorHalfW;

		// Dropped rather than overlapped where a run carries a 牆垣式門 as well and the two would collide.
		const bool bDoorwayClearOfGate = !bGate
			|| DoorX1 + DoorMargin - DoorHalfW < MasonryX0
			|| DoorX0 - DoorMargin + DoorHalfW > MasonryX1;
		const bool bDoorway = P.HasDecorativeDoorway(L) && bDoorwayClearOfGate
			&& DoorX0 > RunX0 && DoorX1 < RunX1;

		// No 散水 on a wall: the apron catches what a roof sheds, and a brick cap sheds a trickle where an eave sheds a metre of roof.

		// 什錦窗, worked out alongside the gate and for the same reason.
		const double WinHalf = 0.5 * FMath::Max(P.WindowSize, 10.0);
		const double WinSurW = FMath::Max(P.WindowSurroundWidth, 0.0);
		const double WinSurP = FMath::Max(P.WindowSurroundProjection, 0.0);
		const double WinOuter = WinHalf + WinSurW;
		const double WinCz = P.WindowCentreHeight;
		const int32 WinSteps = FMath::Clamp(P.WindowOutlineSteps, 2, 32);

		TArray<double> WindowX;
		{
			const int32 Count = P.GetWindowCount(L);
			// Held clear of the wall top and of the 下鹼.
			const bool bFits = (WinCz - WinOuter > BaseH + 4.0) && (WinCz + WinOuter < H - 8.0);
			for (int32 i = 0; i < (bFits ? Count : 0); ++i)
			{
				const double Cx = (i + 0.5) * L / double(Count);
				// Never through the gate or the piers standing either side of it.
				if (bGate)
				{
					const double Keep = FMath::Max(P.GatePierWidth, P.GateWingWidth) + WinOuter;
					if (Cx > MasonryX0 - Keep && Cx < MasonryX1 + Keep) continue;
				}
				// Nor through the garden doorway, for the same reason.
				if (bDoorway)
				{
					const double Keep = WinOuter + FMath::Max(P.DoorwaySurroundWidth, 0.0);
					if (Cx > DoorX0 - Keep && Cx < DoorX1 + Keep) continue;
				}
				if (Cx - WinOuter <= RunX0 || Cx + WinOuter >= RunX1) continue;
				WindowX.Add(Cx);
			}
		}

		// The wall body between Z0 and Z1, cut for whatever windows fall inside the span.
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
					// The row's own widest point.
					const double Zw = FMath::Clamp(WinCz, Za, Zb);
					const double Hw = P.GetWindowHalfWidthFraction((Zw - WinCz) / WinHalf) * WinHalf;
					AppendBox(Mesh, FVector3d(SX0, 0.0, Za), FVector3d(Cx - Hw, T, Zb));
					AppendBox(Mesh, FVector3d(Cx + Hw, 0.0, Za), FVector3d(SX1, T, Zb));
				}
				Cursor = SX1;
			}
			AppendBox(Mesh, FVector3d(Cursor, 0.0, Z0), FVector3d(X1, T, Z1));
		};

		// Length along X, thickness along Y, with the 下鹼 as a separate box standing proud on both faces.
		auto AppendWallRun = [&](double X0, double X1)
		{
			if (X1 - X0 <= 0.0) return;

			if (bBaseCourse)
			{
				const int32 BaseFirstTri = Mesh.MaxTriangleID();
				AppendBox(Mesh, FVector3d(X0, -BaseP, 0.0), FVector3d(X1, T + BaseP, BaseH));
				SetMaterialIDForTrianglesFrom(Mesh, BaseFirstTri, MatSlot_BaseCourse);
				AppendBodySpan(X0, X1, BaseH, H);
			}
			else
			{
				AppendBodySpan(X0, X1, 0.0, H);
			}
		};

		// Every stretch the wall's own masonry has to stop at, in order along the run.
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

		// Masonry over the gate's head, from the top of the jambs to the wall's own top.
		if (bGate && H > JambTop)
		{
			AppendBox(Mesh, FVector3d(MasonryX0, 0.0, JambTop), FVector3d(MasonryX1, T, H));
		}

		if (bDoorway)
		{
			AppendGardenDoorway(Mesh, P, DoorCentre, T, H, BaseH, bBaseCourse ? BaseP : 0.0);

			// The band runs from the top of the surround to the underside of the cap.
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

		// 門垛: brick pilasters flanking the opening, standing proud of both faces.
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

		// The raised pier the hood sits on.
		const double PierX0 = bGate ? FMath::Max(MasonryX0 - FMath::Max(P.GateWingWidth, 0.0), RunX0) : 0.0;
		const double PierX1 = bGate ? FMath::Min(MasonryX1 + FMath::Max(P.GateWingWidth, 0.0), RunX1) : 0.0;
		const double GateRise = bGate ? FMath::Max(P.GateRise, 0.0) : 0.0;
		const bool bGateRoof = bGate && GateRise > 0.0 && PierX1 > PierX0;

		if (bGateRoof)
		{
			AppendBox(Mesh, FVector3d(PierX0, 0.0, H), FVector3d(PierX1, T, H + GateRise));
		}

		// Everything from here up is cap, and takes the roof slot.
		const int32 CapFirstTri = Mesh.MaxTriangleID();

		const double CapO = FMath::Max(P.GetCapOverhang(), 0.0);
		const double CorniceH = FMath::Max(P.CapSlabHeight, 0.0);
		const int32 Courses = FMath::Clamp(P.GetCapCorbelCourses(), 1, 8);
		const double RidgeH = FMath::Max(P.CapRidgeHeight, 0.0);

		// 磚檐: corbelled courses stepping out to the full overhang, then the tiled cap on top.
		// The courses are brick and keep the body's slot; only the tiled cap is roof.
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
				// A miniature roof one 步架 wide.
				const FHutongRoofSection CapSection = Jiajia::MakeSection(
					EHutongPurlins::Three, 0.5 * (T + 2.0 * CapO), 0.0, P.CapRidgeRoll);

				AppendCurvedGableRoof(Mesh,
					FVector3d(X0, -CapO, Z),
					X1 - X0,
					T + 2.0 * CapO,
					RidgeH,
					CapSection,
					6,
					EAxis2D::X);
			}
		};

		if (bGateRoof)
		{
			// The wall cap stops either side of the pier.
			AppendCap(RunX0, PierX0, H);
			AppendCap(PierX1, RunX1, H);
			AppendCap(PierX0, PierX1, H + GateRise);
		}
		else
		{
			AppendCap(RunX0, RunX1, H);
		}

		SetMaterialIDForTrianglesFrom(Mesh, CapFirstTri, MatSlot_Roof);
		for (const TPair<int32, int32>& Range : BrickRanges)
		{
			SetMaterialIDForTriangleRange(Mesh, Range.Key, Range.Value, MatSlot_Body);
		}

		// 椽頭 under the hood only: the wall cap is brick corbelling and never had rafters.
		const double HoodRSec = FMath::Max(P.GateRafterSection, 0.0);
		if (bGateRoof && HoodRSec > 0.0)
		{
			const int32 RafterFirstTri = Mesh.MaxTriangleID();
			const double Reach = 3.0 * HoodRSec;
			const double TopZ = H + GateRise;
			Shell::AppendRafterEnds(Mesh, PierX0, PierX1, -CapO, -CapO + Reach,
				TopZ, HoodRSec, P.GateRafterSpacing);
			Shell::AppendRafterEnds(Mesh, PierX0, PierX1, T + CapO - Reach, T + CapO,
				TopZ, HoodRSec, P.GateRafterSpacing);
			SetMaterialIDForTrianglesFrom(Mesh, RafterFirstTri, MatSlot_Wood);
		}

		if (!bGate) return;

		const int32 WoodFirstTri = Mesh.MaxTriangleID();
		int32 LeafFirstTri = MAX_int32;

		FHutongDoorAssembly Door;
		Door.OpeningX0 = ClearX0;
		Door.OpeningX1 = ClearX1;
		// The frame fills the wall's full depth including the base course projection.
		Door.FrontY = bBaseCourse ? -BaseP : 0.0;
		Door.BackY = bBaseCourse ? T + BaseP : T;
		Door.BottomZ = 0.0;
		Door.LeafTopZ = GateHead;
		Door.JambTopZ = JambTop;
		Door.FrameThickness = GateFrameT;
		Door.ThresholdHeight = P.GateThresholdHeight;
		// Inward, into the courtyard, never flat onto the lane.
		Door.StoneReveal = P.DoorStones.bEnabled
			? DoorStoneReveal(GateFrameT, GateW) : 0.0;
		Door.bUseLeafAngles = P.bGateLeavesOpen;
		Door.LeftLeafAngleDeg = -P.GateLeafAngleDeg;
		Door.RightLeafAngleDeg = -P.GateLeafAngleDeg;
		Door.bLeavesOpen = P.bGateLeavesOpen;
		Door.PegCount = P.GatePegCount;
		Door.SwingClearance = FMath::Max(FMath::Min(MasonryX0, L - MasonryX1), 0.0);

		AppendDoorAssembly(Mesh, Door, &LeafFirstTri);

		SetMaterialIDForTrianglesFrom(Mesh, WoodFirstTri, MatSlot_Wood);
		SetMaterialIDForTrianglesFrom(Mesh, LeafFirstTri, MatSlot_DoorPaint);

		// 門墩 and the threshold slab, which are most of what stops the doorway looking like a cut-out.
		const int32 StoneFirstTri = Mesh.MaxTriangleID();

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

		SetMaterialIDForTrianglesFrom(Mesh, StoneFirstTri, MatSlot_Stone);
	}
}
