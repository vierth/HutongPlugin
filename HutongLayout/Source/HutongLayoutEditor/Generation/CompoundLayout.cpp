#include "Generation/CompoundLayout.h"

namespace HutongGen
{
	namespace
	{
		void Add(TArray<FHutongCompoundSlot>& Out, EHutongCompoundPiece Piece,
			double X0, double Y0, double X1, double Y1,
			EHutongBaySide Facing = EHutongBaySide::MinusY, bool bAlongY = false,
			EHutongWallRole WallRole = EHutongWallRole::Perimeter,
			bool bFlipOpenSide = false, double WallGateAt = -1.0,
			double CorridorBenchGapAt = -1.0)
		{
			// The plan degrades by losing pieces, never by overlapping them.
			if (X1 - X0 < 20.0 || Y1 - Y0 < 20.0) return;

			FHutongCompoundSlot S;
			S.Piece = Piece;
			S.Min = FVector2D(X0, Y0);
			S.Size = FVector2D(X1 - X0, Y1 - Y0);
			S.Facing = Facing;
			S.bLengthAlongY = bAlongY;
			S.WallRole = WallRole;
			S.bFlipOpenSide = bFlipOpenSide;
			S.WallGateAt = WallGateAt;
			S.CorridorBenchGapAt = CorridorBenchGapAt;
			Out.Add(S);
		}

	}

	TArray<FHutongCompoundSlot> LayOutCompound(const FCompoundInput& In)
	{
		TArray<FHutongCompoundSlot> Out;

		const double W = In.Width;
		const double D = In.Depth;
		const double G = FMath::Max(In.Gap, 0.0);

		// The tool holds the drag at the minimum, so this is a backstop rather than the guard.
		double MinW, MinD;
		In.GetMinimumPlot(MinW, MinD);
		if (W < MinW - 1.0 || D < MinD - 1.0) return Out;

		// The two rows that fix everything else: 倒座房 hard against the street, 正房 across the back.
		const double FrontD = FMath::Clamp(In.FrontRowDepth, 100.0, D * 0.3);
		const double HallD = FMath::Clamp(In.HallDepth, 150.0, D * 0.42);
		const double GateD = FMath::Clamp(In.GateDepth, 100.0, D * 0.3);

		// 三進: the hall row comes off the north boundary to make room for a 後院 behind it, closed at the back by the 後罩房.
		const double RearRowD = In.HasRearCourt()
			? FMath::Clamp(In.RearRowDepth, 100.0, D * 0.25) : 0.0;
		// The 後院 grows toward what a back court ordinarily is before the 內院 takes the rest of the plot's surplus.
		const double PlotSurplus = FMath::Max(D - MinD, 0.0);
		const double RearWant = FMath::Max(In.TypicalRearCourtDepth - In.RearCourtDepth, 0.0);
		const double RearCourtD = In.HasRearCourt()
			? FMath::Clamp(In.RearCourtDepth + FMath::Min(0.35 * PlotSurplus, RearWant),
				150.0, D * 0.25)
			: 0.0;
		const double HallNorth = D - RearRowD - RearCourtD;

		const double CourtY0 = FMath::Max(FrontD, GateD) + G;
		const double CourtY1 = HallNorth - HallD - G;
		if (CourtY1 - CourtY0 < In.MinCourtyardDepth * 0.6) return Out;

		// The street side, from the corner inward: 門房, 大門, 倒座房.
		const double GateW = FMath::Clamp(In.GateFrontage, 200.0, W * 0.45);
		const double LodgeW = In.bHasGateLodge
			? FMath::Clamp(In.GateLodgeFrontage, 150.0, W * 0.25) : 0.0;

		const double GateX0 = In.bGateAtEastEnd ? LodgeW : (W - LodgeW - GateW);
		const double GateX1 = GateX0 + GateW;

		// Flush with its neighbours, not set back.
		const double GateY0 = 0.0;
		const double GateY1 = GateD;

		Add(Out, EHutongCompoundPiece::GateHouse, GateX0, GateY0, GateX1, GateY1,
			EHutongBaySide::MinusY);

		// The lodge and the 倒座房 face into the courtyard — their backs are the lane wall, which is what 封護檐 exists for.
		if (In.bGateAtEastEnd)
		{
			if (LodgeW > 0.0) Add(Out, EHutongCompoundPiece::GateLodge, 0.0, 0.0, LodgeW, FrontD, EHutongBaySide::PlusY);
			Add(Out, EHutongCompoundPiece::FrontRow, GateX1, 0.0, W, FrontD, EHutongBaySide::PlusY);
		}
		else
		{
			if (LodgeW > 0.0) Add(Out, EHutongCompoundPiece::GateLodge, W - LodgeW, 0.0, W, FrontD, EHutongBaySide::PlusY);
			Add(Out, EHutongCompoundPiece::FrontRow, 0.0, 0.0, GateX0, FrontD, EHutongBaySide::PlusY);
		}

		const double T = FMath::Max(In.WallThickness, 5.0);

		// 過道: the way through to the 後院 is cut through the 耳房 on the gate's own side, so the
		// flank that carries it has to seat a room and the way both.
		const double PassWanted = In.HasRearCourt()
			? FMath::Clamp(In.PassageWidth, T + 80.0, 0.25 * W) : 0.0;

		// 正房三間兩耳: the hall's own three bays in the middle with an 耳房 against each flank.
		double HallW = FMath::Clamp(In.HallFrontage, 400.0, W);
		double EarD = FMath::Clamp(In.EarRoomDepth, 150.0, FMath::Max(HallD - 60.0, 150.0));
		const bool bEars = In.bHasEarRooms && (0.5 * (W - HallW) >= In.MinEarRoomFrontage + PassWanted);
		if (!bEars)
		{
			HallW = W;
			EarD = HallD;
		}
		// The 正房 stands on the plot's axis, and its two 耳房 fill the flanks either side of it.
		const double HallX0 = 0.5 * (W - HallW);
		const double HallX1 = HallX0 + HallW;

		// The 隔牆 the compound divides itself with: the cross wall, the gate court's cheeks and the 外院's partition.
		const double CT = FMath::Max(In.CourtyardWallThickness, 5.0);

		// Where a 隔牆 meeting the perimeter stops.
		const double WallBury = 0.5 * T;

		const double PassW = bEars ? PassWanted : 0.0;
		const bool bPassAtLowX = In.bGateAtEastEnd;

		Add(Out, EHutongCompoundPiece::MainHall, HallX0, HallNorth - HallD, HallX1, HallNorth,
			EHutongBaySide::MinusY);
		if (bEars)
		{
			// One of the two carries the way through; both run out to the boundary.
			const bool bPassage = PassW > 0.0;
			Add(Out, (bPassage && bPassAtLowX) ? EHutongCompoundPiece::EarPassage : EHutongCompoundPiece::EarRoom,
				0.0, HallNorth - EarD, HallX0, HallNorth, EHutongBaySide::MinusY);
			Add(Out, (bPassage && !bPassAtLowX) ? EHutongCompoundPiece::EarPassage : EHutongCompoundPiece::EarRoom,
				HallX1, HallNorth - EarD, W, HallNorth, EHutongBaySide::MinusY);
		}

		// 後罩房 across the back, full width, its front onto the 後院 and its back the plot boundary.
		if (In.HasRearCourt())
		{
			Add(Out, EHutongCompoundPiece::RearRow, 0.0, D - RearRowD, W, D, EHutongBaySide::MinusY);
		}

		// What stands at the plot edge on the north, which the side walls run up to.
		const double NorthEdgeFace = HallNorth - (bEars ? EarD : HallD);

		// The 垂花門's span is wanted below: the 抄手遊廊's returns stop against its cheeks.
		double InnerY0 = CourtY0;
		double IGX0 = 0.0, IGX1 = 0.0;
		// The cross wall's south face, which the 外院's partition runs up to meet.
		double CrossWallY = CourtY1;
		if (In.HasInnerGate())
		{
			// The forecourt takes the fixed depth the minimum was computed from plus a quarter of the surplus.
			const double IGD = FMath::Clamp(In.InnerGateDepth, 90.0, 260.0);
			const double OuterBase = In.GetOuterCourtDepth();
			const double Needed = OuterBase + IGD + G
				+ FMath::Max(In.MinWingFrontage, In.MinCourtyardDepth) + 2.0 * G;
			// It grows to what a forecourt ordinarily is and stops there.
			const double Surplus = FMath::Max((CourtY1 - CourtY0) - Needed, 0.0);
			const double OuterWant = FMath::Max(In.TypicalOuterCourtDepth - OuterBase, 0.0);
			const double OuterDepth = OuterBase + FMath::Min(Surplus, OuterWant);
			const double IGW = FMath::Clamp(In.InnerGateFrontage, 220.0, W * 0.5);
			IGX0 = 0.5 * (W - IGW);
			IGX1 = IGX0 + IGW;
			const double IGY0 = CourtY0 + OuterDepth;

			Add(Out, EHutongCompoundPiece::InnerGate, IGX0, IGY0, IGX0 + IGW, IGY0 + IGD,
				EHutongBaySide::MinusY);

			// The cross wall either side of the gate.
			const double WallY = IGY0 + 0.5 * IGD - 0.5 * CT;
			CrossWallY = WallY;
			Add(Out, EHutongCompoundPiece::Wall, WallBury, WallY, IGX0, WallY + CT,
				EHutongBaySide::MinusY, false, EHutongWallRole::Courtyard);
			Add(Out, EHutongCompoundPiece::Wall, IGX1, WallY, W - WallBury, WallY + CT,
				EHutongBaySide::MinusY, false, EHutongWallRole::Courtyard);

			// The wing range starts on the cross wall's own north face, not a gap north of the gate.
			InnerY0 = WallY + CT;
		}

		// 大門 → 門道院 → 外院. The compartment's arithmetic comes before the wings because on a plan
		// with no cross wall to back onto it stands in the court itself, and the wing range has to
		// start north of it — GetPlotFor already reserves that depth for a 一進.
		const double RowFace = FMath::Max(FrontD, GateD);

		// 座山影壁: the screen's back is the cross wall wherever there is one.
		const bool bScreenOnCrossWall = In.HasInnerGate();
		bool bScreen = false;
		double SX0 = 0.0, SX1 = 0.0, SY0 = 0.0, SY1 = 0.0, ScreenBackY = 0.0;
		if (In.bHasScreenWall)
		{
			const double SD = FMath::Max(In.ScreenDepth, 30.0);

			// The screen has to cover the doorway it faces before it is anything else, and the two cheeks stand off its ends.
			const double SLMin = GateW + 2.0 * CT;
			const double SL = FMath::Clamp(In.ScreenLength, SLMin, FMath::Max(1.5 * GateW, SLMin));

			// Its 須彌座 and 懸山 gable both run past the rectangle it is laid out on, and the cheeks stand outside that again.
			const double SP = FMath::Max(In.ScreenSideProjection, 0.0);
			const double Edge = T + G + FMath::Max(SP, CT);

			// A 座山影壁 shares its wall with the 垂花門 standing in it, so it also has to stop short
			// of the gate: on a narrow plot the two ran into each other at the compartment's far end.
			double XLo = Edge;
			double XHi = FMath::Max(W - SL - Edge, Edge);
			if (bScreenOnCrossWall)
			{
				// The cheek stands outside the screen's own end, so it is what has to clear the gate.
				if (In.bGateAtEastEnd) XHi = IGX0 - G - CT - SL;
				else                   XLo = IGX1 + G + CT;
			}
			SX0 = FMath::Clamp(GateX0 + 0.5 * GateW - 0.5 * SL, XLo, FMath::Max(XHi, XLo));
			SX1 = SX0 + SL;
			const bool bClearOfInnerGate = !bScreenOnCrossWall
				|| (In.bGateAtEastEnd ? (SX1 + CT <= IGX0 - G + 0.01) : (SX0 - CT >= IGX1 + G - 0.01));

			ScreenBackY = bScreenOnCrossWall
				? CrossWallY
				: (RowFace + FMath::Max(In.ScreenClearance, 120.0) + SD);

			// Pushed into that wall by the plinth's projection.
			const double PP = FMath::Max(In.ScreenPlinthProjection, 0.0);
			const double Bury = FMath::Clamp(2.0, 0.0, FMath::Max(CT - PP - 1.0, 0.0));

			SY1 = ScreenBackY + PP + Bury;
			SY0 = SY1 - SD;

			// Dropped whole rather than in part.
			const double Standback = SY0 - RowFace;
			bScreen = Standback >= FMath::Max(In.ScreenClearance, 120.0)
				&& SX1 + Edge <= W + 0.01 && SX0 - Edge >= -0.01
				&& bClearOfInnerGate;
		}

		// With no cross wall the compartment's backing run is what the court starts on.
		if (bScreen && !bScreenOnCrossWall)
		{
			InnerY0 = FMath::Max(InnerY0, ScreenBackY + CT);
		}

		// 廂房 down both sides, facing each other across the inner court.
		const double WingD = FMath::Clamp(In.WingDepth, 150.0, W * 0.3);
		const double WingY0 = In.HasInnerGate() ? InnerY0 : (InnerY0 + G);
		const double WingY1 = CourtY1 - G;

		// They do not run the court's whole length.
		const double WingRun = WingY1 - WingY0;
		const double EarWant = FMath::Clamp(In.WingEarRoomFrontage,
			In.MinEarRoomFrontage, 0.45 * WingRun);
		double WingF = FMath::Min(
			FMath::Clamp(WingRun - EarWant, In.WingFrontage, In.MaxWingFrontage), WingRun);
		double WingEarF = WingRun - WingF;
		const bool bWingEars = In.bHasWingEarRooms && WingEarF >= In.MinEarRoomFrontage;
		if (!bWingEars)
		{
			WingF = WingRun;
			WingEarF = 0.0;
		}

		// The ear sits at the south, so the wing starts where it ends and runs to the hall row.
		const double WY0 = WingY0 + WingEarF;
		const double WY1 = WingY1;

		Add(Out, EHutongCompoundPiece::SideHouse, 0.0, WY0, WingD, WY1, EHutongBaySide::PlusX);
		Add(Out, EHutongCompoundPiece::SideHouse, W - WingD, WY0, W, WY1, EHutongBaySide::MinusX);

		// 前廊 + 抄手遊廊: each link runs on the line of the 廂房's veranda, its court side on the
		// wing's front, so the rooms either side of the wing give up their front strip to it.
		const double LW = FMath::Clamp(In.CorridorDepth, 90.0, 0.22 * W);
		const bool bLinks = In.HasLinkedVerandas() && WingD - LW >= 120.0;
		const double LegX0 = WingD - LW;

		// Shallower than the wing: an 耳房 as deep as the building it defers to is not one.
		double WED = FMath::Clamp(EarD, 120.0, FMath::Max(WingD - 60.0, 120.0));
		if (bLinks) WED = FMath::Min(WED, LegX0);
		if (bWingEars)
		{
			// Butted against the 廂房 with no gap, the way the street row butts the gate.
			Add(Out, EHutongCompoundPiece::EarRoom, 0.0, WingY0, WED, WY0, EHutongBaySide::PlusX);
			Add(Out, EHutongCompoundPiece::EarRoom, W - WED, WingY0, W, WY0, EHutongBaySide::MinusX);
		}

		// 小天井: the pocket at each plot edge between the 耳房's front and the north end of the wing range.
		// Behind the north link when there is one, whose back its wall then is.
		const double WellX = bLinks ? LegX0 - CT : WingD;
		const double WellY0 = WingY1;
		const double WellY1 = NorthEdgeFace;
		if (bEars && WellY1 - WellY0 >= In.MinLightWellDepth && WellX >= 150.0)
		{
			// A doorway in each, or the well is a room nobody can get into.
			Add(Out, EHutongCompoundPiece::Wall, WellX, WellY0, WellX + CT, WellY1,
				EHutongBaySide::MinusY, /*bAlongY*/ true, EHutongWallRole::Courtyard,
				/*bFlip*/ false, /*WallGateAt*/ 0.5);
			Add(Out, EHutongCompoundPiece::Wall, W - WellX - CT, WellY0, W - WellX, WellY1,
				EHutongBaySide::MinusY, /*bAlongY*/ true, EHutongWallRole::Courtyard,
				/*bFlip*/ false, /*WallGateAt*/ 0.5);
		}

		// Perimeter walls fill only the gaps: a run behind a building is a second skin buried in it.
		auto SideWall = [&](double X0, double Y0, double Y1)
		{
			Add(Out, EHutongCompoundPiece::Wall, X0, Y0, X0 + T, Y1,
				EHutongBaySide::MinusY, /*bAlongY*/ true);
		};

		// Each side starts at the north face of whatever stands at that side's south end.
		const double CornerFace = (LodgeW > 0.0) ? FrontD : GateY1;
		const double EastFace = In.bGateAtEastEnd ? CornerFace : FrontD;
		const double WestFace = In.bGateAtEastEnd ? FrontD : CornerFace;

		SideWall(0.0, EastFace, WingY0);
		SideWall(W - T, WestFace, WingY0);
		SideWall(0.0, WingY1, NorthEdgeFace);
		SideWall(W - T, WingY1, NorthEdgeFace);

		// On a 三進 the boundary carries on past the hall row.
		if (In.HasRearCourt())
		{
			const double LowStart  = (PassW > 0.0 && bPassAtLowX)  ? NorthEdgeFace : HallNorth;
			const double HighStart = (PassW > 0.0 && !bPassAtLowX) ? NorthEdgeFace : HallNorth;
			SideWall(0.0, LowStart, D - RearRowD);
			SideWall(W - T, HighStart, D - RearRowD);
		}

		// Nothing behind the 廂房: a wing runs to the plot edge and its own side wall is the boundary.

		const double CentreX = 0.5 * W;

		// What the compartment leaves of the outer court, which the partition below sits in.
		double YardX0 = T, YardX1 = W - T;

		if (bScreen)
		{
			Add(Out, EHutongCompoundPiece::ScreenWall, SX0, SY0, SX1, SY1);

			// On a 二進 plan this run is exactly the second skin the layout refuses everywhere else.
			if (!bScreenOnCrossWall)
			{
				Add(Out, EHutongCompoundPiece::Wall,
					In.bGateAtEastEnd ? T : (SX0 - CT), ScreenBackY,
					In.bGateAtEastEnd ? (SX1 + CT) : (W - T), ScreenBackY + CT,
					EHutongBaySide::MinusY, /*bAlongY*/ false, EHutongWallRole::Courtyard);
			}

			// Both cheeks, each with a doorway, running the full depth from the street row to the wall the screen is on.
			const double DoorAt = 0.75;
			Add(Out, EHutongCompoundPiece::Wall, SX0 - CT, RowFace, SX0, ScreenBackY,
				EHutongBaySide::MinusY, /*bAlongY*/ true, EHutongWallRole::Courtyard,
				/*bFlip*/ false, DoorAt);
			Add(Out, EHutongCompoundPiece::Wall, SX1, RowFace, SX1 + CT, ScreenBackY,
				EHutongBaySide::MinusY, /*bAlongY*/ true, EHutongWallRole::Courtyard,
				/*bFlip*/ false, DoorAt);

			if (In.bGateAtEastEnd) YardX0 = SX1 + CT; else YardX1 = SX0 - CT;
		}

		// The 外院's far end, behind the 倒座房's last bays, is partitioned off as a service yard.
		if (In.bHasOuterYard && In.HasInnerGate())
		{
			const double YW = FMath::Min(FMath::Max(In.OuterYardWidth, 250.0), 0.3 * W);
			const double PX0 = In.bGateAtEastEnd ? (W - T - YW - CT) : (T + YW);
			// Only where a yard worth having and a 外院 worth the name are both left.
			const double Court = In.bGateAtEastEnd ? (PX0 - YardX0) : (YardX1 - PX0 - CT);
			if (YW >= 250.0 && Court >= 300.0 && CrossWallY - RowFace >= 150.0)
			{
				Add(Out, EHutongCompoundPiece::Wall, PX0, RowFace, PX0 + CT, CrossWallY,
					EHutongBaySide::MinusY, /*bAlongY*/ true, EHutongWallRole::Courtyard,
					/*bFlip*/ false, /*WallGateAt*/ 0.5);
			}
		}

		// 抄手遊廊: a ring, not two runs down the sides.
		const double CD = FMath::Clamp(In.CorridorDepth, 90.0, 0.22 * W);
		const double RingX0 = WingD;
		const double RingX1 = W - WingD;
		// Every run abuts what it runs along: the south returns land on the cross wall's north face,
		// which InnerY0 already is. Backing off by a Gap put them inside the wall instead — and with
		// the shipped figures the run covered its whole thickness.
		const double RingY0 = InnerY0;
		// The north band stays on the hall's front.
		const double RingY1 = CourtY1 + G;
		const bool bRing = In.HasRingCorridor()
			&& In.HasInnerGate()
			&& (RingX1 - RingX0 - 2.0 * CD) >= In.MinCourtyardWidth
			&& (RingY1 - RingY0 - 2.0 * CD) >= In.MinCourtyardDepth;

		if (bRing)
		{
			// The four sides abut rather than overlap at the corners.

			// North, stopping at the 正房 rather than crossing it.
			const double LinkX0 = FMath::Min(HallX0, RingX1);
			const double LinkX1 = FMath::Max(HallX1, RingX0);
			if (LinkX0 - RingX0 >= CD)
			{
				Add(Out, EHutongCompoundPiece::Corridor, RingX0, RingY1 - CD, LinkX0, RingY1);
			}
			if (RingX1 - LinkX1 >= CD)
			{
				Add(Out, EHutongCompoundPiece::Corridor, LinkX1, RingY1 - CD, RingX1, RingY1);
			}

			// East and west, in front of the 廂房.
			const double SideY0 = RingY0 + CD, SideY1 = RingY1 - CD;
			const double ArmMidY = 0.5 * (WY0 + WY1);
			const double BenchGap = (SideY1 - SideY0 > 1.0)
				? FMath::Clamp((ArmMidY - SideY0) / (SideY1 - SideY0), 0.0, 1.0) : -1.0;

			Add(Out, EHutongCompoundPiece::Corridor, RingX0, SideY0, RingX0 + CD, SideY1,
				EHutongBaySide::MinusY, /*bAlongY*/ true, EHutongWallRole::Perimeter,
				/*bFlipOpenSide*/ false, /*WallGateAt*/ -1.0, BenchGap);
			Add(Out, EHutongCompoundPiece::Corridor, RingX1 - CD, SideY0, RingX1, SideY1,
				EHutongBaySide::MinusY, /*bAlongY*/ true, EHutongWallRole::Perimeter,
				/*bFlipOpenSide*/ true, /*WallGateAt*/ -1.0, BenchGap);

			// The two returns along the cross wall, stopping against the 垂花門's cheeks.
			Add(Out, EHutongCompoundPiece::Corridor, RingX0, RingY0, FMath::Min(IGX0, RingX1), RingY0 + CD,
				EHutongBaySide::MinusY, /*bAlongY*/ false, EHutongWallRole::Perimeter,
				/*bFlipOpenSide*/ true);
			Add(Out, EHutongCompoundPiece::Corridor, FMath::Max(IGX1, RingX0), RingY0, RingX1, RingY0 + CD,
				EHutongBaySide::MinusY, /*bAlongY*/ false, EHutongWallRole::Perimeter,
				/*bFlipOpenSide*/ true);
		}

		// 前廊 + 抄手遊廊: four L-shaped links, each run abutting the next, so the 垂花門, both 廂房
		// 前廊 and the 正房's 前廊 are one covered walk. The 廂房 and 正房 open their gables across
		// their verandas where a link meets them.
		if (bLinks)
		{
			for (const bool bHigh : { false, true })
			{
				// Mirrored about the axis: X measured in from this side's plot edge.
				auto X = [&](double A) { return bHigh ? W - A : A; };
				auto AddRun = [&](double A0, double Y0, double A1, double Y1, bool bAlongY, bool bOpenFlip)
				{
					Add(Out, EHutongCompoundPiece::Corridor, FMath::Min(X(A0), X(A1)), Y0, FMath::Max(X(A0), X(A1)), Y1,
						EHutongBaySide::MinusY, bAlongY, EHutongWallRole::Perimeter, bOpenFlip);
				};
				// An along-Y run opens to +X unflipped, an along-X run to -Y: both toward the court.
				const bool bLegFlip = bHigh;

				// South: from the 垂花門's cheek along the cross wall, the corner, then north past the
				// 廂耳房 to the 廂房's south gable. With no 廂耳房 the wing starts on the cross wall and
				// the return stops at its veranda instead.
				if (In.HasInnerGate())
				{
					const double Cheek = bHigh ? W - IGX1 : IGX0;
					const bool bLeg = WY0 - (InnerY0 + LW) >= 20.0;
					const double ReturnFrom = bLeg ? LegX0 : WingD;
					if (Cheek - ReturnFrom >= LW)
					{
						AddRun(ReturnFrom, InnerY0, Cheek, InnerY0 + LW, false, true);
						if (bLeg) AddRun(LegX0, InnerY0 + LW, WingD, WY0, true, bLegFlip);
					}
				}

				// North: from the 廂房's north gable up the same line to the hall row. Where the 正房
				// reaches past the wing's front the leg lands on its 前廊 directly; otherwise it turns
				// and runs along the front of the 正房's 耳房 to the 正房's gable.
				const double HallFront = HallNorth - HallD;
				const double HallEnd = bHigh ? W - HallX1 : HallX0;
				AddRun(LegX0, WY1, WingD, HallFront, true, bLegFlip);
				const double Band = FMath::Min(LW, NorthEdgeFace - HallFront);
				if (bEars && Band >= 0.6 * LW && HallEnd - LegX0 >= LW)
				{
					AddRun(LegX0, HallFront, HallEnd, HallFront + Band, false, false);
				}
			}
		}

		// 天棚魚缸石榴樹.
		if (In.bHasCourtyardFurnishing && In.bHasPath)
		{
			const double JarS = FMath::Max(In.WaterJarSpan, 30.0);
			const double PW = FMath::Max(In.PathWidth, 40.0);
			const double CourtN = CourtY1 + G;                      // the hall's front
			const double CourtS = bRing ? RingY0 + CD : InnerY0 + G; // where the court opens
			// Clear of the hall's 踏跺 and far enough down the axis to be looked at.
			const double JarY1 = CourtN - 1.6 * JarS;
			const double JarY0 = JarY1 - JarS;

			if (JarY0 - CourtS > JarS && JarS > 0.0)
			{
				// Beside the spine, not on it: the paving is walked down and the jar is looked at.
				const double JarX0 = CentreX + 0.5 * PW + 0.5 * G;
				Add(Out, EHutongCompoundPiece::WaterJar, JarX0, JarY0, JarX0 + JarS, JarY1);

				// A bed each side of the walk, level with the jar and held inside the ring's clear court so neither lands under a colonnade.
				const double BX = FMath::Max(In.FlowerBedSizeX, 40.0);
				const double BY = FMath::Max(In.FlowerBedSizeY, 40.0);
				const double InnerX0 = bRing ? RingX0 + CD : WingD;
				const double InnerX1 = bRing ? RingX1 - CD : (W - WingD);
				const double BedY0 = JarY1 - BY;
				const double WestX1 = CentreX - 0.5 * PW - G;
				const double EastX0 = JarX0 + JarS + G;
				if (WestX1 - BX - G >= InnerX0)
				{
					Add(Out, EHutongCompoundPiece::FlowerBed, WestX1 - BX, BedY0, WestX1, BedY0 + BY);
				}
				if (EastX0 + BX + G <= InnerX1)
				{
					Add(Out, EHutongCompoundPiece::FlowerBed, EastX0, BedY0, EastX0 + BX, BedY0 + BY);
				}
			}
		}

		if (In.bHasPath)
		{
			// 十字甬路: the spine up the middle of the inner court to the hall's steps, with an arm across to each 廂房.
			const double PW = FMath::Max(In.PathWidth, 40.0);
			Add(Out, EHutongCompoundPiece::Path,
				CentreX - 0.5 * PW, bRing ? RingY0 + CD : InnerY0 + G,
				CentreX + 0.5 * PW, CourtY1 + G,
				EHutongBaySide::MinusY, /*bAlongY*/ true);

			// The arms stop at the spine's edge.
			const double ReachX0 = bRing ? (RingX0 + CD) : WingD;
			const double ReachX1 = bRing ? (RingX1 - CD) : (W - WingD);
			// On the middle of the 廂房 itself, which is where its door is.
			const double ArmY = 0.5 * (WY0 + WY1);
			Add(Out, EHutongCompoundPiece::Path,
				ReachX0, ArmY - 0.5 * PW, CentreX - 0.5 * PW, ArmY + 0.5 * PW);
			Add(Out, EHutongCompoundPiece::Path,
				CentreX + 0.5 * PW, ArmY - 0.5 * PW, ReachX1, ArmY + 0.5 * PW);
		}

		return Out;
	}

	void ApplySlotDoorway(FHutongWallParams& P, const FHutongCompoundSlot& Slot, double RunLength)
	{
		P.bHasGate = false;
		if (Slot.WallGateAt < 0.0)
		{
			P.Doorway = EHutongWallDoorway::None;
			return;
		}

		P.Doorway = EHutongWallDoorway::Rect;
		P.DoorwayPosition = Slot.WallGateAt;

		// HasDecorativeDoorway wants the opening, its surround, and 60 cm of masonry left over, inside the run.
		const double MinClear = 90.0;
		const double Margin = 62.0;
		const double Sur = FMath::Max(P.DoorwaySurroundWidth, 0.0);

		double Width = FMath::Min(FMath::Max(P.DoorwayWidth, MinClear),
			RunLength - 2.0 * Sur - Margin);
		if (Width < MinClear)
		{
			P.DoorwaySurroundWidth = 0.0;
			Width = RunLength - Margin;
		}
		P.DoorwayWidth = FMath::Max(Width, MinClear);
	}
}
