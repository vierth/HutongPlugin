#include "Generation/GateHouseGenerator.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongDoor.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongShell.h"
#include "Math/RandomStream.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	void BuildGateHouse(FDynamicMesh3& Mesh, const FHutongGateHouseParams& P)
	{
		using namespace HutongMeshUtils;

		const double W = FMath::Max(P.Width, 1.0);
		const double D = FMath::Max(P.Depth, 1.0);
		const double Eave = FMath::Max(P.GetEaveHeight(), 20.0);
		const double T = FMath::Clamp(P.WallThickness, 1.0, FMath::Min(W, D) * 0.25);
		const double Floor = FMath::Clamp(P.FloorHeight, 0.0, Eave * 0.4);
		const double ColR = FMath::Max(0.5 * P.GetColumnDiameter(), 1.0);

		// A gate house is one bay (一間).
		auto BoundaryX = [&](int32 i) { return (i <= 0) ? ColR : (W - ColR); };

		const double BaseH = FMath::Clamp(P.GetBaseCourseHeight(), 0.0, (Eave - Floor) * 0.6);
		const double BaseP = FMath::Max(P.BaseCourseProjection, 0.0);
		const bool bBaseCourse = (BaseH > 0.0 && BaseP > 0.0);
		const double WallBottom = bBaseCourse ? Floor + BaseH : Floor;

		// The door plane.
		double DoorY = FMath::Clamp(P.GetDoorPlaneFraction() * D, 0.0, FMath::Max(D - T - 20.0, 0.0));
		// A recess shallower than the wall it is cut into is not a recess; snap it flush so the two cases do not produce nearly-coincident faces.
		if (DoorY < T) DoorY = 0.0;
		const bool bRecessed = (DoorY > 0.0);

		// 1) 臺基, with the 踏跺 under the doorway.
		const double PlatO = FMath::Max(P.PlatformOverhang, ColR);
		// Flush on the flanks with the 下鹼 running down past it.
		const double PlatSide = 0.0;
		Shell::AppendPlatform(Mesh, W, D, Floor, PlatO, PlatSide,
			P.StepCount, P.StepTread, BoundaryX(0), BoundaryX(1));

		// 1b) 散水 outside the platform.
		if (P.Apron.bEnabled)
		{
			// A gate has courtyard behind it and lane in front.
			Shell::AppendApron(Mesh, 0.0, -PlatO, W, D,
				P.Apron.GetWidth(P.RoofOverhang),
				P.Apron.GetWidth(P.RoofOverhang),
				/*SideWidth*/ 0.0,   // 硬山 as well
				P.Apron.Thickness);
		}

		// 2) 下鹼 on the two side walls only — there is no back wall to carry it.
		{
			const int32 BaseFirstTri = Mesh.MaxTriangleID();
			Shell::AppendBaseCourseU(Mesh, W, D, T, Floor, BaseH, BaseP,
				/*bIncludeRear*/ false, /*bToGround*/ true);
			SetMaterialIDForTrianglesFrom(Mesh, BaseFirstTri, MatSlot_BaseCourse);
		}

		// Side walls only.
		AppendBox(Mesh, FVector3d(0.0, 0.0, WallBottom), FVector3d(T, D, Eave));
		AppendBox(Mesh, FVector3d(W - T, 0.0, WallBottom), FVector3d(W, D, Eave));

		// 踏跺 down the courtyard side as well.
		Shell::AppendSteps(Mesh, BoundaryX(0), BoundaryX(1), D + PlatSide, 1.0,
			P.StepCount, P.StepTread, Floor);

		// 4) 墀頭 at the front corners.
		if (P.bHasChitou)
		{
			Shell::AppendChitou(Mesh, W, T, Floor, Eave,
				P.GetChitouProjection(), P.ChitouCorbelSteps);
		}

		// --- Woodwork ---
		const int32 WoodFirstTri = Mesh.MaxTriangleID();
		int32 LeafFirstTri = MAX_int32;
		const double ColOvershoot = 1.0;
		// Resolved before the frame, not with the doorway further down.
		const double DoorHead = FMath::Clamp(P.GetDoorHeadHeight(), Floor + 60.0, Eave - 10.0);
		const double ColTopR = Frame::TaperedTopRadius(ColR, P.GetColumnHeight(), P.ColumnTaperRatio);

		// 檐柱 on the front edge, always.
		Frame::AppendColumnRow(Mesh, BoundaryX, 1, 0.0, ColR, ColTopR, 0.0, Eave, ColOvershoot);
		Frame::AppendArchitrave(Mesh, BoundaryX(0), BoundaryX(1), 0.0,
			FMath::Max(2.0 * ColR * 0.8, 8.0), DoorHead, Eave);

		if (bRecessed)
		{
			Frame::AppendColumnRow(Mesh, BoundaryX, 1, DoorY, ColR, ColTopR, 0.0, Eave, ColOvershoot);
			Frame::AppendTieBeams(Mesh, BoundaryX, 1, 0.0, DoorY, FMath::Max(2.0 * ColR * 0.7, 6.0),
				DoorHead, Eave, ColOvershoot);
		}

		// The doorway itself, centred in the bay.
		const double BayW = BoundaryX(1) - BoundaryX(0);
		const double FrameT = FMath::Clamp(P.DoorFrameThickness, 2.0, T);
		const double DoorW = FMath::Clamp(P.DoorWidthFraction, 0.1, 1.0) * BayW;
		const double ClearX0 = BoundaryX(0) + 0.5 * (BayW - DoorW);
		const double ClearX1 = ClearX0 + DoorW;

		FHutongDoorAssembly Door;
		Door.OpeningX0 = ClearX0;
		Door.OpeningX1 = ClearX1;
		Door.FrontY = DoorY;
		Door.BackY = DoorY + T;
		Door.BottomZ = Floor;
		Door.LeafTopZ = DoorHead;
		Door.JambTopZ = FMath::Min(DoorHead + FrameT, Eave);
		Door.FrameThickness = FrameT;
		Door.ThresholdHeight = P.ThresholdHeight;
		Door.PegCount = P.DoorPegCount;
		// Both leaves swing inward, each by its own angle from the seed.
		Door.StoneReveal = P.GetDoorStones().bEnabled
			? DoorStoneReveal(FrameT, DoorW) : 0.0;
		Door.bLeavesOpen = P.bLeavesOpen;
		Door.bUseLeafAngles = P.bLeavesOpen;
		if (P.bLeavesOpen)
		{
			FRandomStream Rand(P.RandomSeed);

			const double LoDeg = FMath::Min(P.AjarAngleMin, P.AjarAngleMax);
			const double HiDeg = FMath::Max(P.AjarAngleMin, P.AjarAngleMax);
			double LeftDeg = FMath::Lerp(LoDeg, HiDeg, static_cast<double>(Rand.FRand()));
			double RightDeg = FMath::Lerp(LoDeg, HiDeg, static_cast<double>(Rand.FRand()));

			// Clear gap = DoorW - LeafW * (cos L + cos R).
			const double LeafW = 0.5 * DoorW;
			if (LeafW > 1.0 && P.MinClearWidth > 0.0)
			{
				const double Clear =
					DoorW - LeafW * (FMath::Cos(FMath::DegreesToRadians(LeftDeg))
								   + FMath::Cos(FMath::DegreesToRadians(RightDeg)));
				if (Clear < P.MinClearWidth)
				{
					const double NeededDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
						1.0 - P.MinClearWidth / (2.0 * LeafW), -1.0, 1.0)));
					LeftDeg = FMath::Max(LeftDeg, NeededDeg);
					RightDeg = FMath::Max(RightDeg, NeededDeg);
				}
			}

			// Negative swings inward, into the passage rather than out over the lane.
			Door.LeftLeafAngleDeg = -LeftDeg;
			Door.RightLeafAngleDeg = -RightDeg;
		}
		AppendDoorAssembly(Mesh, Door, &LeafFirstTri);

		SetMaterialIDForTrianglesFrom(Mesh, WoodFirstTri, MatSlot_Wood);
		SetMaterialIDForTrianglesFrom(Mesh, LeafFirstTri, MatSlot_DoorPaint);

		// 門墩 at the foot of the jambs, in the door plane — so on a recessed gate they stand inside the recess.
		{
			const int32 StoneFirstTri = Mesh.MaxTriangleID();
			AppendDoorStonePair(Mesh, P.GetDoorStones(), ClearX0, ClearX1,
				DoorY, DoorY + T, Floor, FrameT,
				/*MaxOutward*/ FMath::Max(ClearX0 - FrameT - T, 4.0));
			SetMaterialIDForTrianglesFrom(Mesh, StoneFirstTri, MatSlot_Stone);
		}

		// 餘塞板 and 走馬板 closing the rest of the door plane, since the doorway is only part of it.
		const double FlankX0 = T;
		const double FlankX1 = W - T;
		const double JambOuterX0 = ClearX0 - FrameT;
		const double JambOuterX1 = ClearX1 + FrameT;

		if (P.HasBrickScreen())
		{
			// 如意門: the bay is brickwork with a doorway cut in it, so the fill is masonry and runs the full height either side.
			if (JambOuterX0 > FlankX0)
			{
				AppendBox(Mesh, FVector3d(FlankX0, DoorY, Floor),
					FVector3d(JambOuterX0, DoorY + T, Eave));
			}
			if (FlankX1 > JambOuterX1)
			{
				AppendBox(Mesh, FVector3d(JambOuterX1, DoorY, Floor),
					FVector3d(FlankX1, DoorY + T, Eave));
			}
			if (Eave > Door.JambTopZ)
			{
				AppendBox(Mesh, FVector3d(JambOuterX0, DoorY, Door.JambTopZ),
					FVector3d(JambOuterX1, DoorY + T, Eave));
			}
		}
		else
		{
			// The timber gates get 餘塞板 filling between the frame and the columns, and a 走馬板 board across the whole plane above the head.
			const int32 InfillFirstTri = Mesh.MaxTriangleID();

			if (JambOuterX0 > FlankX0)
			{
				AppendBox(Mesh, FVector3d(FlankX0, DoorY, Floor),
					FVector3d(JambOuterX0, DoorY + T, Door.JambTopZ));
			}
			if (FlankX1 > JambOuterX1)
			{
				AppendBox(Mesh, FVector3d(JambOuterX1, DoorY, Floor),
					FVector3d(FlankX1, DoorY + T, Door.JambTopZ));
			}
			if (Eave > Door.JambTopZ)
			{
				AppendBox(Mesh, FVector3d(FlankX0, DoorY, Door.JambTopZ),
					FVector3d(FlankX1, DoorY + T, Eave));
			}

			SetMaterialIDForTrianglesFrom(Mesh, InfillFirstTri, MatSlot_Wood);
		}

		// 門頭: the corbelled courses hooding the doorway, and what makes a 如意門 recognisable.
		const int32 HoodCourses = FMath::Clamp(P.HoodCourses, 0, 8);
		const double HoodProj = FMath::Max(P.HoodProjection, 0.0);
		if (P.HasBrickScreen() && HoodCourses > 0 && HoodProj > 0.0)
		{
			const double CourseH = HoodProj;
			const double StepOut = HoodProj / HoodCourses;
			const double HoodBase = Door.JambTopZ;
			for (int32 i = 0; i < HoodCourses; ++i)
			{
				const double Proj = StepOut * (i + 1);
				AppendBox(Mesh,
					FVector3d(ClearX0 - FrameT - Proj, DoorY - Proj, HoodBase + i * CourseH),
					FVector3d(ClearX1 + FrameT + Proj, DoorY,        HoodBase + (i + 1) * CourseH));
			}
		}

		// 5) Roof, eave course and 正脊, tagged into the roof slot by the shell helper.
		Shell::FRoofParams Roof;
		Roof.FrontOverhang = P.RoofOverhang;
		// Symmetrical: a gate's back is the courtyard, not a lane. See the note on the params.
		Roof.RearOverhang = P.RoofOverhang;
		Roof.Rise = P.GetRoofRise(D);
		Roof.Section = Jiajia::MakeSection(P.GetPurlins(), 0.5 * D, Roof.FrontOverhang, P.RoofApexRoll);
		Roof.Tile = P.GetRoofTile();
		Roof.TileRowSpacing = P.TileRowSpacing;
		Roof.SlopeSegments = 8;
		Roof.FasciaDepth = P.EaveFasciaDepth;
		Roof.FasciaWidth = P.EaveFasciaWidth;
		Roof.RafterSection = P.RafterEndSection;
		Roof.RafterSpacing = P.RafterEndSpacing;
		Roof.bHasRidgeCourse = P.bHasRidgeCourse;
		Roof.RidgeCourseHeight = P.RidgeCourseHeight;
		Roof.RidgeCourseWidth = P.RidgeCourseWidth;
		Roof.RidgeEndKick = P.RidgeEndKick;
		Shell::AppendGableRoof(Mesh, W, D, Eave, Roof);
	}
}
