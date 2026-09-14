#include "Generation/InnerGateGenerator.h"
#include "Generation/HutongDoor.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongShell.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	void BuildInnerGate(FDynamicMesh3& Mesh, const FHutongInnerGateParams& P)
	{
		using namespace HutongMeshUtils;

		const double W = FMath::Max(P.Width, 1.0);
		const double D = FMath::Max(P.Depth, 1.0);
		const double Eave = P.GetEaveHeight();
		const double ColR = FMath::Min(P.GetColumnRadius(), 0.2 * FMath::Min(W, D));
		const double Floor = FMath::Clamp(P.FloorHeight, 0.0, Eave * 0.3);
		const double PlatO = FMath::Max(P.PlatformOverhang, ColR);

		// One bay, so the boundaries are the two ends, inset so the posts sit inside the platform.
		auto BoundaryX = [&](int32 i) { return (i <= 0) ? ColR : (W - ColR); };

		// 獨立柱擔梁式, and the layout is the whole type.
		const double FrontY = 0.0;
		const double BackY = D;
		const double MidY = 0.5 * D;

		// 1) 臺基, with 踏跺 down both sides.
		Shell::AppendPlatform(Mesh, W, D, Floor, PlatO, PlatO,
			P.StepCount, P.StepTread, BoundaryX(0), BoundaryX(1));
		Shell::AppendSteps(Mesh, BoundaryX(0), BoundaryX(1), D + PlatO, 1.0,
			P.StepCount, P.StepTread, Floor);

		// --- Woodwork ---
		const int32 WoodFirstTri = Mesh.MaxTriangleID();
		int32 LeafFirstTri = MAX_int32;
		int32 LeafEndTri = MAX_int32;
		const double ColTopR = Frame::TaperedTopRadius(ColR, P.GetColumnHeight(), P.ColumnTaperRatio);
		const double PT = FMath::Max(2.0 * ColR * 0.8, 8.0);
		const double FrameT = FMath::Clamp(P.DoorFrameThickness, 2.0, 2.0 * ColR);
		// The beam's underside is the head's ceiling, so it has to clear the frame as well as ride the head.
		const double BeamBottom = FMath::Clamp(
			P.GetDoorHeadHeight() + FMath::Max(0.4 * PT, FrameT),
			Floor + 60.0, Eave - 1.2 * PT);

		// 2) The one column pair, on the centre line, and the 額枋 across it.
		Frame::AppendColumnRow(Mesh, BoundaryX, 1, MidY, ColR, ColTopR, Floor, Eave, 1.0, 16);
		Frame::AppendArchitrave(Mesh, BoundaryX(0), BoundaryX(1), MidY, PT, BeamBottom, Eave);

		// 擔梁 riding over each column and running the full depth.
		Frame::AppendTieBeams(Mesh, BoundaryX, 1, FrontY, BackY, PT, BeamBottom, Eave, 1.0);

		// The cross stack, identical on both faces.
		const double BeamTop = FMath::Min(BeamBottom + PT, Eave);
		const double BracketReach = FMath::Clamp(P.BracketReach, 4.0,
			0.4 * (BoundaryX(1) - BoundaryX(0)));

		auto AppendCrossFace = [&](double FaceY)
		{
			// Deeper in Y than the 擔梁 it laps.
			AppendBox(Mesh,
				FVector3d(BoundaryX(0), FaceY - 0.6 * PT, BeamBottom),
				FVector3d(BoundaryX(1), FaceY + 0.6 * PT, BeamTop));

			// 花板, held narrower than the beam so it is recessed on both faces.
			if (P.bHasFriezePanel && Eave > BeamTop)
			{
				AppendBox(Mesh,
					FVector3d(BoundaryX(0), FaceY - 0.34 * PT, BeamTop),
					FVector3d(BoundaryX(1), FaceY + 0.34 * PT, Eave));
			}

			// 雀替, stepped rather than scrolled for the reason the shopfront's 花牙子 are.
			if (P.bHasBrackets)
			{
				const int32 Steps = 3;
				for (int32 s = 0; s < Steps; ++s)
				{
					const double A = BracketReach * (Steps - s) / double(Steps);
					const double Drop = 0.5 * BracketReach * (s + 1) / double(Steps);
					AppendBox(Mesh,
						FVector3d(BoundaryX(0),     FaceY - 0.44 * PT, BeamBottom - Drop),
						FVector3d(BoundaryX(0) + A, FaceY + 0.44 * PT, BeamBottom));
					AppendBox(Mesh,
						FVector3d(BoundaryX(1) - A, FaceY - 0.44 * PT, BeamBottom - Drop),
						FVector3d(BoundaryX(1),     FaceY + 0.44 * PT, BeamBottom));
				}
			}
		};

		AppendCrossFace(FrontY);
		AppendCrossFace(BackY);

		// 3) 垂蓮柱, one at each of the 擔梁's four ends.
		if (P.bHasHangingPosts)
		{
			const double PostR = FMath::Max(0.5 * P.HangingPostDiameter, 3.0);
			const double Drop = FMath::Max(P.HangingPostDrop, 4.0 * PostR);
			// Only build them if there is air under the beam to hang in; a bud resting on the platform is not a hanging post.
			if (BeamBottom - Drop > Floor + 40.0)
			{
				for (int32 i = 0; i <= 1; ++i)
				{
					Frame::AppendHangingPost(Mesh, BoundaryX(i), FrontY, BeamBottom, Drop, PostR, P.BudFraction);
					Frame::AppendHangingPost(Mesh, BoundaryX(i), BackY,  BeamBottom, Drop, PostR, P.BudFraction);
				}
			}
		}

		// 4) The doors, on the centre column line — the middle of the gate, not the back of it.
		const double BayW = BoundaryX(1) - BoundaryX(0);
		const double DoorW = BayW * FMath::Clamp(P.DoorWidthFraction, 0.2, 1.0);
		const double JambL = BoundaryX(0) + 0.5 * (BayW - DoorW);
		const double JambR = JambL + DoorW;
		const double Head = FMath::Clamp(P.GetDoorHeadHeight(), Floor + 80.0, BeamBottom - FrameT);

		{
			FHutongDoorAssembly Door;
			Door.OpeningX0 = JambL;
			Door.OpeningX1 = JambR;
			Door.FrontY = MidY - ColR;
			Door.BackY = MidY + ColR;
			Door.BottomZ = Floor;
			Door.LeafTopZ = Head;
			Door.JambTopZ = BeamBottom;
			Door.FrameThickness = FrameT;
			Door.ThresholdHeight = P.ThresholdHeight;
			Door.PegCount = P.DoorPegCount;
			// Driven by angle rather than clearance.
			Door.bUseLeafAngles = true;
			Door.LeftLeafAngleDeg = P.bDoorLeavesOpen ? -90.0 : 0.0;
			Door.RightLeafAngleDeg = P.bDoorLeavesOpen ? -90.0 : 0.0;
			Door.StoneReveal = P.DoorStones.bEnabled
				? DoorStoneReveal(FrameT, DoorW) : 0.0;

			AppendDoorAssembly(Mesh, Door, &LeafFirstTri);
			LeafEndTri = Mesh.MaxTriangleID();
		}

		// 餘塞板 either side of the doorway, from each jamb out to the column centre, and a 走馬板 across the whole plane above the head.
		const double PlaneY0 = MidY - 0.35 * ColR;
		const double PlaneY1 = MidY + 0.35 * ColR;
		if (JambL - FrameT > BoundaryX(0))
		{
			AppendBox(Mesh, FVector3d(BoundaryX(0), PlaneY0, Floor),
				FVector3d(JambL - FrameT, PlaneY1, BeamBottom));
		}
		if (BoundaryX(1) > JambR + FrameT)
		{
			AppendBox(Mesh, FVector3d(JambR + FrameT, PlaneY0, Floor),
				FVector3d(BoundaryX(1), PlaneY1, BeamBottom));
		}

		SetMaterialIDForTrianglesFrom(Mesh, WoodFirstTri, MatSlot_Wood);
		// Bounded: the 餘塞板 above are appended after the assembly, and they are joinery like the rest of the gate.
		SetMaterialIDForTriangleRange(Mesh, LeafFirstTri, LeafEndTri, MatSlot_DoorPaint);

		// 門枕石 on the platform at the foot of the jambs, in the door plane on the centre column line.
		{
			const int32 StoneFirstTri = Mesh.MaxTriangleID();
			AppendDoorStonePair(Mesh, P.DoorStones, JambL, JambR,
				MidY - ColR, MidY + ColR, Floor, FrameT,
				/*MaxOutward*/ FMath::Max(JambL - BoundaryX(0), 4.0));
			SetMaterialIDForTrianglesFrom(Mesh, StoneFirstTri, MatSlot_Stone);
		}

		// 5) Roof.
		Shell::FRoofParams Roof;
		Roof.FrontOverhang = FMath::Max(P.RoofOverhang, 0.0);
		Roof.RearOverhang = FMath::Max(P.RoofOverhang, 0.0);
		Roof.GableOverhang = FMath::Max(P.GableOverhang, 0.0);
		Roof.Rise = P.GetRoofRise();
		// 三檁: the 獨立柱 gate is one 步架 front to back, which is exactly what its depth band says.
		Roof.Section = Jiajia::MakeSection(
			EHutongPurlins::Three, 0.5 * D, Roof.FrontOverhang, P.RoofApexRoll);
		Roof.SlopeSegments = 8;
		Roof.FasciaDepth = P.EaveFasciaDepth;
		Roof.FasciaWidth = P.EaveFasciaWidth;
		Roof.RafterSection = P.RafterEndSection;
		Roof.RafterSpacing = P.RafterEndSpacing;
		// 正脊 with 蠍子尾, like a gate house and unlike a dwelling.
		Roof.bHasRidgeCourse = P.bHasRidgeCourse;
		Roof.RidgeCourseHeight = P.RidgeCourseHeight;
		Roof.RidgeCourseWidth = P.RidgeCourseWidth;
		Roof.RidgeEndKick = P.RidgeEndKick;

		// Spanning the full depth, front beam line to back beam line.
		Shell::AppendGableRoof(Mesh, W, D, Eave, Roof);
	}
}
