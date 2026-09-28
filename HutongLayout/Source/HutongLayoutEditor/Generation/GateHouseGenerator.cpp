#include "Generation/GateHouseGenerator.h"
#include "Generation/FrameGenerator.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
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
		// Roof on the frame's line; walls to the ceiling under it; columns and 額枋 at the column top.
		const double RoofZ = Eave + P.GetRoofLift();
		const double CeilZ = RoofZ + P.GetUndersideRise();
		const double T = FMath::Clamp(P.WallThickness, 1.0, FMath::Min(W, D) * 0.25);
		const double Floor = FMath::Clamp(P.FloorHeight, 0.0, Eave * 0.4);
		const double ColR = FMath::Max(0.5 * P.GetColumnDiameter(), 1.0);

		// One bay (一間). Wall frame: the columns stand proud of the side walls' passage faces.
		const double ColX = P.HasWallFrame()
			? FMath::Max(T - (1.0 - 2.0 * HutongCanon::Gate::ColumnProudShare) * ColR, ColR) : ColR;
		auto BoundaryX = [&](int32 i) { return (i <= 0) ? ColX : (W - ColX); };

		const double BaseH = FMath::Clamp(P.GetBaseCourseHeight(), 0.0, (Eave - Floor) * 0.6);
		const double BaseP = FMath::Max(P.BaseCourseProjection, 0.0);
		const bool bBaseCourse = (BaseH > 0.0 && BaseP > 0.0);
		const double WallBottom = bBaseCourse ? Floor + BaseH : Floor;

		double DoorY = FMath::Clamp(P.GetDoorPlaneFraction() * D, 0.0, FMath::Max(D - T - 20.0, 0.0));
		// Snap a recess shallower than the wall flush, avoiding near-coincident faces.
		if (DoorY < T) DoorY = 0.0;
		const bool bRecessed = (DoorY > 0.0);

		// Roof, eave course, 正脊, built last; the shell helper tags the roof slot. Set up first: the door plane
		// rises to its underside.
		Shell::FRoofParams Roof;
		Roof.FrontOverhang = P.GetRoofOverhang();
		Roof.UndersideRise = P.GetUndersideRise();
		// Symmetric: a gate's back is the courtyard, not a lane.
		Roof.RearOverhang = P.GetRoofOverhang();
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
		Roof.bTileRuns = P.bHasTileRuns;
		Roof.RafterEndInset = P.bHasChitou ? T : 0.0;
		Roof.GableWallThickness = T;
		if (P.bExposedFrame) Roof.ShellCover = HutongCanon::Frame::RoofCover * P.GetColumnDiameter();
		// The door plane to the underside: level to its lower face, sloped over the thickness above that.
		const double DoorTop = FMath::Min(Shell::UndersideAt(Roof, D, RoofZ, DoorY), Shell::UndersideAt(Roof, D, RoofZ, DoorY + T));

		// 1) 臺基, 踏跺 under the doorway.
		const double PlatO = FMath::Max(P.PlatformOverhang, ColR);
		// Flush on the flanks; the 下鹼 runs down past it.
		const double PlatSide = 0.0;
		// 踏跺 between the piers (圖5-1-4.2).
		Shell::AppendPlatform(Mesh, W, D, Floor, PlatO, PlatSide,
			P.StepCount, P.StepTread, T, W - T);

		// 2) 下鹼 on the side walls only; no back wall.
		{
			FSlotScope BaseTag(Mesh, MatSlot_BaseCourse);
			Shell::AppendBaseCourseU(Mesh, W, D, T, Floor, BaseH, BaseP,
				/*bIncludeRear*/ false, /*bToGround*/ true);
			BaseTag.Close();
		}

		AppendBox(Mesh, FVector3d(0.0, 0.0, WallBottom), FVector3d(T, D, CeilZ));
		AppendBox(Mesh, FVector3d(W - T, 0.0, WallBottom), FVector3d(W, D, CeilZ));

		// 踏跺 on the courtyard side too.
		Shell::AppendSteps(Mesh, T, W - T, D + PlatSide, 1.0,
			P.StepCount, P.StepTread, Floor);

		FSlotScope WoodTag(Mesh, MatSlot_Wood);
		const double ColOvershoot = 1.0;
		// Resolved before the frame.
		const double DoorHead = FMath::Clamp(P.GetDoorHeadHeight(), Floor + 60.0, Eave - 10.0);
		const double ColTopR = Frame::TaperedTopRadius(ColR, P.GetColumnHeight(), P.ColumnTaperRatio);

		// 檐柱 on the front edge.
		Frame::AppendColumnRow(Mesh, BoundaryX, 1, 0.0, ColR, ColTopR, 0.0, Eave, ColOvershoot);
		// Wall frame: the rear 檐柱 show on the passage walls too.
		if (P.HasWallFrame())
		{
			Frame::AppendColumnRow(Mesh, BoundaryX, 1, D, ColR, ColTopR, 0.0, Eave, ColOvershoot);
		}
		// 額枋 at the column top; the 檻框 fills the bay under it. 如意門: it stands inside the brick screen.
		// 清式營造則例 表四: 檐枋 one 柱徑 high, ⅘ thick.
		const double ArchitraveH = FMath::Min(FMath::Max(HutongCanon::Frame::EaveTieHeight * 2.0 * ColR, 8.0), Eave - DoorHead);
		const double LintelZ = Eave - ArchitraveH;
		if (!P.HasBrickScreen())
		{
			Frame::AppendArchitrave(Mesh, BoundaryX(0), BoundaryX(1), 0.0,
				FMath::Max(HutongCanon::Frame::EaveTieThickness * 2.0 * ColR, 6.0), LintelZ, Eave);
		}

		// Wall frame shown: its 中柱 or 金柱 and beams come with the cross frames below.
		if (bRecessed && !(P.HasWallFrame() && P.bExposedFrame))
		{
			Frame::AppendColumnRow(Mesh, BoundaryX, 1, DoorY, ColR, ColTopR, 0.0, Eave, ColOvershoot);
			Frame::AppendTieBeams(Mesh, BoundaryX, 1, 0.0, DoorY, FMath::Max(HutongCanon::Frame::ThroughTieThickness * 2.0 * ColR, 6.0),
				DoorHead, Eave, ColOvershoot);
		}

		// 雀替 under the front 額枋 of a recessed gate (圖5-1-3; "as on the 廣亮大門"): a board in the column
		// plane, root buried in the column, underside sagging from the tip down to it.
		if (bRecessed && !P.HasBrickScreen())
		{
			namespace G = HutongCanon::Gate;
			FSlotScope QuetiTag(Mesh, MatSlot_Paint);
			const double Span = BoundaryX(1) - BoundaryX(0) - 2.0 * ColR;
			const double Reach = G::QuetiReachShare * Span;
			const double Drop = FMath::Min(G::QuetiHeightInD * 2.0 * ColR, LintelZ - DoorHead);
			const double Root = 0.5 * ColR;
			const double HalfT = 0.5 * G::QuetiThicknessInD * 2.0 * ColR;
			if (Reach > 5.0 && Drop > 3.0)
			{
				// Along the beam from the column axis (s), height above the 額枋's underside.
				TArray<FVector2d> Profile = { { Root, 1.0 }, { ColR + Reach, 1.0 } };
				const int32 Steps = 8;
				for (int32 k = 0; k <= Steps; ++k)
				{
					const double U = 1.0 - double(k) / Steps;
					Profile.Add({ ColR + U * Reach, -Drop * FMath::Pow(1.0 - U, G::QuetiSag) });
				}
				Profile.Add({ Root, -Drop });
				for (const int32 Side : { 0, 1 })
				{
					const int32 First = Mesh.MaxVertexID();
					AppendYZPrism(Mesh, Profile, -HalfT, HalfT);
					// Profile's s onto +X from the left column, -X from the right; no mirroring.
					TransformVerticesFrom(Mesh, First, FTransform(FQuat(FVector::ZAxisVector, Side == 0 ? -HALF_PI : HALF_PI),
						FVector(BoundaryX(Side), 0.0, LintelZ)));
				}
			}
			QuetiTag.Close();
		}

		// Doorway, centred in the bay.
		const double BayW = BoundaryX(1) - BoundaryX(0);
		const double FrameT = FMath::Clamp(P.DoorFrameThickness, 2.0, T);
		const double StoneShare = P.GetDoorStones().bEnabled ? 1.0 : 0.0;
		// The doorway of a given clear width, centred in the bay, leaves square to the plane.
		auto MakeDoor = [&](double OpenW)
		{
			FHutongDoorAssembly D;
			D.OpeningX0 = BoundaryX(0) + 0.5 * (BayW - OpenW);
			D.OpeningX1 = D.OpeningX0 + OpenW;
			D.FrontY = DoorY;
			D.BackY = DoorY + T;
			D.BottomZ = Floor;
			D.LeafTopZ = DoorHead;
			D.JambTopZ = FMath::Min(DoorHead + FrameT, Eave);
			D.FrameThickness = FrameT;
			D.ThresholdHeight = P.ThresholdHeight;
			D.PegCount = P.GetDoorPegCount();
			D.StoneReveal = StoneShare * DoorStoneReveal(FrameT, OpenW);
			D.bLeavesOpen = P.bLeavesOpen;
			D.bUseLeafAngles = P.bLeavesOpen;
			D.LeftLeafAngleDeg = D.RightLeafAngleDeg = -90.0;
			return D;
		};
		// The swing that leaves the widest way: past square the pivot's set-in brings the hinge edge back in.
		auto WidestSwing = [&](FHutongDoorAssembly D, double& OutDeg)
		{
			double Best = -BIG_NUMBER;
			OutDeg = 90.0;
			for (double Deg = 60.0; Deg <= 170.0; Deg += 1.0)
			{
				D.LeftLeafAngleDeg = D.RightLeafAngleDeg = -Deg;
				const double Clear = LeafClearWidth(D);
				if (Clear > Best) { Best = Clear; OutDeg = Deg; }
			}
			return Best;
		};

		// 如意門: the figure's share of the span between the piers, widened (within the bay) until its leaves,
		// swung their widest, leave Min Clear Width to walk through. The mesh is the collision.
		double DoorW = FMath::Clamp(P.DoorWidthFraction, 0.1, 1.0) * BayW;
		if (P.HasBrickScreen())
		{
			DoorW = FMath::Min(HutongCanon::Gate::RuyiDoorShare * (W - 2.0 * T), BayW);
			double Deg = 90.0;
			while (DoorW < BayW && WidestSwing(MakeDoor(DoorW), Deg) < P.MinClearWidth)
			{
				DoorW = FMath::Min(DoorW + 2.0, BayW);
			}
		}
		const double ClearX0 = BoundaryX(0) + 0.5 * (BayW - DoorW);
		const double ClearX1 = ClearX0 + DoorW;

		FHutongDoorAssembly Door = MakeDoor(DoorW);
		// Leaves swing inward, each by its own angle from the seed; too little way left, both take the widest swing.
		if (P.bLeavesOpen)
		{
			FRandomStream Rand(P.RandomSeed);
			const double LoDeg = FMath::Min(P.AjarAngleMin, P.AjarAngleMax);
			const double HiDeg = FMath::Max(P.AjarAngleMin, P.AjarAngleMax);
			double LeftDeg = FMath::Lerp(LoDeg, HiDeg, static_cast<double>(Rand.FRand()));
			double RightDeg = FMath::Lerp(LoDeg, HiDeg, static_cast<double>(Rand.FRand()));
			// Negative = inward, into the passage, not over the lane.
			auto Swing = [&]() { Door.LeftLeafAngleDeg = -LeftDeg; Door.RightLeafAngleDeg = -RightDeg; };
			Swing();
			if (P.MinClearWidth > 0.0 && LeafClearWidth(Door) < P.MinClearWidth)
			{
				double Deg = 90.0;
				WidestSwing(Door, Deg);
				LeftDeg = RightDeg = Deg;
				Swing();
			}
		}
		AppendDoorAssembly(Mesh, Door);

		WoodTag.Close();

		// 門墩 at the jamb feet in the door plane (inside the recess on a recessed gate).
		{
			FSlotScope StoneTag(Mesh, MatSlot_Stone);
			AppendDoorStonePair(Mesh, P.GetDoorStones(), ClearX0, ClearX1,
				DoorY, DoorY + T, Floor, FrameT,
				/*MaxOutward*/ FMath::Max(ClearX0 - FrameT - T, 4.0));
			StoneTag.Close();
		}

		// 餘塞板 and 走馬板 fill the door plane round the doorway.
		const double FlankX0 = T;
		const double FlankX1 = W - T;
		const double JambOuterX0 = ClearX0 - FrameT;
		const double JambOuterX1 = ClearX1 + FrameT;

		if (P.HasBrickScreen())
		{
			// 如意門: brick bay with a doorway cut in it; masonry fill full height either side, on a 下鹼
			// proud of it like the side walls'.
			if (JambOuterX0 > FlankX0)
			{
				AppendBox(Mesh, FVector3d(FlankX0, DoorY, WallBottom),
					FVector3d(JambOuterX0, DoorY + T, DoorTop));
			}
			if (FlankX1 > JambOuterX1)
			{
				AppendBox(Mesh, FVector3d(JambOuterX1, DoorY, WallBottom),
					FVector3d(FlankX1, DoorY + T, DoorTop));
			}
			if (WallBottom > Floor)
			{
				FSlotScope BaseTag(Mesh, MatSlot_BaseCourse);
				if (JambOuterX0 > FlankX0)
				{
					AppendBox(Mesh, FVector3d(FlankX0, DoorY - BaseP, Floor),
						FVector3d(JambOuterX0, DoorY + T, WallBottom));
				}
				if (FlankX1 > JambOuterX1)
				{
					AppendBox(Mesh, FVector3d(JambOuterX1, DoorY - BaseP, Floor),
						FVector3d(FlankX1, DoorY + T, WallBottom));
				}
				BaseTag.Close();
			}
			if (DoorTop > Door.JambTopZ)
			{
				AppendBox(Mesh, FVector3d(JambOuterX0, DoorY, Door.JambTopZ),
					FVector3d(JambOuterX1, DoorY + T, DoorTop));
			}
			Shell::AppendWallToUnderside(Mesh, Roof, D, RoofZ, FlankX0, FlankX1, DoorY, DoorY + T, DoorTop);
		}
		else
		{
			// Timber gates, 檻框 (圖5-1-4.2): 抱框 at the piers, 中檻 across the bay at the door head, 走馬板
			// above it, 上檻 under the 額枋; 餘塞板 split by two 腰枋. Members full depth, panels set back.
			namespace G = HutongCanon::Gate;
			FSlotScope InfillTag(Mesh, MatSlot_Wood);
			const double PanelY = DoorY + FMath::Min(G::PanelRecessCm, 0.25 * T);
			const double BackY = DoorY + T;
			const double MidRailZ = Door.JambTopZ - FrameT;
			const double SillZ = Floor + FMath::Max(P.ThresholdHeight, 0.0);
			const double TopZ = FMath::Max(LintelZ, Door.JambTopZ);
			const double UpperSillZ = TopZ - G::UpperSillShare * (TopZ - Door.JambTopZ);

			auto Yusai = [&](double X0, double X1, double PostX0, double PostX1)
			{
				if (X1 - X0 < 1.0) return;
				AppendBox(Mesh, FVector3d(PostX0, DoorY, Floor), FVector3d(PostX1, BackY, TopZ));
				AppendBox(Mesh, FVector3d(X0, DoorY, MidRailZ), FVector3d(X1, BackY, Door.JambTopZ));
				AppendBox(Mesh, FVector3d(X0, PanelY, Floor), FVector3d(X1, BackY, MidRailZ));
				if (SillZ > Floor) AppendBox(Mesh, FVector3d(X0, DoorY, Floor), FVector3d(X1, BackY, SillZ));
				const double LeafH = MidRailZ - SillZ;
				for (const double* Rail : { G::YusaiRailUpper, G::YusaiRailLower })
				{
					AppendBox(Mesh, FVector3d(X0, DoorY, MidRailZ - Rail[1] * LeafH),
						FVector3d(X1, BackY, MidRailZ - Rail[0] * LeafH));
				}
			};
			Yusai(FlankX0 + FrameT, JambOuterX0, FlankX0, FlankX0 + FrameT);
			Yusai(JambOuterX1, FlankX1 - FrameT, FlankX1 - FrameT, FlankX1);

			if (TopZ > Door.JambTopZ)
			{
				AppendBox(Mesh, FVector3d(FlankX0 + FrameT, PanelY, Door.JambTopZ),
					FVector3d(FlankX1 - FrameT, BackY, UpperSillZ));
				AppendBox(Mesh, FVector3d(FlankX0 + FrameT, DoorY, UpperSillZ), FVector3d(FlankX1 - FrameT, BackY, TopZ));
			}
			if (DoorTop > TopZ)
			{
				AppendBox(Mesh, FVector3d(FlankX0, DoorY, TopZ), FVector3d(FlankX1, BackY, DoorTop));
			}
			Shell::AppendWallToUnderside(Mesh, Roof, D, RoofZ, FlankX0, FlankX1, DoorY, DoorY + T, DoorTop);

			InfillTag.Close();
		}

		// 門頭 (圖5-1-6, text with 圖5-1-5): a brick frieze across the bay from the door surround to the roof's
		// base — 掛落, 頭層檐, 連珠混, 半混, 蓋板, 欄板望柱 standing on it, dentils, top course. Ends buried
		// 1 cm in the piers, backs in the screen. Plain (素活): the same courses, the carving left off.
		const double HeadP = FMath::Max(P.DoorHeadProjection, 0.0);
		const double FriezeH = RoofZ - Door.JambTopZ;
		if (P.HasBrickScreen() && HeadP > 0.0 && FriezeH > 10.0)
		{
			namespace G = HutongCanon::Gate;
			const bool bCarved = P.bCarvedDoorHead;
			FSlotScope FriezeTag(Mesh, MatSlot_Body);
			const double X0 = FlankX0 - 1.0;
			const double X1 = FlankX1 + 1.0;
			const double Span = FlankX1 - FlankX0;
			const double Back = DoorY + 1.0;
			auto Face = [&](double Proud) { return DoorY - Proud * HeadP; };
			auto Strip = [&](double SX0, double SX1, double Proud, double Z0, double Z1)
			{
				AppendBox(Mesh, FVector3d(SX0, Face(Proud), Z0), FVector3d(SX1, Back, Z1));
			};
			double Z = Door.JambTopZ;
			auto Course = [&](double Share, double Proud)
			{
				const double Z1 = (Share > 0.0) ? Z + Share * FriezeH : RoofZ;
				Strip(X0, X1, Proud, Z, Z1);
				Z = Z1;
			};
			const double PostW = G::RuyiFriezePostShare * Span;
			const double PanelW = (Span - (G::RuyiFriezePanelCount + 1) * PostW) / G::RuyiFriezePanelCount;
			// Posts over the bay at the panels' spacing, the end ones buried in the piers.
			auto Posts = [&](double Proud, double Z0, double Z1)
			{
				for (int32 i = 0; i <= G::RuyiFriezePanelCount && PanelW > 0.0; ++i)
				{
					const double PX = FlankX0 + i * (PostW + PanelW);
					Strip(i == 0 ? X0 : PX, i == G::RuyiFriezePanelCount ? X1 : PX + PostW, Proud, Z0, Z1);
				}
			};

			// 掛落: carved, a sunk field framed top, bottom and at the posts.
			{
				const double Z0 = Z;
				Course(G::RuyiFriezeHanging, bCarved ? 0.5 * G::RuyiHangingProud : G::RuyiHangingProud);
				if (bCarved)
				{
					const double Border = G::RuyiHangingBorderShare * (Z - Z0);
					Strip(X0, X1, G::RuyiHangingProud, Z0, Z0 + Border);
					Strip(X0, X1, G::RuyiHangingProud, Z - Border, Z);
					Posts(G::RuyiHangingProud, Z0 + Border, Z - Border);
				}
			}

			Course(G::RuyiFriezeFirstEave, G::RuyiFirstEaveProud);

			// 連珠混: a string of beads on a course set back to the 頭層檐.
			{
				const double Z0 = Z;
				Course(G::RuyiFriezeBead, bCarved ? G::RuyiFirstEaveProud : G::RuyiBeadProud);
				const double H = Z - Z0;
				if (bCarved && H > 1.0)
				{
					const int32 Beads = FMath::Max(FMath::RoundToInt32(Span / (G::RuyiBeadPitchShare * H)), 1);
					const double Pitch = Span / Beads;
					for (int32 i = 0; i < Beads; ++i)
					{
						const double BX = FlankX0 + (i + 0.5) * Pitch;
						Strip(BX - 0.35 * H, BX + 0.35 * H, G::RuyiBeadProud, Z0 + 0.15 * H, Z - 0.15 * H);
					}
				}
			}

			// 半混: a half round swept across the bay.
			{
				const double Z0 = Z;
				const double Z1 = Z + G::RuyiFriezeHalfRound * FriezeH;
				constexpr int32 Arc = 6;
				TArray<FVector2d> Profile = { FVector2d(Back, Z0) };
				for (int32 k = 0; k <= Arc; ++k)
				{
					const double A = PI * k / Arc;
					const double Proud = FMath::Lerp(G::RuyiHalfRoundProud[0], G::RuyiHalfRoundProud[1], FMath::Sin(A));
					Profile.Add(FVector2d(Face(Proud), Z0 + 0.5 * (Z1 - Z0) * (1.0 - FMath::Cos(A))));
				}
				Profile.Add(FVector2d(Back, Z1));
				// (Y, Z) → world, swept along X: a rotation, never a mirror.
				const FMatrix Basis(FVector(0, 1, 0), FVector(0, 0, 1), FVector(1, 0, 0), FVector::ZeroVector);
				FTransform A(Basis), B(Basis);
				A.SetTranslation(FVector(X0, 0, 0));
				B.SetTranslation(FVector(X1, 0, 0));
				AppendSweptProfile(Mesh, Profile, { A, B });
				Z = Z1;
			}

			Course(G::RuyiFriezeCover, G::RuyiCoverProud);

			// 欄板望柱 standing back on the 蓋板: panels between posts, the 望柱 heads clear of the panels' top.
			{
				const double Z0 = Z;
				if (bCarved)
				{
					Course((1.0 - G::RuyiPostHeadShare) * G::RuyiFriezePanels, G::RuyiPanelProud);
					Course(G::RuyiPostHeadShare * G::RuyiFriezePanels, 0.5 * G::RuyiPanelProud);
					Posts(G::RuyiPostProud, Z0, Z);
				}
				else
				{
					Course(G::RuyiFriezePanels, G::RuyiPanelProud);
				}
			}

			const double DentilZ = Z;
			Course(G::RuyiFriezeDentils, 0.55);
			const double Pitch = G::RuyiDentilSpacingShare * Span;
			const int32 Dentils = FMath::Max(FMath::RoundToInt32(Span / Pitch), 1);
			const double Step = Span / Dentils;
			for (int32 i = 0; i < Dentils; ++i)
			{
				const double DX = FlankX0 + (i + 0.25) * Step;
				Strip(DX, DX + 0.5 * Step, G::RuyiDentilProud, DentilZ, Z);
			}
			Course(0.0, 1.0);
			FriezeTag.Close();
		}

		// 墀頭 at the corners; reach the eave, so need the roof's figures.
		if (P.bHasChitou)
		{
			FSlotScope PierTag(Mesh, MatSlot_Body);
			Shell::AppendChitou(Mesh, W, D, T, Floor, RoofZ,
				P.GetChitouProjection(), P.ChitouCorbelSteps, Roof, bBaseCourse ? BaseH : 0.0, BaseP);
			PierTag.Close();
		}

		// 墊板 and 檐檁 on the columns front and back, under the lifted roof; ends buried in the side walls.
		if (RoofZ > Eave)
		{
			// 如意門: the front pair stands inside the screen, under the 門頭.
			if (!P.HasBrickScreen())
			{
				Frame::AppendEaveStack(Mesh, 0.5 * T, W - 0.5 * T, 0.0, P.GetColumnDiameter(), Eave);
			}
			Frame::AppendEaveStack(Mesh, 0.5 * T, W - 0.5 * T, D, P.GetColumnDiameter(), Eave);
		}
		// 徹上明造: the 檁 of every purlin line between the side walls, under the shell; 中柱式, the cross frames
		// on the side walls too.
		if (P.bExposedFrame)
		{
			FRoofFrameOptions Options;
			Options.bSkipEaveLines = true;
			Options.Underside = [&](double Y) { return Shell::UndersideAt(Roof, D, RoofZ, Y); };
			TArray<double> FrameX;
			if (P.HasWallFrame()) FrameX = { BoundaryX(0), BoundaryX(1) };
			if (P.HasCentreColumn()) Options.CentreColumnDiameter = P.GetColumnDiameter() + HutongCanon::Frame::GableColumnExtraCm;
			Options.bGoldColumns = P.HasFrontVeranda();
			// In the side walls, behind the 廊心 (圖5-1-3 shows none).
			Options.bVerandaTies = !P.HasWallFrame();
			AppendRoofFrame(Mesh, FrameLayout::Make(P.GetColumnDiameter(), Eave, Floor, D, P.GetPurlins(),
				P.HasFrontVeranda(), false), FrameX, 0.5 * T, W - 0.5 * T, Options);
		}

		// 廊心 (圖5-1-4.2, 圖5-1-3 sections) on each passage wall between the columns: a brick frame proud of the
		// wall round a field of 方磚 laid on the diagonal (UVs turned 45°).
		if (P.HasWallFrame())
		{
			namespace G = HutongCanon::Gate;
			const double ColD = P.GetColumnDiameter();
			const double Clear = Eave - WallBottom;
			const double Z0 = WallBottom + G::LangxinBottomGap * Clear;
			const double Z1 = Eave - G::LangxinTopGap * Clear;
			const double Border = G::LangxinBorderCm;
			const double Margin = ColR + G::LangxinColumnMargin * ColD;
			// The middle column (中柱 or 前金柱), with the door plane behind it on a recessed gate.
			const double MidY = bRecessed ? DoorY : 0.5 * D;
			const double MidBack = bRecessed ? FMath::Max(T, ColR) + G::LangxinColumnMargin * ColD : Margin;
			const double SpanY[2][2] = { { Margin, MidY - Margin }, { MidY + MidBack, D - Margin } };
			for (const double Face : { T, W - T })
			{
				// Out of the wall into the passage.
				const double In = (Face < 0.5 * W) ? 1.0 : -1.0;
				auto Slab = [&](double Y0, double Y1, double ZA, double ZB, double Proud)
				{
					const double XA = Face - In, XB = Face + In * Proud;
					AppendBox(Mesh, FVector3d(FMath::Min(XA, XB), Y0, ZA), FVector3d(FMath::Max(XA, XB), Y1, ZB));
				};
				for (const auto& Span : SpanY)
				{
					const double Y0 = Span[0], Y1 = Span[1];
					if (Y1 - Y0 < 4.0 * Border || Z1 - Z0 < 4.0 * Border) continue;
					FSlotScope FrameTag(Mesh, MatSlot_Body);
					Slab(Y0, Y1, Z0, Z0 + Border, G::LangxinBorderProudCm);
					Slab(Y0, Y1, Z1 - Border, Z1, G::LangxinBorderProudCm);
					Slab(Y0, Y0 + Border, Z0 + Border, Z1 - Border, G::LangxinBorderProudCm);
					Slab(Y1 - Border, Y1, Z0 + Border, Z1 - Border, G::LangxinBorderProudCm);
					FrameTag.Close();

					const int32 FirstTri = Mesh.MaxTriangleID();
					FSlotScope FieldTag(Mesh, MatSlot_Floor);
					Slab(Y0 + Border, Y1 - Border, Z0 + Border, Z1 - Border, G::LangxinFieldProudCm);
					FieldTag.Close();
					SetDiagonalPaverUVs(Mesh, FirstTri, FVector3d::YAxisVector, FVector3d::ZAxisVector,
						FVector3d(Face, 0.5 * (Y0 + Y1), 0.5 * (Z0 + Z1)));
				}
			}
		}

		// 反八字影壁 (圖5-1-2): a wing from each pier's front outer corner, splayed 45° out to the street. Each is
		// built along +X with its show face at Y = 0 (the cap's AppendGableRoof wants the wall on Y = 0..t), then
		// turned into place: the left one runs toward its corner, the right one away from its own, so both show
		// face toward the approach without a mirror.
		if (P.HasSplayedScreens())
		{
			namespace G = HutongCanon::Gate;
			const double Len = G::WingLengthShare * W;
			const double Thick = FMath::Max(G::WingThicknessShare * W, G::WingMinThicknessCm);
			const double BaseZ = G::WingBaseShare * RoofZ;
			const double FieldTop = G::WingFieldTopShare * RoofZ;
			const double BodyZ = G::WingBodyShare * RoofZ;
			const double CorniceH = G::WingCorniceShare * RoofZ;
			const double CapEave = BodyZ + CorniceH;
			const double Step = G::WingCorbelStepCm;
			const double Post = G::WingPostProudCm;
			const double PierY = P.bHasChitou ? P.GetChitouProjection() : 0.0;
			// Buried past the corner into the pier.
			const double Bury = Thick;

			for (const bool bLeft : { true, false })
			{
				const int32 FirstVert = Mesh.MaxVertexID();
				const double RunX0 = bLeft ? 0.0 : -Bury;
				const double RunX1 = bLeft ? Len + Bury : Len;
				// Local x of a share measured from the gate end.
				auto At = [&](double Share) { return bLeft ? Len * (1.0 - Share) : Len * Share; };
				auto Span = [&](double A, double B, double Y0, double Y1, double Z0, double Z1)
				{
					const double XA = At(A), XB = At(B);
					AppendBox(Mesh, FVector3d(FMath::Min(XA, XB), Y0, Z0), FVector3d(FMath::Max(XA, XB), Y1, Z1));
				};

				{
					FSlotScope BaseTag(Mesh, MatSlot_BaseCourse);
					AppendBox(Mesh, FVector3d(RunX0, -BaseP, 0.0), FVector3d(RunX1, Thick + BaseP, BaseZ));
				}
				FSlotScope BrickTag(Mesh, MatSlot_Body);
				AppendBox(Mesh, FVector3d(RunX0, 0.0, BaseZ), FVector3d(RunX1, Thick, BodyZ));
				// Posts round the field, a band over it, the 盤頭 pier at the outer end: brick proud of the face.
				Span(G::WingPostInner[0], G::WingPostInner[1], -Post, 1.0, BaseZ, BodyZ);
				Span(G::WingPostOuter[0], G::WingPostOuter[1], -Post, 1.0, BaseZ, BodyZ);
				Span(G::WingPostInner[1], G::WingPostOuter[0], -Post, 1.0, FieldTop, BodyZ);
				Span(G::WingPierFrom, 1.0, -Post, 1.0, BaseZ, BodyZ);
				// Cornice: two courses stepping out both faces; the pier's head (盤頭) steps once more.
				for (int32 k = 0; k < 2; ++k)
				{
					const double Out = Step * (k + 1);
					AppendBox(Mesh, FVector3d(RunX0, -Out, BodyZ + 0.5 * k * CorniceH),
						FVector3d(RunX1, Thick + Out, BodyZ + 0.5 * (k + 1) * CorniceH));
				}
				Span(G::WingPierFrom, 1.0, -Post - 3.0 * Step, 1.0, BodyZ + 0.5 * CorniceH, CapEave);
				BrickTag.Close();

				const int32 FieldTri = Mesh.MaxTriangleID();
				{
					FSlotScope FieldTag(Mesh, MatSlot_Floor);
					Span(G::WingPostInner[1], G::WingPostOuter[0], -G::WingFieldProudCm, 1.0, BaseZ, FieldTop);
				}
				SetDiagonalPaverUVs(Mesh, FieldTri, FVector3d::XAxisVector, FVector3d::ZAxisVector,
					FVector3d(0.5 * (At(G::WingPostInner[1]) + At(G::WingPostOuter[0])), 0.0, 0.5 * (BaseZ + FieldTop)));

				// Tiled cap over the cornice, 硬山 at the pier.
				Shell::FRoofParams Cap;
				Cap.FrontOverhang = Cap.RearOverhang = 2.0 * Step + 4.0;
				Cap.Rise = FMath::Max((G::WingRidgeShare - G::WingBodyShare - G::WingCorniceShare) * RoofZ, 5.0);
				Cap.Section = Jiajia::MakeSection(EHutongPurlins::Three, 0.5 * Thick, Cap.FrontOverhang, 0.0);
				Cap.Tile = P.GetRoofTile();
				Cap.TileRowSpacing = P.TileRowSpacing;
				Cap.SlopeSegments = 4;
				Cap.bHasRidgeCourse = P.bHasRidgeCourse;
				Cap.RidgeCourseHeight = 0.6 * P.RidgeCourseHeight;
				Cap.RidgeCourseWidth = 0.6 * P.RidgeCourseWidth;
				Cap.bTileRuns = P.bHasTileRuns;
				// On brick corbels: no 椽頭, a thin eave course, a 博縫 to the cap's scale.
				Cap.RafterSection = 0.0;
				Cap.FasciaDepth = 4.0;
				Cap.FasciaWidth = 6.0;
				Cap.RakeDepth = 12.0;
				Cap.RakeProjection = 3.0;
				const int32 CapVert = Mesh.MaxVertexID();
				Shell::AppendGableRoof(Mesh, RunX1 - RunX0, Thick, CapEave, Cap);
				TransformVerticesFrom(Mesh, CapVert, FTransform(FVector(RunX0, 0.0, 0.0)));

				// Left: its x = Len on the corner at (0, -PierY), turned +45°. Right: x = 0 on (W, -PierY), turned -45°.
				const double Yaw = FMath::DegreesToRadians(bLeft ? G::WingAngleDeg : -G::WingAngleDeg);
				const FQuat Turn(FVector::ZAxisVector, Yaw);
				const FVector Corner(bLeft ? 0.0 : W, -PierY, 0.0);
				const FVector Origin = bLeft ? Corner - Turn.RotateVector(FVector(Len, 0.0, 0.0)) : Corner;
				TransformVerticesFrom(Mesh, FirstVert, FTransform(Turn, Origin));
			}
		}
		Shell::AppendGableRoof(Mesh, W, D, RoofZ, Roof);
	}
}
