#include "Generation/InnerGateGenerator.h"
#include "Generation/FrameGenerator.h"
#include "Generation/HutongDoor.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongShell.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	namespace
	{
		using namespace HutongMeshUtils;

		// 臺基 with its front flight. Flush sides keep the rear lip (where the back flight lands) as its own box.
		void AppendGatePlatform(FDynamicMesh3& Mesh, const FHutongInnerGateParams& P,
			double W, double D, double Floor, double PlatO, double StepX0, double StepX1)
		{
			if (!P.bFlushSides)
			{
				Shell::AppendPlatform(Mesh, W, D, Floor, PlatO, PlatO, P.GetStepCount(), P.StepTread, StepX0, StepX1);
				return;
			}
			Shell::AppendPlatform(Mesh, W, D, Floor, PlatO, 0.0, P.GetStepCount(), P.StepTread, StepX0, StepX1);
			if (Floor <= 0.0) return;
			FSlotScope RearTag(Mesh, MatSlot_Stone);
			AppendBox(Mesh, FVector3d(0.0, D, 0.0), FVector3d(W, D + PlatO, Floor));
			RearTag.Close();
		}

		// 徹上明造: a 三架梁 and 脊瓜柱 on each post line and the 脊檁 over them, under the shell; Span deep.
		void AppendGateFrame(FDynamicMesh3& Mesh, const FHutongInnerGateParams& P, double Span, double Eave, double Floor,
			const TArray<double>& FrameX, double W, const Shell::FRoofParams& Roof, double RoofZ)
		{
			FRoofFrameOptions Options;
			Options.bSkipEaveLines = true;
			Options.Underside = [&](double Y) { return Shell::UndersideAt(Roof, Span, RoofZ, Y); };
			Options.GableReach = FMath::Max(Roof.GableOverhang - 0.5, 0.0);
			AppendRoofFrame(Mesh, FrameLayout::Make(2.0 * P.GetColumnRadius(), Eave, Floor, Span, EHutongPurlins::Three, false, false),
				FrameX, 0.0, W, Options);
		}

		// 一殿一卷式: two column rows. Front row in the wall line carries the 攢邊門; 麻葉抱頭梁 run from the
		// rear row over it to the 垂蓮柱, whose cross beam, 花板 and 雀替 form the face. Rear row carries a
		// 屏門, turning the way in aside onto the covered walk. Two roofs, 殿 (gable, 清水脊) front and 卷
		// (rolled) back, meet eave to eave at the 天溝: each an ordinary gable ending on that line, no cutting.
		void BuildHallAndRoll(FDynamicMesh3& Mesh, const FHutongInnerGateParams& P)
		{
			const double W = FMath::Max(P.Width, 1.0);
			const double D = FMath::Max(P.Depth, 1.0);
			const double Eave = P.GetEaveHeight();
			// Roofs on the frame's line; posts and beams at the column top, 花板 up to the ceiling.
			const double RoofZ = Eave + P.GetRoofLift();
			const double CeilZ = RoofZ + P.GetUndersideRise();
			const double ColR = FMath::Min(P.GetColumnRadius(), 0.2 * FMath::Min(W, D));
			const double Floor = FMath::Clamp(P.FloorHeight, 0.0, Eave * 0.3);
			const double PlatO = FMath::Max(P.PlatformOverhang, ColR);

			auto BoundaryX = [&](int32 i) { return (i <= 0) ? ColR : (W - ColR); };
			// 懸山: each 檁 out to the 博縫板 (1 cm into it), a 燕尾枋 under the overhang.
			const double GableReach = ColR + FMath::Max(P.GableOverhang - 0.5, 0.0);
			const double BoardH = HutongCanon::Frame::BoardHeight, PurlinD = HutongCanon::Frame::PurlinDiameter;
			const double HangY = 0.0;                 // 垂蓮柱, at the beams' front ends
			const double FrontY = P.GetWallLineY();  // 前簷柱, in the wall line
			const double RearY = P.GetRearColumnY(); // 後簷柱
			const double ValleyY = P.GetValleyY();

			// 臺基 over the whole depth, a flight down each face.
			AppendGatePlatform(Mesh, P, W, D, Floor, PlatO, BoundaryX(0), BoundaryX(1));
			Shell::AppendSteps(Mesh, BoundaryX(0), BoundaryX(1), D + PlatO, 1.0,
				P.GetStepCount(), P.StepTread, Floor);

			FSlotScope WoodTag(Mesh, MatSlot_Wood);
			const double ColTopR = Frame::TaperedTopRadius(ColR, P.GetColumnHeight(), P.ColumnTaperRatio);
			const double PT = FMath::Max(2.0 * ColR * 0.8, 8.0);
			const double FrameT = FMath::Clamp(P.DoorFrameThickness, 2.0, 2.0 * ColR);
			const double BeamBottom = FMath::Clamp(
				P.GetDoorHeadHeight() + FMath::Max(0.4 * PT, FrameT),
				Floor + 60.0, Eave - 1.2 * PT);
			const double BeamTop = FMath::Min(BeamBottom + PT, Eave);

			// Both rows, an 額枋 each, 麻葉抱頭梁 from the rear row to the hanging posts.
			for (int32 i = 0; i <= 1; ++i)
			{
				Frame::AppendSquareColumn(Mesh, BoundaryX(i), FrontY, Frame::SquareHalfWidth(ColR), Frame::SquareHalfWidth(ColTopR), Floor, Eave);
				Frame::AppendSquareColumn(Mesh, BoundaryX(i), RearY, Frame::SquareHalfWidth(ColR), Frame::SquareHalfWidth(ColTopR), Floor, Eave);
			}
			Frame::AppendArchitrave(Mesh, BoundaryX(0), BoundaryX(1), FrontY, PT, BeamBottom, Eave);
			Frame::AppendArchitrave(Mesh, BoundaryX(0), BoundaryX(1), RearY, PT, BeamBottom, Eave);
			Frame::AppendTieBeams(Mesh, BoundaryX, 1, HangY, RearY, PT, BeamBottom, Eave, 1.0);

			// Face: deep cross beam, 花板 above to the eave, 雀替 in the corners.
			AppendBox(Mesh,
				FVector3d(BoundaryX(0), HangY - 0.6 * PT, BeamBottom),
				FVector3d(BoundaryX(1), HangY + 0.6 * PT, BeamTop));
			if (P.bHasFriezePanel && CeilZ > BeamTop)
			{
				AppendBox(Mesh,
					FVector3d(BoundaryX(0), HangY - 0.34 * PT, BeamTop),
					FVector3d(BoundaryX(1), HangY + 0.34 * PT, CeilZ));
			}
			if (P.bHasBrackets)
			{
				const double Reach = FMath::Clamp(P.BracketReach, 4.0, 0.4 * (BoundaryX(1) - BoundaryX(0)));
				for (int32 s = 0; s < 3; ++s)
				{
					const double A = Reach * (3 - s) / 3.0;
					const double Drop = 0.5 * Reach * (s + 1) / 3.0;
					AppendBox(Mesh,
						FVector3d(BoundaryX(0),     HangY - 0.44 * PT, BeamBottom - Drop),
						FVector3d(BoundaryX(0) + A, HangY + 0.44 * PT, BeamBottom));
					AppendBox(Mesh,
						FVector3d(BoundaryX(1) - A, HangY - 0.44 * PT, BeamBottom - Drop),
						FVector3d(BoundaryX(1),     HangY + 0.44 * PT, BeamBottom));
				}
			}

			// 垂蓮柱 at the front only; beam rear ends sit on the rear columns.
			if (P.bHasHangingPosts)
			{
				const double PostR = FMath::Max(0.5 * P.HangingPostDiameter, 3.0);
				const double Drop = FMath::Max(P.HangingPostDrop, 4.0 * PostR);
				if (BeamBottom - Drop > Floor + 40.0)
				{
					for (int32 i = 0; i <= 1; ++i)
					{
						Frame::AppendHangingPost(Mesh, BoundaryX(i), HangY, BeamBottom, Drop, PostR, P.BudFraction);
					}
				}
			}

			// Sides between rows (Fig 5-2-2): a 罩面枋 each above head height (the walk passes under), lattice
			// band from it up to the 麻葉抱頭梁.
			{
				const double RailH = FMath::Max(0.45 * PT, 6.0);
				const double RailZ = Floor + HutongCanon::Openings::WalkerHeightCm + 15.0;
				const double Y0 = FrontY + ColR, Y1 = RearY - ColR;
				if (RailZ + RailH < BeamBottom - 2.0 && Y1 - Y0 > 40.0)
				{
					const double BarW = 2.5;
					for (int32 i = 0; i <= 1; ++i)
					{
						const double X = BoundaryX(i);
						AppendBox(Mesh, FVector3d(X - 0.4 * PT, Y0, RailZ), FVector3d(X + 0.4 * PT, Y1, RailZ + RailH));
						const double BandZ0 = RailZ + RailH, BandZ1 = BeamBottom;
						if (BandZ1 - BandZ0 >= 12.0)
						{
							// 步步錦 as a grid: uprights at a hand's spacing, one mid rail.
							const int32 N = FMath::Max(FMath::RoundToInt32((Y1 - Y0) / 14.0), 2);
							for (int32 k = 1; k < N; ++k)
							{
								const double Y = Y0 + (Y1 - Y0) * k / N;
								AppendBox(Mesh, FVector3d(X - 0.25 * PT, Y - 0.5 * BarW, BandZ0), FVector3d(X + 0.25 * PT, Y + 0.5 * BarW, BandZ1));
							}
							const double Mid = 0.5 * (BandZ0 + BandZ1);
							AppendBox(Mesh, FVector3d(X - 0.25 * PT, Y0, Mid - 0.5 * BarW), FVector3d(X + 0.25 * PT, Y1, Mid + 0.5 * BarW));
						}
					}
				}
			}

			// 攢邊門 in the wall line, 餘塞板 either side.
			const double BayW = BoundaryX(1) - BoundaryX(0);
			const double DoorW = BayW * FMath::Clamp(P.DoorWidthFraction, 0.2, 1.0);
			const double JambL = BoundaryX(0) + 0.5 * (BayW - DoorW);
			const double JambR = JambL + DoorW;
			const double Head = FMath::Clamp(P.GetDoorHeadHeight(), Floor + 80.0, BeamBottom - FrameT);
			{
				FHutongDoorAssembly Door;
				Door.OpeningX0 = JambL;
				Door.OpeningX1 = JambR;
				Door.FrontY = FrontY - ColR;
				Door.BackY = FrontY + ColR;
				Door.BottomZ = Floor;
				Door.LeafTopZ = Head;
				Door.JambTopZ = BeamBottom;
				Door.FrameThickness = FrameT;
				Door.ThresholdHeight = P.ThresholdHeight;
				Door.PegCount = P.DoorPegCount;
				// Opens inward, between the rows.
				Door.bUseLeafAngles = true;
				Door.LeftLeafAngleDeg = P.bDoorLeavesOpen ? 90.0 : 0.0;
				Door.RightLeafAngleDeg = P.bDoorLeavesOpen ? 90.0 : 0.0;
				Door.StoneReveal = P.DoorStones.bEnabled ? DoorStoneReveal(FrameT, DoorW) : 0.0;
				AppendDoorAssembly(Mesh, Door);
			}
			const double PlaneY0 = FrontY - 0.35 * ColR;
			const double PlaneY1 = FrontY + 0.35 * ColR;
			if (JambL - FrameT > BoundaryX(0))
			{
				AppendBox(Mesh, FVector3d(BoundaryX(0), PlaneY0, Floor), FVector3d(JambL - FrameT, PlaneY1, BeamBottom));
			}
			if (BoundaryX(1) > JambR + FrameT)
			{
				AppendBox(Mesh, FVector3d(JambR + FrameT, PlaneY0, Floor), FVector3d(BoundaryX(1), PlaneY1, BeamBottom));
			}

			// 屏門 across the rear row, column to column (背立面, Fig 5-2-1): four leaves under a head rail. Shut,
			// it turns the way aside onto the walk; open, each outer leaf folds flat on its neighbour against the column.
			const int32 ScreenFirstTri = Mesh.MaxTriangleID();
			// Open, the middle two leaves' width is the way through.
			{
				const double X0 = BoundaryX(0) + ColR;
				const double X1 = BoundaryX(1) - ColR;
				const double LeafW = 0.25 * (X1 - X0);
				const double LeafT = FMath::Clamp(0.25 * ColR, 3.0, 5.0);
				const double LeafTop = BeamBottom - FrameT;
				AppendBox(Mesh, FVector3d(X0, RearY - 0.5 * ColR, LeafTop), FVector3d(X1, RearY + 0.5 * ColR, BeamBottom));
				if (P.bScreenDoorOpen)
				{
					// Two leaves stacked flat per jamb, a leaf's thickness either side of the plane.
					for (int32 k = 0; k < 2; ++k)
					{
						const double Y0 = RearY - (1 - k) * LeafT, Y1 = Y0 + LeafT;
						AppendBox(Mesh, FVector3d(X0, Y0, Floor), FVector3d(X0 + LeafW, Y1, LeafTop));
						AppendBox(Mesh, FVector3d(X1 - LeafW, Y0, Floor), FVector3d(X1, Y1, LeafTop));
					}
				}
				else
				{
					// Shut: each leaf its own panel, hairline between.
					for (int32 k = 0; k < 4; ++k)
					{
						AppendBox(Mesh, FVector3d(X0 + k * LeafW + 0.3, RearY - 0.5 * LeafT, Floor),
							FVector3d(X0 + (k + 1) * LeafW - 0.3, RearY + 0.5 * LeafT, LeafTop));
					}
				}
			}
			const int32 ScreenEndTri = Mesh.MaxTriangleID();

			WoodTag.Close();
			SetMaterialIDForTriangleRange(Mesh, ScreenFirstTri, ScreenEndTri, MatSlot_DoorPaint);

			{
				FSlotScope StoneTag(Mesh, MatSlot_Stone);
				AppendDoorStonePair(Mesh, P.DoorStones, JambL, JambR,
					FrontY - ColR, FrontY + ColR, Floor, FrameT,
					/*MaxOutward*/ FMath::Max(JambL - BoundaryX(0), 4.0));
				StoneTag.Close();
			}

			// 墊板 and 檐檁 over the hanging posts' beam and the rear row, under the lifted roofs.
			if (RoofZ > Eave)
			{
				Frame::AppendEaveStack(Mesh, BoundaryX(0), BoundaryX(1), HangY, 2.0 * ColR, Eave, BoardH, PurlinD, GableReach, true);
				Frame::AppendEaveStack(Mesh, BoundaryX(0), BoundaryX(1), RearY, 2.0 * ColR, Eave, BoardH, PurlinD, GableReach, true);
			}

			// 殿: gable over the front, hanging posts to 天溝, with 清水脊 and 蠍子尾.
			const double O = FMath::Max(P.RoofOverhang, 0.0);
			{
				const double Span = ValleyY - HangY;
				Shell::FRoofParams Roof;
				Roof.FrontOverhang = O;
				Roof.RearOverhang = 0.0;
				Roof.UndersideRise = P.GetUndersideRise();
				if (P.bExposedFrame) Roof.ShellCover = HutongCanon::Frame::RoofCover * 2.0 * ColR;
				Roof.GableOverhang = FMath::Max(P.GableOverhang, 0.0);
				Roof.Rise = P.GetRoofRise();
				Roof.Section = Jiajia::MakeSection(EHutongPurlins::Three, 0.5 * Span, O, 0.0);
				Roof.SlopeSegments = 8;
				Roof.FasciaDepth = P.EaveFasciaDepth;
				Roof.FasciaWidth = P.EaveFasciaWidth;
				Roof.RafterSection = P.RafterEndSection;
				Roof.RafterSpacing = P.RafterEndSpacing;
				Roof.bHasRidgeCourse = P.bHasRidgeCourse;
				Roof.RidgeCourseHeight = P.RidgeCourseHeight;
				Roof.RidgeCourseWidth = P.RidgeCourseWidth;
				Roof.RidgeEndKick = P.RidgeEndKick;
				Roof.RakeDepth = 0.0;
				Roof.bWoodenGable = true;
				Roof.BargeboardDepth = HutongCanon::Roof::BargeboardInD * FMath::Max(P.ColumnDiameter, 2.0);
				Roof.BargeboardThickness = HutongCanon::Roof::BargeboardThicknessInD * FMath::Max(P.ColumnDiameter, 2.0);
				Roof.bTileRuns = P.bHasTileRuns;
				const int32 V0 = Mesh.MaxVertexID();
				Shell::AppendGableRoof(Mesh, W, Span, RoofZ, Roof);
				if (P.bExposedFrame) AppendGateFrame(Mesh, P, Span, Eave, Floor, { BoundaryX(0), BoundaryX(1) }, W, Roof, RoofZ);
				TransformVerticesFrom(Mesh, V0, FTransform(FVector(0.0, HangY, 0.0)));
			}

			// 卷: rolled roof from 天溝 over the rear row, crown level with the 殿's fold. Built eave-front and
			// turned end for end so the eave course and 椽頭 face the court.
			{
				const double Span = RearY - ValleyY;
				Shell::FRoofParams Roof;
				Roof.FrontOverhang = O;
				Roof.RearOverhang = 0.0;
				Roof.UndersideRise = P.GetUndersideRise();
				if (P.bExposedFrame) Roof.ShellCover = HutongCanon::Frame::RoofCover * 2.0 * ColR;
				Roof.GableOverhang = FMath::Max(P.GableOverhang, 0.0);
				Roof.Section = Jiajia::MakeSection(EHutongPurlins::Three, 0.5 * Span, O, 0.6);
				Roof.Rise = P.GetRoofRise() / Roof.Section.CrownFactor();
				Roof.SlopeSegments = 8;
				Roof.FasciaDepth = P.EaveFasciaDepth;
				Roof.FasciaWidth = P.EaveFasciaWidth;
				Roof.RafterSection = P.RafterEndSection;
				Roof.RafterSpacing = P.RafterEndSpacing;
				Roof.bHasRidgeCourse = false;
				Roof.RakeDepth = 0.0;
				Roof.bWoodenGable = true;
				Roof.BargeboardDepth = HutongCanon::Roof::BargeboardInD * FMath::Max(P.ColumnDiameter, 2.0);
				Roof.BargeboardThickness = HutongCanon::Roof::BargeboardThicknessInD * FMath::Max(P.ColumnDiameter, 2.0);
				Roof.bTileRuns = P.bHasTileRuns;
				const int32 V0 = Mesh.MaxVertexID();
				Shell::AppendGableRoof(Mesh, W, Span, RoofZ, Roof);
				if (P.bExposedFrame) AppendGateFrame(Mesh, P, Span, Eave, Floor, { BoundaryX(0), BoundaryX(1) }, W, Roof, RoofZ);
				YawVerticesFrom(Mesh, V0, FVector2d(0.5 * W, 0.5 * Span), 180.0);
				TransformVerticesFrom(Mesh, V0, FTransform(FVector(0.0, ValleyY, 0.0)));
			}
		}
	}

	void BuildInnerGate(FDynamicMesh3& Mesh, const FHutongInnerGateParams& P)
	{
		using namespace HutongMeshUtils;

		if (P.IsHallAndRoll())
		{
			BuildHallAndRoll(Mesh, P);
			return;
		}

		const double W = FMath::Max(P.Width, 1.0);
		const double D = FMath::Max(P.Depth, 1.0);
		const double Eave = P.GetEaveHeight();
		// Roof on the frame's line; posts and beams at the column top, 花板 up to the ceiling.
		const double RoofZ = Eave + P.GetRoofLift();
		const double CeilZ = RoofZ + P.GetUndersideRise();
		const double ColR = FMath::Min(P.GetColumnRadius(), 0.2 * FMath::Min(W, D));
		const double Floor = FMath::Clamp(P.FloorHeight, 0.0, Eave * 0.3);
		const double PlatO = FMath::Max(P.PlatformOverhang, ColR);

		// One bay: boundaries at the ends, inset so posts sit on the platform.
		auto BoundaryX = [&](int32 i) { return (i <= 0) ? ColR : (W - ColR); };
		// 懸山: each 檁 out to the 博縫板 (1 cm into it), a 燕尾枋 under the overhang.
		const double GableReach = ColR + FMath::Max(P.GableOverhang - 0.5, 0.0);
		const double BoardH = HutongCanon::Frame::BoardHeight, PurlinD = HutongCanon::Frame::PurlinDiameter;

		// 獨立柱擔梁式.
		const double FrontY = 0.0;
		const double BackY = D;
		const double MidY = 0.5 * D;

		// 1) 臺基, 踏跺 down both sides.
		AppendGatePlatform(Mesh, P, W, D, Floor, PlatO, BoundaryX(0), BoundaryX(1));
		Shell::AppendSteps(Mesh, BoundaryX(0), BoundaryX(1), D + PlatO, 1.0,
			P.GetStepCount(), P.StepTread, Floor);

		FSlotScope WoodTag(Mesh, MatSlot_Wood);
		const double ColTopR = Frame::TaperedTopRadius(ColR, P.GetColumnHeight(), P.ColumnTaperRatio);
		const double PT = FMath::Max(2.0 * ColR * 0.8, 8.0);
		const double FrameT = FMath::Clamp(P.DoorFrameThickness, 2.0, 2.0 * ColR);
		// Beam underside is the head's ceiling: must clear the frame and ride the head.
		const double BeamBottom = FMath::Clamp(
			P.GetDoorHeadHeight() + FMath::Max(0.4 * PT, FrameT),
			Floor + 60.0, Eave - 1.2 * PT);

		// 2) One column pair on the centre line, 額枋 across.
		for (int32 i = 0; i <= 1; ++i)
		{
			Frame::AppendSquareColumn(Mesh, BoundaryX(i), MidY, Frame::SquareHalfWidth(ColR), Frame::SquareHalfWidth(ColTopR), Floor, Eave);
		}
		Frame::AppendArchitrave(Mesh, BoundaryX(0), BoundaryX(1), MidY, PT, BeamBottom, Eave);

		// 擔梁 over each column, full depth.
		Frame::AppendTieBeams(Mesh, BoundaryX, 1, FrontY, BackY, PT, BeamBottom, Eave, 1.0);

		// Cross stack, identical on both faces.
		const double BeamTop = FMath::Min(BeamBottom + PT, Eave);
		const double BracketReach = FMath::Clamp(P.BracketReach, 4.0,
			0.4 * (BoundaryX(1) - BoundaryX(0)));

		auto AppendCrossFace = [&](double FaceY)
		{
			// Deeper in Y than the 擔梁 it laps.
			AppendBox(Mesh,
				FVector3d(BoundaryX(0), FaceY - 0.6 * PT, BeamBottom),
				FVector3d(BoundaryX(1), FaceY + 0.6 * PT, BeamTop));

			// 花板 narrower than the beam, recessed on both faces.
			if (P.bHasFriezePanel && CeilZ > BeamTop)
			{
				AppendBox(Mesh,
					FVector3d(BoundaryX(0), FaceY - 0.34 * PT, BeamTop),
					FVector3d(BoundaryX(1), FaceY + 0.34 * PT, CeilZ));
			}

			// 雀替 stepped, not scrolled (as the shopfront's 花牙子).
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

		// 3) 垂蓮柱 at the 擔梁's four ends.
		if (P.bHasHangingPosts)
		{
			const double PostR = FMath::Max(0.5 * P.HangingPostDiameter, 3.0);
			const double Drop = FMath::Max(P.HangingPostDrop, 4.0 * PostR);
			// Only with air under the beam; a bud resting on the platform is not hanging.
			if (BeamBottom - Drop > Floor + 40.0)
			{
				for (int32 i = 0; i <= 1; ++i)
				{
					Frame::AppendHangingPost(Mesh, BoundaryX(i), FrontY, BeamBottom, Drop, PostR, P.BudFraction);
					Frame::AppendHangingPost(Mesh, BoundaryX(i), BackY,  BeamBottom, Drop, PostR, P.BudFraction);
				}
			}
		}

		// 4) Doors on the centre column line, mid-gate.
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
			// Driven by angle, not clearance.
			Door.bUseLeafAngles = true;
			Door.LeftLeafAngleDeg = P.bDoorLeavesOpen ? -90.0 : 0.0;
			Door.RightLeafAngleDeg = P.bDoorLeavesOpen ? -90.0 : 0.0;
			Door.StoneReveal = P.DoorStones.bEnabled
				? DoorStoneReveal(FrameT, DoorW) : 0.0;

			AppendDoorAssembly(Mesh, Door);
		}

		// 餘塞板 from each jamb to the column centre; 走馬板 across the plane above the head.
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

		WoodTag.Close();
		// Bounded: the 餘塞板 above are appended later and stay joinery.

		// 門枕石 at the jamb feet, in the door plane on the centre line.
		{
			FSlotScope StoneTag(Mesh, MatSlot_Stone);
			AppendDoorStonePair(Mesh, P.DoorStones, JambL, JambR,
				MidY - ColR, MidY + ColR, Floor, FrameT,
				/*MaxOutward*/ FMath::Max(JambL - BoundaryX(0), 4.0));
			StoneTag.Close();
		}

		// 5) Roof.
		Shell::FRoofParams Roof;
		Roof.FrontOverhang = FMath::Max(P.RoofOverhang, 0.0);
		Roof.RearOverhang = FMath::Max(P.RoofOverhang, 0.0);
		Roof.UndersideRise = P.GetUndersideRise();
		if (P.bExposedFrame) Roof.ShellCover = HutongCanon::Frame::RoofCover * 2.0 * ColR;
		Roof.GableOverhang = FMath::Max(P.GableOverhang, 0.0);
		Roof.Rise = P.GetRoofRise();
		// 三檁: the 獨立柱 gate is one 步架 deep, matching its depth band.
		Roof.Section = Jiajia::MakeSection(
			EHutongPurlins::Three, 0.5 * D, Roof.FrontOverhang, P.RoofApexRoll);
		Roof.SlopeSegments = 8;
		Roof.FasciaDepth = P.EaveFasciaDepth;
		Roof.FasciaWidth = P.EaveFasciaWidth;
		Roof.RafterSection = P.RafterEndSection;
		Roof.RafterSpacing = P.RafterEndSpacing;
		// 正脊 with 蠍子尾, as a gate house, unlike a dwelling.
		Roof.bHasRidgeCourse = P.bHasRidgeCourse;
		Roof.RidgeCourseHeight = P.RidgeCourseHeight;
		Roof.RidgeCourseWidth = P.RidgeCourseWidth;
		Roof.RidgeEndKick = P.RidgeEndKick;

		// Spans front beam line to back; the ends are wooden 博風板, not a brick rake.
		Roof.RakeDepth = 0.0;
		Roof.bWoodenGable = true;
		Roof.BargeboardDepth = HutongCanon::Roof::BargeboardInD * FMath::Max(P.ColumnDiameter, 2.0);
		Roof.BargeboardThickness = HutongCanon::Roof::BargeboardThicknessInD * FMath::Max(P.ColumnDiameter, 2.0);
		Roof.bTileRuns = P.bHasTileRuns;
		if (RoofZ > Eave)
		{
			Frame::AppendEaveStack(Mesh, BoundaryX(0), BoundaryX(1), FrontY, 2.0 * ColR, Eave, BoardH, PurlinD, GableReach, true);
			Frame::AppendEaveStack(Mesh, BoundaryX(0), BoundaryX(1), BackY, 2.0 * ColR, Eave, BoardH, PurlinD, GableReach, true);
		}
		if (P.bExposedFrame) AppendGateFrame(Mesh, P, D, Eave, Floor, { BoundaryX(0), BoundaryX(1) }, W, Roof, RoofZ);
		Shell::AppendGableRoof(Mesh, W, D, RoofZ, Roof);
	}
}
