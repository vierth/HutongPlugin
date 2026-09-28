#include "Generation/CompoundLayout.h"

namespace HutongGen
{
	namespace
	{
		FHutongCompoundSlot* Add(TArray<FHutongCompoundSlot>& Out, EHutongCompoundPiece Piece,
			double X0, double Y0, double X1, double Y1,
			EHutongBaySide Facing = EHutongBaySide::MinusY, bool bAlongY = false,
			EHutongWallRole WallRole = EHutongWallRole::Perimeter,
			bool bFlipOpenSide = false, double WallGateAt = -1.0,
			double CorridorBenchGapAt = -1.0)
		{
			// The plan degrades by dropping pieces, never by overlapping them.
			if (X1 - X0 < 20.0 || Y1 - Y0 < 20.0) return nullptr;

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
			return &Out.Add_GetRef(S);
		}

	}

	TArray<FHutongCompoundSlot> LayOutCompound(const FCompoundInput& In)
	{
		TArray<FHutongCompoundSlot> Out;

		const double W = In.Width;
		const double D = In.Depth;
		const double G = FMath::Max(In.Gap, 0.0);

		// Backstop: the tool already clamps the drag to the minimum.
		double MinW, MinD;
		In.GetMinimumPlot(MinW, MinD);
		if (W < MinW - 1.0 || D < MinD - 1.0) return Out;

		// The two rows that fix the rest: 倒座房 on the street, 正房 across the back.
		const double FrontD = FMath::Clamp(In.FrontRowDepth, 100.0, D * 0.3);
		const double HallD = FMath::Clamp(In.HallDepth, 150.0, D * 0.42);
		const double GateD = FMath::Clamp(In.GateDepth, 100.0, D * 0.3);

		// 三進: the hall row moves off the north edge for a 後院, closed by the 後罩房.
		const double RearRowD = In.HasRearCourt()
			? FMath::Clamp(In.RearRowDepth, 100.0, D * 0.25) : 0.0;
		// The 後院 grows toward its ordinary depth before the 內院 takes the remaining surplus.
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

		// Street side, corner inward: 門房, 大門, 倒座房.
		const double GateW = FMath::Clamp(In.GateFrontage, 200.0, W * 0.45);
		const double LodgeW = In.bHasGateLodge
			? FMath::Clamp(In.GateLodgeFrontage, 150.0, W * 0.25) : 0.0;

		const double GateX0 = In.bGateAtEastEnd ? LodgeW : (W - LodgeW - GateW);
		const double GateX1 = GateX0 + GateW;

		// Flush with neighbours.
		const double GateY0 = 0.0;
		const double GateY1 = GateD;

		Add(Out, EHutongCompoundPiece::GateHouse, GateX0, GateY0, GateX1, GateY1,
			EHutongBaySide::MinusY);

		// Lodge and 倒座房 face the court; their backs are the lane wall (封護檐).
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

		// The 過道 cuts through the gate-side 耳房, so that flank must seat both room and way.
		const double PassWanted = In.HasRearCourt()
			? FMath::Clamp(In.PassageWidth, T + 80.0, 0.25 * W) : 0.0;

		// 正房三間兩耳: three hall bays in the middle, an 耳房 on each flank.
		double HallW = FMath::Clamp(In.HallFrontage, 400.0, W);
		double EarD = FMath::Clamp(In.EarRoomDepth, 150.0, FMath::Max(HallD - 60.0, 150.0));
		const bool bEars = In.bHasEarRooms && (0.5 * (W - HallW) >= In.MinEarRoomFrontage + PassWanted);
		if (!bEars)
		{
			HallW = W;
			EarD = HallD;
		}
		const double HallX0 = 0.5 * (W - HallW);
		const double HallX1 = HallX0 + HallW;

		// 隔牆 thickness: cross wall, gate-court cheeks, 外院 partition.
		const double CT = FMath::Max(In.CourtyardWallThickness, 5.0);

		// Where a 隔牆 stops against the perimeter.
		const double WallBury = 0.5 * T;

		const double PassW = bEars ? PassWanted : 0.0;
		const bool bPassAtLowX = In.bGateAtEastEnd;

		Add(Out, EHutongCompoundPiece::MainHall, HallX0, HallNorth - HallD, HallX1, HallNorth,
			EHutongBaySide::MinusY);
		if (bEars)
		{
			// One ear carries the way through; both reach the boundary.
			const bool bPassage = PassW > 0.0;
			Add(Out, (bPassage && bPassAtLowX) ? EHutongCompoundPiece::EarPassage : EHutongCompoundPiece::EarRoom,
				0.0, HallNorth - EarD, HallX0, HallNorth, EHutongBaySide::MinusY);
			Add(Out, (bPassage && !bPassAtLowX) ? EHutongCompoundPiece::EarPassage : EHutongCompoundPiece::EarRoom,
				HallX1, HallNorth - EarD, W, HallNorth, EHutongBaySide::MinusY);
		}

		// 後罩房: full width, front on the 後院, back on the boundary.
		if (In.HasRearCourt())
		{
			Add(Out, EHutongCompoundPiece::RearRow, 0.0, D - RearRowD, W, D, EHutongBaySide::MinusY);
		}

		// North-edge face the side walls run up to.
		const double NorthEdgeFace = HallNorth - (bEars ? EarD : HallD);

		// Needed below: the 抄手遊廊 returns stop against the 垂花門's cheeks.
		double InnerY0 = CourtY0;
		double GateRearY = CourtY0;
		double IGX0 = 0.0, IGX1 = 0.0;
		// Cross wall south face; the 外院 partition meets it.
		double CrossWallY = CourtY1;
		if (In.HasInnerGate())
		{
			// Forecourt = the minimum's fixed depth plus a quarter of the surplus, up to the grand
			// gate's depth, most of which stands in the 內院.
			const double IGD = FMath::Clamp(In.InnerGateDepth, 90.0, 520.0);
			const double OuterBase = In.GetOuterCourtDepth();
			const double Needed = OuterBase + FMath::Min(In.GetInnerGateOuterPart(), IGD) + G
				+ FMath::Max(In.MinWingFrontage, In.MinCourtyardDepth) + 2.0 * G;
			// Grows to an ordinary forecourt, then stops.
			const double Surplus = FMath::Max((CourtY1 - CourtY0) - Needed, 0.0);
			const double OuterWant = FMath::Max(In.TypicalOuterCourtDepth - OuterBase, 0.0);
			const double OuterDepth = OuterBase + FMath::Min(Surplus, OuterWant);
			const double IGW = FMath::Clamp(In.InnerGateFrontage, 220.0, W * 0.5);
			IGX0 = 0.5 * (W - IGW);
			IGX1 = IGX0 + IGW;
			const double IGY0 = CourtY0 + OuterDepth;

			Add(Out, EHutongCompoundPiece::InnerGate, IGX0, IGY0, IGX0 + IGW, IGY0 + IGD,
				EHutongBaySide::MinusY);

			// Cross wall either side of the gate, on its door line.
			const double WallAt = (In.InnerGateWallAt >= 0.0) ? FMath::Min(In.InnerGateWallAt, IGD) : 0.5 * IGD;
			const double WallY = IGY0 + WallAt - 0.5 * CT;
			CrossWallY = WallY;
			Add(Out, EHutongCompoundPiece::Wall, WallBury, WallY, IGX0, WallY + CT,
				EHutongBaySide::MinusY, false, EHutongWallRole::Courtyard);
			Add(Out, EHutongCompoundPiece::Wall, IGX1, WallY, W - WallBury, WallY + CT,
				EHutongBaySide::MinusY, false, EHutongWallRole::Courtyard);

			// The wing range starts on the cross wall's north face.
			InnerY0 = WallY + CT;
			// A 垂花門 standing in the court ends where its rear flight does.
			GateRearY = IGY0 + IGD + FMath::Max(In.InnerGateRearReach, 0.0);
		}

		// 大門 → 門道院 → 外院. Computed before the wings: with no cross wall the compartment stands in
		// the court and the wings start north of it (GetPlotFor reserves that depth for 一進).
		const double RowFace = FMath::Max(FrontD, GateD);

		// 座山影壁: the screen backs onto the cross wall when there is one.
		const bool bScreenOnCrossWall = In.HasInnerGate();
		bool bScreen = false;
		double SX0 = 0.0, SX1 = 0.0, SY0 = 0.0, SY1 = 0.0, ScreenBackY = 0.0;
		if (In.bHasScreenWall)
		{
			const double SD = FMath::Max(In.ScreenDepth, 30.0);

			// The screen must cover the doorway it faces; the cheeks stand off its ends.
			const double SLMin = GateW + 2.0 * CT;
			const double SL = FMath::Clamp(In.ScreenLength, SLMin, FMath::Max(1.5 * GateW, SLMin));

			// Its 須彌座 and 懸山 gable overhang its rectangle; the cheeks stand outside that.
			const double SP = FMath::Max(In.ScreenSideProjection, 0.0);
			const double Edge = T + G + FMath::Max(SP, CT);

			// A 座山影壁 shares its wall with the 垂花門, so it must stop short of the gate (they collided
			// on narrow plots).
			double XLo = Edge;
			double XHi = FMath::Max(W - SL - Edge, Edge);
			if (bScreenOnCrossWall)
			{
				// The cheek, outside the screen's end, is what must clear the gate.
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

			// Pushed into the wall by the plinth projection.
			const double PP = FMath::Max(In.ScreenPlinthProjection, 0.0);
			const double Bury = FMath::Clamp(2.0, 0.0, FMath::Max(CT - PP - 1.0, 0.0));

			SY1 = ScreenBackY + PP + Bury;
			SY0 = SY1 - SD;

			// Dropped whole, never in part.
			const double Standback = SY0 - RowFace;
			bScreen = Standback >= FMath::Max(In.ScreenClearance, 120.0)
				&& SX1 + Edge <= W + 0.01 && SX0 - Edge >= -0.01
				&& bClearOfInnerGate;
		}

		// No cross wall: the court starts on the compartment's backing run.
		if (bScreen && !bScreenOnCrossWall)
		{
			InnerY0 = FMath::Max(InnerY0, ScreenBackY + CT);
		}

		// 廂房 down both sides, facing across the inner court.
		const double WingD = FMath::Clamp(In.WingDepth, 150.0, W * 0.3);
		const double WingY0 = In.HasInnerGate() ? InnerY0 : (InnerY0 + G);
		const double WingY1 = CourtY1 - G;

		// Wing range: 廂耳房 at the south, 廂房 north of it, the remainder 小天井 at the north end.
		const double WingRun = WingY1 - WingY0;
		const double EarWant = FMath::Clamp(In.WingEarRoomFrontage,
			In.MinEarRoomFrontage, 0.45 * WingRun);
		double WingEarF = In.bHasWingEarRooms ? EarWant : 0.0;
		double WingF = FMath::Clamp(WingRun - WingEarF, FMath::Min(In.MinWingFrontage, WingRun),
			FMath::Max(In.MaxWingFrontage, In.WingFrontage));
		// Remainder stays at the north end under the walk's leg (Fig 2-9.1: 廂房 a little short of the
		// hall row, 廂耳房 one room).
		const bool bWingEars = In.bHasWingEarRooms && WingEarF >= In.MinEarRoomFrontage;
		if (!bWingEars)
		{
			WingF = FMath::Min(WingF + WingEarF, WingRun);
			WingEarF = 0.0;
		}

		// The ear is at the south; the wing starts where it ends.
		const double WY0 = WingY0 + WingEarF;
		const double WY1 = FMath::Min(WY0 + WingF, WingY1);

		Add(Out, EHutongCompoundPiece::SideHouse, 0.0, WY0, WingD, WY1, EHutongBaySide::PlusX);
		Add(Out, EHutongCompoundPiece::SideHouse, W - WingD, WY0, W, WY1, EHutongBaySide::MinusX);

		// 前廊 + 抄手遊廊: each link runs on the 廂房 veranda line, court side on the wing front, so
		// rooms either side of the wing give up their front strip to it.
		const double LW = FMath::Clamp(In.CorridorDepth, 90.0, 0.22 * W);
		const bool bLinks = In.HasLinkedVerandas() && WingD - LW >= 120.0;
		const double LegX1 = WingD + (bLinks ? FMath::Clamp(In.LinkShift, 0.0, 0.5 * LW) : 0.0);
		const double LegX0 = LegX1 - LW;

		// Shallower than the wing: an 耳房 as deep as its host is not one.
		double WED = FMath::Clamp(EarD, 120.0, FMath::Max(WingD - 60.0, 120.0));
		if (bLinks) WED = FMath::Min(WED, LegX0);
		if (bWingEars)
		{
			// Butted to the 廂房, no gap, as the street row butts the gate.
			Add(Out, EHutongCompoundPiece::EarRoom, 0.0, WingY0, WED, WY0, EHutongBaySide::PlusX);
			Add(Out, EHutongCompoundPiece::EarRoom, W - WED, WingY0, W, WY0, EHutongBaySide::MinusX);
		}

		// 小天井: pocket between the 耳房 front and the 廂房 north gable, behind the north link if any
		// (whose back is then its wall).
		const double WellX = bLinks ? LegX0 - CT : WingD;
		const double WellY0 = WY1;
		const double WellY1 = NorthEdgeFace;
		if (bEars && WellY1 - WellY0 >= In.MinLightWellDepth && WellX >= 150.0)
		{
			// A doorway in each, or the well is unreachable.
			Add(Out, EHutongCompoundPiece::Wall, WellX, WellY0, WellX + CT, WellY1,
				EHutongBaySide::MinusY, /*bAlongY*/ true, EHutongWallRole::Courtyard,
				/*bFlip*/ false, /*WallGateAt*/ 0.5);
			Add(Out, EHutongCompoundPiece::Wall, W - WellX - CT, WellY0, W - WellX, WellY1,
				EHutongBaySide::MinusY, /*bAlongY*/ true, EHutongWallRole::Courtyard,
				/*bFlip*/ false, /*WallGateAt*/ 0.5);
		}

		// Perimeter walls fill only gaps: a run behind a building would be a buried second skin.
		auto SideWall = [&](double X0, double Y0, double Y1)
		{
			Add(Out, EHutongCompoundPiece::Wall, X0, Y0, X0 + T, Y1,
				EHutongBaySide::MinusY, /*bAlongY*/ true);
		};

		// Each side starts at the north face of whatever stands at its south end.
		const double CornerFace = (LodgeW > 0.0) ? FrontD : GateY1;
		const double EastFace = In.bGateAtEastEnd ? CornerFace : FrontD;
		const double WestFace = In.bGateAtEastEnd ? FrontD : CornerFace;

		SideWall(0.0, EastFace, WingY0);
		SideWall(W - T, WestFace, WingY0);
		SideWall(0.0, WY1, NorthEdgeFace);
		SideWall(W - T, WY1, NorthEdgeFace);

		// 三進: the boundary continues past the hall row.
		if (In.HasRearCourt())
		{
			const double LowStart  = (PassW > 0.0 && bPassAtLowX)  ? NorthEdgeFace : HallNorth;
			const double HighStart = (PassW > 0.0 && !bPassAtLowX) ? NorthEdgeFace : HallNorth;
			SideWall(0.0, LowStart, D - RearRowD);
			SideWall(W - T, HighStart, D - RearRowD);
		}

		// Nothing behind the 廂房: the wing reaches the plot edge and is the boundary.

		const double CentreX = 0.5 * W;

		// Outer court left by the compartment; the partition sits in it.
		double YardX0 = T, YardX1 = W - T;

		if (bScreen)
		{
			Add(Out, EHutongCompoundPiece::ScreenWall, SX0, SY0, SX1, SY1);

			// On 二進 this run would be a second skin.
			if (!bScreenOnCrossWall)
			{
				Add(Out, EHutongCompoundPiece::Wall,
					In.bGateAtEastEnd ? T : (SX0 - CT), ScreenBackY,
					In.bGateAtEastEnd ? (SX1 + CT) : (W - T), ScreenBackY + CT,
					EHutongBaySide::MinusY, /*bAlongY*/ false, EHutongWallRole::Courtyard);
			}

			// Both cheeks, each with a doorway, from the street row to the screen's wall.
			const double DoorAt = 0.75;
			Add(Out, EHutongCompoundPiece::Wall, SX0 - CT, RowFace, SX0, ScreenBackY,
				EHutongBaySide::MinusY, /*bAlongY*/ true, EHutongWallRole::Courtyard,
				/*bFlip*/ false, DoorAt);
			Add(Out, EHutongCompoundPiece::Wall, SX1, RowFace, SX1 + CT, ScreenBackY,
				EHutongBaySide::MinusY, /*bAlongY*/ true, EHutongWallRole::Courtyard,
				/*bFlip*/ false, DoorAt);

			if (In.bGateAtEastEnd) YardX0 = SX1 + CT; else YardX1 = SX0 - CT;
		}

		// Service yard behind the 倒座房's last bays, at the 外院's far end.
		if (In.bHasOuterYard && In.HasInnerGate())
		{
			const double YW = FMath::Min(FMath::Max(In.OuterYardWidth, 250.0), 0.3 * W);
			const double PX0 = In.bGateAtEastEnd ? (W - T - YW - CT) : (T + YW);
			// Only if both the yard and the remaining 外院 are worth having.
			const double Court = In.bGateAtEastEnd ? (PX0 - YardX0) : (YardX1 - PX0 - CT);
			if (YW >= 250.0 && Court >= 300.0 && CrossWallY - RowFace >= 150.0)
			{
				Add(Out, EHutongCompoundPiece::Wall, PX0, RowFace, PX0 + CT, CrossWallY,
					EHutongBaySide::MinusY, /*bAlongY*/ true, EHutongWallRole::Courtyard,
					/*bFlip*/ false, /*WallGateAt*/ 0.5);
			}
		}

		// 抄手遊廊: a ring, not two side runs.
		const double CD = FMath::Clamp(In.CorridorDepth, 90.0, 0.22 * W);
		const double RingX0 = WingD;
		const double RingX1 = W - WingD;
		// Runs abut what they run along: south returns land on the cross wall's north face (InnerY0).
		// Not backed off by Gap: that put them inside the wall.
		const double RingY0 = InnerY0;
		// North band on the hall front.
		const double RingY1 = CourtY1 + G;
		const bool bRing = In.HasRingCorridor()
			&& In.HasInnerGate()
			&& (RingX1 - RingX0 - 2.0 * CD) >= In.MinCourtyardWidth
			&& (RingY1 - RingY0 - 2.0 * CD) >= In.MinCourtyardDepth;

		if (bRing)
		{
			// Sides abut at the corners, no overlap.

			// North, stopping at the 正房.
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

			// East and west, before the 廂房.
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

			// Returns along the cross wall, stopping at the 垂花門's cheeks.
			Add(Out, EHutongCompoundPiece::Corridor, RingX0, RingY0, FMath::Min(IGX0, RingX1), RingY0 + CD,
				EHutongBaySide::MinusY, /*bAlongY*/ false, EHutongWallRole::Perimeter,
				/*bFlipOpenSide*/ true);
			Add(Out, EHutongCompoundPiece::Corridor, FMath::Max(IGX1, RingX0), RingY0, RingX1, RingY0 + CD,
				EHutongBaySide::MinusY, /*bAlongY*/ false, EHutongWallRole::Perimeter,
				/*bFlipOpenSide*/ true);
		}

		// 前廊 + 抄手遊廊: four L-shaped links, each abutting the next, so 垂花門, both 廂房 前廊 and the
		// 正房 前廊 form one covered walk. 廂房 and 正房 open their gables across their verandas at a link.
		if (bLinks)
		{
			for (const bool bHigh : { false, true })
			{
				// Mirrored about the axis: X measured in from this side's plot edge.
				auto X = [&](double A) { return bHigh ? W - A : A; };
				// An index, not a pointer: the next run's Add may reallocate Out. INDEX_NONE when nothing was added.
				auto AddRun = [&](double A0, double Y0, double A1, double Y1, bool bAlongY, bool bOpenFlip)
				{
					const FHutongCompoundSlot* S = Add(Out, EHutongCompoundPiece::Corridor, FMath::Min(X(A0), X(A1)), Y0,
						FMath::Max(X(A0), X(A1)), Y1, EHutongBaySide::MinusY, bAlongY, EHutongWallRole::Perimeter, bOpenFlip);
					return S ? int32(S - Out.GetData()) : int32(INDEX_NONE);
				};
				auto At = [&](int32 Index) { return Out.IsValidIndex(Index) ? &Out[Index] : nullptr; };
				// 轉角: both runs at a corner cover its square, cut on the diagonal from outer corner (walls) to
				// inner (court), so roofs mitre: hip outside, valley inside. The X run moves its court-side end
				// corner in X, the Y run in Y; A is from this side's edge, so the X pull reverses on the far side.
				auto CutX = [&](FHutongCompoundSlot* S, bool bSouthEdgeOuter, double Pull)
				{
					// X run's end at this side's plot edge: low X near side, high far side.
					if (!S) return;
					const FVector2D Off(bHigh ? -Pull : Pull, 0.0);
					// Court-side corner: north on a south corner, south on a north one.
					if (bHigh) { if (bSouthEdgeOuter) S->Skew.Corner11 = Off; else S->Skew.Corner10 = Off; }
					else       { if (bSouthEdgeOuter) S->Skew.Corner01 = Off; else S->Skew.Corner00 = Off; }
				};
				// Ends mode keeps the run body as built but its end zone is at most half the run; a short run
				// (leg past the 廂耳房, band along the 耳房) cannot take a full pull there, so shear the whole footprint.
				auto Fit = [](FHutongCompoundSlot* S)
				{
					if (!S || S->Skew.IsZero()) return;
					if (HutongFootprint::IsSkewValid(S->Size, S->Skew)) return;
					S->Skew.Mode = EHutongSkewMode::Whole;
					if (!HutongFootprint::IsSkewValid(S->Size, S->Skew)) S->Skew = FHutongFootprintSkew();
				};
				auto CutY = [&](FHutongCompoundSlot* S, bool bAtLowY, double Pull)
				{
					if (!S) return;
					// Court-side corner of that end: high X on the near side.
					const FVector2D Off(0.0, bAtLowY ? Pull : -Pull);
					if (bAtLowY) { if (bHigh) S->Skew.Corner00 = Off; else S->Skew.Corner10 = Off; }
					else         { if (bHigh) S->Skew.Corner01 = Off; else S->Skew.Corner11 = Off; }
					// Inner-corner post belongs to the X run.
					if (bAtLowY) S->bOmitLowEndPost = true; else S->bOmitHighEndPost = true;
				};
				// Along-Y opens +X unflipped, along-X opens -Y: both toward the court.
				const bool bLegFlip = bHigh;

				// South: from the 垂花門 cheek along the cross wall, round the corner, north past the 廂耳房 to the
				// 廂房 south gable. No 廂耳房: the wing starts on the cross wall, the return stops at its veranda.
				if (In.HasInnerGate())
				{
					const double Cheek = bHigh ? W - IGX1 : IGX0;
					const bool bLeg = WY0 - (InnerY0 + LW) >= 20.0;
					const double ReturnFrom = bLeg ? LegX0 : WingD;
					if (Cheek - ReturnFrom >= LW)
					{
						const int32 ReturnAt = AddRun(ReturnFrom, InnerY0, Cheek, InnerY0 + LW, false, true);
						if (bLeg)
						{
							const int32 LegAt = AddRun(LegX0, InnerY0, LegX1, WY0, true, bLegFlip);
							FHutongCompoundSlot* Return = At(ReturnAt);
							FHutongCompoundSlot* Leg = At(LegAt);
							// Ends on the 廂房 south gable, through its 廊門筒子.
							if (Leg) Leg->bNoBenchAtHighEnd = true;
							CutX(Return, /*bSouthEdgeOuter*/ true, LW);
							CutY(Leg, /*bAtLowY*/ true, LW);
							Fit(Return); Fit(Leg);
						}
					}
				}

				// North: from the 廂房 north gable to the hall row. If the 正房 reaches past the wing front the leg
				// lands on its 前廊; else it turns along the 耳房 front to the 正房 gable.
				const double HallFront = HallNorth - HallD;
				const double HallEnd = bHigh ? W - HallX1 : HallX0;
				const double Band = FMath::Min(LW, NorthEdgeFace - HallFront);
				// The band continues a little past the leg's corner, or into the 正房 gable doorway if nearer;
				// a post there would stand in the doorway.
				constexpr double MinBandBeyond = 30.0;
				// Into the gable doorway no deeper than this; a leg standing further in front of the 正房 lands on
				// its 前廊 instead (a wing's 廊 a quarter of its depth, 圖5-3-3, widens the leg).
				constexpr double MaxIntoHall = 60.0;
				const double BandEnd = FMath::Max(HallEnd, LegX1 + MinBandBeyond);
				if (bEars && Band >= 0.6 * LW && HallEnd > LegX0 + 1.0 && BandEnd - HallEnd <= MaxIntoHall)
				{
					const int32 LegAt = AddRun(LegX0, WY1, LegX1, HallFront + Band, true, bLegFlip);
					const int32 AlongAt = AddRun(LegX0, HallFront, BandEnd, HallFront + Band, false, false);
					FHutongCompoundSlot* Leg = At(LegAt);
					FHutongCompoundSlot* Along = At(AlongAt);
					// From the 廂房 north gable on to the 正房's.
					if (Leg) Leg->bNoBenchAtLowEnd = true;
					if (Along) { if (bHigh) Along->bNoBenchAtLowEnd = true; else Along->bNoBenchAtHighEnd = true; }
					CutX(Along, /*bSouthEdgeOuter*/ false, LW);
					CutY(Leg, /*bAtLowY*/ false, Band);
					Fit(Along); Fit(Leg);
					if (Along && BandEnd > HallEnd + 1.0)
					{
						if (bHigh) Along->bOmitLowEndPost = true; else Along->bOmitHighEndPost = true;
					}
				}
				else if (FHutongCompoundSlot* Leg = At(AddRun(LegX0, WY1, LegX1, HallFront, true, bLegFlip)))
				{
					// Gable to 前廊: the walk turns off at both ends.
					Leg->bNoBenchAtLowEnd = Leg->bNoBenchAtHighEnd = true;
				}
			}
		}

		// 天棚魚缸石榴樹.
		if (In.bHasCourtyardFurnishing && In.bHasPath)
		{
			const double JarS = FMath::Max(In.WaterJarSpan, 30.0);
			const double PW = FMath::Max(In.PathWidth, 40.0);
			const double CourtN = CourtY1 + G;                      // hall front
			const double CourtS = FMath::Max(bRing ? RingY0 + CD : InnerY0 + G, GateRearY + G); // court opening
			// Clear of the hall's 踏跺, far enough down the axis to be seen.
			const double JarY1 = CourtN - 1.6 * JarS;
			const double JarY0 = JarY1 - JarS;

			if (JarY0 - CourtS > JarS && JarS > 0.0)
			{
				// Beside the spine: paving is walked, the jar is looked at.
				const double JarX0 = CentreX + 0.5 * PW + 0.5 * G;
				Add(Out, EHutongCompoundPiece::WaterJar, JarX0, JarY0, JarX0 + JarS, JarY1);

				// A bed each side, level with the jar, inside the ring's clear court (not under a colonnade).
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
			// 十字甬路: spine to the hall steps, an arm to each 廂房.
			const double PW = FMath::Max(In.PathWidth, 40.0);
			Add(Out, EHutongCompoundPiece::Path,
				CentreX - 0.5 * PW, FMath::Max(bRing ? RingY0 + CD : InnerY0 + G, GateRearY + G),
				CentreX + 0.5 * PW, CourtY1 + G,
				EHutongBaySide::MinusY, /*bAlongY*/ true);

			// Arms stop at the spine's edge.
			const double ReachX0 = bRing ? (RingX0 + CD) : WingD;
			const double ReachX1 = bRing ? (RingX1 - CD) : (W - WingD);
			// At the 廂房's middle, where its door is.
			const double ArmY = 0.5 * (WY0 + WY1);
			Add(Out, EHutongCompoundPiece::Path,
				ReachX0, ArmY - 0.5 * PW, CentreX - 0.5 * PW, ArmY + 0.5 * PW);
			Add(Out, EHutongCompoundPiece::Path,
				CentreX + 0.5 * PW, ArmY - 0.5 * PW, ReachX1, ArmY + 0.5 * PW);
		}

		// Wall under a 遊廊 roof stands to the walk's eave uncapped, the roof passing over (抄手遊廊 section
		// at the 垂花門). The rest keeps its cap.
		{
			TArray<FHutongCompoundSlot> Pieces;
			for (FHutongCompoundSlot& Wall : Out)
			{
				if (Wall.Piece != EHutongCompoundPiece::Wall || Wall.WallRole != EHutongWallRole::Courtyard || Wall.bLengthAlongY) continue;
				const double X0 = Wall.Min.X, X1 = X0 + Wall.Size.X, NorthY = Wall.Min.Y + Wall.Size.Y;
				for (const FHutongCompoundSlot& C : Out)
				{
					if (C.Piece != EHutongCompoundPiece::Corridor || C.bLengthAlongY || FMath::Abs(C.Min.Y - NorthY) > 1.0) continue;
					const double A = FMath::Max(X0, C.Min.X), B = FMath::Min(X1, C.Min.X + C.Size.X);
					if (B - A < 20.0) continue;
					// Split into west / under / east of the walk, each kept if buildable.
					FHutongCompoundSlot Under = Wall;
					Under.Min.X = A; Under.Size.X = B - A; Under.bUnderEave = true;
					for (const TPair<double, double>& Rest : { TPair<double, double>(X0, A), TPair<double, double>(B, X1) })
					{
						if (Rest.Value - Rest.Key < 20.0) continue;
						FHutongCompoundSlot Part = Wall;
						Part.Min.X = Rest.Key; Part.Size.X = Rest.Value - Rest.Key;
						Pieces.Add(Part);
					}
					Wall = Under;
					break;
				}
			}
			Out.Append(Pieces);
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

		// HasDecorativeDoorway needs the opening, its surround and 60 cm of masonry inside the run.
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
