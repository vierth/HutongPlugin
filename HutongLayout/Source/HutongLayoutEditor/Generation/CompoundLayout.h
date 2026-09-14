#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "Generation/BaySide.h"
#include "Generation/WallGenerator.h"
#include "CompoundLayout.generated.h"

UENUM()
enum class EHutongCourtWalk : uint8
{
	WingVerandas UMETA(DisplayName = "Front Verandas (前廊) on the Side Houses (廂房)", ToolTip="Verandas on the side houses (廂房), with no covered ring round the court."),

	Corridor UMETA(DisplayName = "Ring Corridor (抄手遊廊) — the covered ring", ToolTip="A covered ring corridor (抄手遊廊) round all four sides of the inner court."),

	None UMETA(DisplayName = "Neither", ToolTip="No verandas and no corridor."),
};

UENUM()
enum class EHutongCompoundPlan : uint8
{
	OneCourtyard UMETA(DisplayName = "One Courtyard (一進)", ToolTip="One courtyard, with the gate at the southeast corner."),

	TwoCourtyards UMETA(DisplayName = "Two Courtyards (二進)", ToolTip="An outer court behind the gate, an inner gate (垂花門), and the inner court beyond it."),

	ThreeCourtyards UMETA(DisplayName = "Three Courtyards (三進)", ToolTip="Two courts plus a rear court (後院) behind the hall, closed by the rear row (後罩房)."),
};

UENUM()
enum class EHutongCompoundPiece : uint8
{
	MainHall UMETA(ToolTip="The main hall (正房) across the back of the inner court."),
	EarRoom UMETA(ToolTip="An ear room (耳房) against the flank of a hall or wing."),
	SideHouse UMETA(ToolTip="A side house (廂房) down one side of the court."),
	FrontRow UMETA(ToolTip="The front row (倒座房) along the street."),
	RearRow UMETA(ToolTip="The rear row (後罩房) closing the back of the plot."),
	Passage UMETA(ToolTip="The roof over the covered passage (過道) through to the rear court (後院)."),
	GateLodge UMETA(ToolTip="The gate lodge (門房) between the gate and the corner."),
	GateHouse UMETA(ToolTip="The main gate (大門)."),
	InnerGate UMETA(ToolTip="The inner gate (垂花門) in the cross wall."),
	ScreenWall UMETA(ToolTip="The screen wall (影壁) facing the gate."),
	Corridor UMETA(ToolTip="A run of covered corridor (遊廊)."),
	Path UMETA(ToolTip="A run of paved path (甬路)."),
	Wall UMETA(ToolTip="A run of wall."),
	FlowerBed UMETA(ToolTip="A flower bed (花池)."),
	WaterJar UMETA(ToolTip="The water jar (魚缸)."),
};

// One building's place in the compound: what to build, where, and which way it faces.
struct FHutongCompoundSlot
{
	EHutongCompoundPiece Piece = EHutongCompoundPiece::Wall;

	// Footprint in the plot's local frame.
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;

	// Which side of its own footprint the facade is on. Ignored by the pieces that have no front.
	EHutongBaySide Facing = EHutongBaySide::MinusY;

	// For the line-like pieces, whether the run lies along the footprint's Y rather than its X.
	bool bLengthAlongY = false;

	// Wall slots only: a run either bounds the plot or crosses it, and that is the whole difference.
	EHutongWallRole WallRole = EHutongWallRole::Perimeter;

	// 遊廊 slots only.
	bool bFlipOpenSide = false;

	// 遊廊 slots only: where the 坐凳楣子 breaks so the walk can be stepped onto.
	double CorridorBenchGapAt = -1.0;

	// Wall slots only.
	double WallGateAt = -1.0;

};

// The plan itself, over a plot with the street at Y = 0 and the back at Y = Depth.
namespace HutongGen
{
	struct FCompoundInput
	{
		double Width = 2400.0;
		double Depth = 3000.0;

		EHutongCompoundPlan Plan = EHutongCompoundPlan::ThreeCourtyards;

		// Depths taken from the building presets, so the two agree.
		double HallDepth = 600.0;

		// 正房三間兩耳: the hall does not run the plot's width.
		double HallFrontage = HutongCanon::Compound::HallFrontageCm;
		double EarRoomDepth = 340.0;
		bool bHasEarRooms = true;

		// Below this an 耳房 stops being a room; the hall takes the whole width instead.
		double MinEarRoomFrontage = HutongCanon::Compound::MinEarRoomFrontageCm;

		// 小天井: the shallowest light well worth walling off.
		double MinLightWellDepth = HutongCanon::Compound::MinLightWellDepthCm;

		// 廂房 likewise do not run the court's full length.
		double WingFrontage = HutongCanon::Compound::WingFrontageCm;
		double MaxWingFrontage = HutongCanon::Compound::MaxWingFrontageCm;

		// What a 廂耳房 wants to be, rather than whatever the wing leaves over.
		double WingEarRoomFrontage = HutongCanon::Compound::WingEarRoomFrontageCm;
		bool bHasWingEarRooms = true;
		double WingDepth = 450.0;
		double FrontRowDepth = 360.0;

		// 後罩房 and the 後院 it closes, on a 三進 plan only.
		double RearRowDepth = HutongCanon::Compound::RearRowDepthCm;
		double RearCourtDepth = HutongCanon::Compound::RearCourtDepthCm;

		// 過道: the way through to the 後院, at the plot edge past the 耳房 on the gate's own side.
		double PassageWidth = HutongCanon::Compound::PassageWidthCm;

		// How far the 過道's roof runs into the wall at each side.
		double PassageBearing = HutongCanon::Compound::PassageBearingCm;

		double GateFrontage = 360.0;

		// 門房: the room carrying the street row past the gate to the corner.
		double GateLodgeFrontage = HutongCanon::Compound::GateLodgeFrontageCm;
		double GateDepth = 320.0;
		double InnerGateFrontage = 330.0;
		double InnerGateDepth = 140.0;
		double WallThickness = HutongCanon::Wall::PerimeterThicknessCm;

		// The cross wall the 垂花門 stands in is a 隔牆 and is thinner.
		double CourtyardWallThickness = HutongCanon::Wall::CourtyardThicknessCm;
		double CorridorDepth = 175.0;

		// The clear walk the ring is built at.
		double CorridorWalkWidth = HutongCanon::Compound::CorridorWalkWidthCm;
		double PathWidth = 130.0;
		double ScreenLength = 300.0;
		double ScreenDepth = 60.0;

		// How far the 影壁's own mesh runs past its footprint at each end.
		double ScreenSideProjection = 20.0;

		// The 須彌座's projection across the run: how far the screen's body sits inside its own footprint.
		double ScreenPlinthProjection = HutongCanon::Screen::PlinthProjectionCm;

		// The least standing room between the 大門 and the 影壁 it faces.
		double ScreenClearance = 320.0;

		// How much of the 外院's far end is partitioned off as a service yard — where the well, the store and the privy go.
		double OuterYardWidth = HutongCanon::Compound::OuterYardWidthCm;
		bool bHasOuterYard = true;

		// Gaps left between a building and its neighbours, so nothing is drawn touching.
		double Gap = HutongCanon::Compound::GapCm;

		// 巽位: the southeast corner.
		bool bGateAtEastEnd = true;

		// The 廂房's own depth already carries its 前廊 when there is one.
		EHutongCourtWalk CourtWalk = EHutongCourtWalk::WingVerandas;
		bool bHasPath = true;

		bool HasRingCorridor() const { return CourtWalk == EHutongCourtWalk::Corridor; }

		// 天棚魚缸石榴樹: the jar on the axis before the 正房 with a bed either side of it.
		bool bHasCourtyardFurnishing = true;
		double WaterJarSpan = 92.0;
		double FlowerBedSizeX = 200.0;
		double FlowerBedSizeY = 150.0;
		bool bHasScreenWall = true;
		bool bHasGateLodge = true;

		// 內院, and the number the whole minimum derives from.
		double MinCourtyardWidth = HutongCanon::Compound::MinCourtyardWidthCm;
		double MinCourtyardDepth = HutongCanon::Compound::MinCourtyardDepthCm;

		// The 廂房's frontage runs along the plot's depth, so it is as long as the courtyard beside it.
		double MinWingFrontage = 560.0;

		// 外院: the forecourt between the gate and the 垂花門.
		double OuterCourtDepth = HutongCanon::Compound::OuterCourtDepthCm;

		// What each court is on an ordinary compound rather than the least it can be.
		double CourtyardWidth = HutongCanon::Compound::CourtyardWidthCm;
		double CourtyardDepth = HutongCanon::Compound::CourtyardDepthCm;

		// The 外院 is a shallow strip, not a second courtyard. It is the space between the gate row and the 垂花門.
		double TypicalOuterCourtDepth = HutongCanon::Compound::TypicalOuterCourtDepthCm;
		double TypicalRearCourtDepth = HutongCanon::Compound::TypicalRearCourtDepthCm;

		// Every plan but the smallest is entered through a 垂花門 in a cross wall; only the largest has a court behind the hall.
		bool HasInnerGate() const { return Plan != EHutongCompoundPlan::OneCourtyard; }
		bool HasRearCourt() const { return Plan == EHutongCompoundPlan::ThreeCourtyards; }

		// The forecourt this plan needs, which the 影壁 can push past the nominal one.
		double GetOuterCourtDepth() const
		{
			if (!bHasScreenWall) return OuterCourtDepth;
			// Room to come in, meet the screen, and walk round it.
			return FMath::Max(OuterCourtDepth,
				ScreenClearance + ScreenDepth + 2.0 * FMath::Max(Gap, 0.0) + 60.0);
		}

		// The plot these settings lay out on with the courts at the sizes given.
		void GetPlotFor(double CourtW, double CourtD, double OuterD, double RearD,
			double& OutWidth, double& OutDepth) const
		{
			const double G = FMath::Max(Gap, 0.0);

			// Across: the courtyard itself, plus the two 廂房 and the colonnades in front of them.
			double Between = CourtW;
			if (HasRingCorridor()) Between += 2.0 * (CorridorDepth + G);
			const double CourtWidth = 2.0 * WingDepth + Between;

			// The street frontage has to seat the gate, the 門房 past it, and a 倒座房 worth having on the other side.
			const double StreetWidth = GateFrontage + (bHasGateLodge ? GateLodgeFrontage : 0.0) + 500.0;
			OutWidth = FMath::Max(CourtWidth, StreetWidth);

			// Front to back: street row, courtyard, hall, plus the forecourt and 垂花門 wherever there is one, and the 後院 and 後罩房 on a 三進.
			double WingRun = MinWingFrontage;
			if (bHasWingEarRooms)
			{
				WingRun = FMath::Max(WingRun, WingFrontage + 2.0 * (MinEarRoomFrontage + G));
			}
			double Court = FMath::Max(WingRun, CourtD) + 2.0 * G;
			if (HasRingCorridor()) Court += 2.0 * (CorridorDepth + G);
			if (HasInnerGate())
			{
				Court += FMath::Max(OuterD, GetOuterCourtDepth()) + InnerGateDepth + G;
			}
			// With no forecourt the screen stands in the courtyard, which still has to be a court beyond it.
			if (Plan == EHutongCompoundPlan::OneCourtyard && bHasScreenWall)
			{
				Court += ScreenClearance + ScreenDepth + G;
			}
			OutDepth = FMath::Max(FrontRowDepth, GateDepth) + G + Court + G + HallDepth;
			// The back court is behind the hall.
			if (HasRearCourt()) OutDepth += FMath::Max(RearD, RearCourtDepth) + RearRowDepth;
		}

		// The smallest plot these settings lay out on, which the tool holds the drag to.
		void GetMinimumPlot(double& OutWidth, double& OutDepth) const
		{
			GetPlotFor(MinCourtyardWidth, MinCourtyardDepth, OuterCourtDepth, RearCourtDepth,
				OutWidth, OutDepth);
		}

		// The plot an ordinary compound of this plan stood on, which is what the drag snaps to.
		void GetSuggestedPlot(double& OutWidth, double& OutDepth) const
		{
			GetPlotFor(CourtyardWidth, CourtyardDepth, TypicalOuterCourtDepth,
				TypicalRearCourtDepth, OutWidth, OutDepth);

			double MinW, MinD;
			GetMinimumPlot(MinW, MinD);
			OutWidth = FMath::Max(OutWidth, MinW);
			OutDepth = FMath::Max(OutDepth, MinD);
		}
	};

	// Lays the plot out, in build order. May be empty if the plot is tiny.
	TArray<FHutongCompoundSlot> LayOutCompound(const FCompoundInput& In);

	// The doorway a wall slot carries, written onto that run's own params.
	void ApplySlotDoorway(FHutongWallParams& P, const FHutongCompoundSlot& Slot, double RunLength);
}
