#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "Generation/BaySide.h"
#include "Generation/WallGenerator.h"
#include "Generation/HutongFootprint.h"
#include "CompoundLayout.generated.h"

UENUM()
enum class EHutongCourtWalk : uint8
{
	WingVerandas UMETA(DisplayName = "Front Verandas (前廊) on the Side Houses (廂房)", ToolTip="Verandas on the side houses (廂房), with no covered ring round the court."),

	Corridor UMETA(DisplayName = "Ring Corridor (抄手遊廊) — the covered ring", ToolTip="A ring corridor (抄手遊廊) round the inner court."),

	Linked UMETA(DisplayName = "Verandas Linked by Corridors (前廊 + 抄手遊廊)", ToolTip="Side-house verandas (前廊) linked by corridors (抄手遊廊) into a covered walk round the court."),

	None UMETA(DisplayName = "Neither", ToolTip="No verandas and no corridor."),
};

// Plot size: 小型/中型/大型 (四合院建築及其構造 p.83) at their own widths; Custom takes the drag.
UENUM()
enum class EHutongCompoundSize : uint8
{
	Small UMETA(DisplayName = "Small (小型) — 16 m wide"),

	Medium UMETA(DisplayName = "Medium (中型) — 20 m wide"),

	Large UMETA(DisplayName = "Large (大型) — 25 m wide"),

	Standard UMETA(DisplayName = "Standard (標准) — Fig 2-9.1, 22 m wide", ToolTip="The standard three-court compound, at its reference proportions."),

	Custom UMETA(DisplayName = "Custom — sized by dragging"),
};

UENUM()
enum class EHutongCompoundPlan : uint8
{
	OneCourtyard UMETA(DisplayName = "One Courtyard (一進)", ToolTip="One courtyard, with the gate at the southeast corner."),

	TwoCourtyards UMETA(DisplayName = "Two Courtyards (二進)", ToolTip="An outer court, an inner gate (垂花門) and an inner court."),

	ThreeCourtyards UMETA(DisplayName = "Three Courtyards (三進)", ToolTip="Two courts and a rear court (後院)."),
};

UENUM()
enum class EHutongCompoundPiece : uint8
{
	MainHall UMETA(ToolTip="The main hall (正房) across the back of the inner court."),
	EarRoom UMETA(ToolTip="An ear room (耳房) against the flank of a hall or wing."),
	EarPassage UMETA(ToolTip="An ear room (耳房) with the covered passage (過道) cut through it."),
	SideHouse UMETA(ToolTip="A side house (廂房) down one side of the court."),
	FrontRow UMETA(ToolTip="The front row (倒座房) along the street."),
	RearRow UMETA(ToolTip="The rear row (後罩房) closing the back of the plot."),
	Passage UMETA(ToolTip="The roof over the covered passage (過道)."),
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

// One building's place in the compound: what, where, which way it faces.
struct FHutongCompoundSlot
{
	EHutongCompoundPiece Piece = EHutongCompoundPiece::Wall;

	// Footprint in the plot's local frame.
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;

	// Side of the footprint the facade is on; ignored by pieces with no front.
	EHutongBaySide Facing = EHutongBaySide::MinusY;

	// Line-like pieces: the run lies along Y rather than X.
	bool bLengthAlongY = false;

	// Wall slots only: a run bounds the plot or crosses it.
	EHutongWallRole WallRole = EHutongWallRole::Perimeter;

	// 遊廊 slots only.
	bool bFlipOpenSide = false;

	// 遊廊 slots only: where the 坐凳楣子 breaks so the walk can be stepped onto.
	double CorridorBenchGapAt = -1.0;

	// Wall slots only.
	double WallGateAt = -1.0;

	// Wall slots only: under a 遊廊 roof, so it stops at that eave, uncapped.
	bool bUnderEave = false;

	// 遊廊 run ends cut on the diagonal at a corner, so the two roofs mitre into one 轉角.
	FHutongFootprintSkew Skew;

	// 遊廊 slots only: this end's post belongs to the other run at the corner; not built twice.
	bool bOmitLowEndPost = false;
	bool bOmitHighEndPost = false;

	// 遊廊 slots only: no bench in the end bay where the walk turns through a gable doorway.
	bool bNoBenchAtLowEnd = false;
	bool bNoBenchAtHighEnd = false;

};

// Plot frame: street at Y = 0, back at Y = Depth.
namespace HutongGen
{
	struct FCompoundInput
	{
		double Width = 2400.0;
		double Depth = 3000.0;

		EHutongCompoundPlan Plan = EHutongCompoundPlan::ThreeCourtyards;

		// Match the building presets.
		double HallDepth = 600.0;

		// 正房三間兩耳: the hall does not span the plot.
		double HallFrontage = HutongCanon::Compound::HallFrontageCm;
		double EarRoomDepth = 340.0;
		bool bHasEarRooms = true;

		// Below this an 耳房 is not a room; the hall takes the whole width.
		double MinEarRoomFrontage = HutongCanon::Compound::MinEarRoomFrontageCm;

		// 小天井: shallowest light well worth walling off.
		double MinLightWellDepth = HutongCanon::Compound::MinLightWellDepthCm;

		// 廂房 do not span the court's full length either.
		double WingFrontage = HutongCanon::Compound::WingFrontageCm;
		double MaxWingFrontage = HutongCanon::Compound::MaxWingFrontageCm;

		// Preferred 廂耳房 frontage, not whatever the wing leaves.
		double WingEarRoomFrontage = HutongCanon::Compound::WingEarRoomFrontageCm;
		bool bHasWingEarRooms = true;
		double WingDepth = 450.0;
		double FrontRowDepth = 360.0;

		// 後罩房 and its 後院; 三進 only.
		double RearRowDepth = HutongCanon::Compound::RearRowDepthCm;
		double RearCourtDepth = HutongCanon::Compound::RearCourtDepthCm;

		// 過道 to the 後院, at the plot edge past the 耳房 on the gate side.
		double PassageWidth = HutongCanon::Compound::PassageWidthCm;

		// How far the 過道 roof runs into each side wall.
		double PassageBearing = HutongCanon::Compound::PassageBearingCm;

		double GateFrontage = 360.0;

		// 門房: carries the street row from the gate to the corner.
		double GateLodgeFrontage = HutongCanon::Compound::GateLodgeFrontageCm;
		double GateDepth = 320.0;
		double InnerGateFrontage = 330.0;
		double InnerGateDepth = 140.0;

		// 垂花門 position across the cross wall's depth (negative: middle), and how far its rear platform and steps reach into the court.
		double InnerGateWallAt = -1.0;
		double InnerGateRearReach = 0.0;
		double WallThickness = HutongCanon::Wall::PerimeterThicknessCm;

		// The 垂花門's cross wall is a thinner 隔牆.
		double CourtyardWallThickness = HutongCanon::Wall::CourtyardThicknessCm;
		double CorridorDepth = 175.0;

		// Clear walk width of the ring.
		double CorridorWalkWidth = HutongCanon::Compound::CorridorWalkWidthCm;

		// Link veranda offset past the 廂房 front: posts on its 檐柱 line, walk continues its 前廊 (Fig 2-9.1).
		double LinkShift = 0.0;
		double PathWidth = 130.0;
		double ScreenLength = 300.0;
		double ScreenDepth = 60.0;

		// 影壁 mesh overhang past its footprint at each end.
		double ScreenSideProjection = 20.0;

		// 須彌座 projection: how far the screen body sits inside its footprint.
		double ScreenPlinthProjection = HutongCanon::Screen::PlinthProjectionCm;

		// Least standing room between the 大門 and the 影壁.
		double ScreenClearance = 320.0;

		// Service yard at the 外院's far end: well, store, privy.
		double OuterYardWidth = HutongCanon::Compound::OuterYardWidthCm;
		bool bHasOuterYard = true;

		// Gap between neighbouring buildings so none touch.
		double Gap = HutongCanon::Compound::GapCm;

		// 巽位: the southeast corner.
		bool bGateAtEastEnd = true;

		// The 廂房 depth already includes its 前廊 when present.
		EHutongCourtWalk CourtWalk = EHutongCourtWalk::WingVerandas;
		bool bHasPath = true;

		bool HasRingCorridor() const { return CourtWalk == EHutongCourtWalk::Corridor; }
		bool HasLinkedVerandas() const { return CourtWalk == EHutongCourtWalk::Linked; }

		// 天棚魚缸石榴樹: jar on the axis before the 正房, a bed either side.
		bool bHasCourtyardFurnishing = true;
		double WaterJarSpan = 92.0;
		double FlowerBedSizeX = 200.0;
		double FlowerBedSizeY = 150.0;
		bool bHasScreenWall = true;
		bool bHasGateLodge = true;

		// 內院; the whole minimum derives from it.
		double MinCourtyardWidth = HutongCanon::Compound::MinCourtyardWidthCm;
		double MinCourtyardDepth = HutongCanon::Compound::MinCourtyardDepthCm;

		// 廂房 frontage runs along the plot depth, as long as the court beside it.
		double MinWingFrontage = 560.0;

		// 外院: the forecourt between the gate and the 垂花門.
		double OuterCourtDepth = HutongCanon::Compound::OuterCourtDepthCm;

		// Ordinary court sizes, not minimums.
		double CourtyardWidth = HutongCanon::Compound::CourtyardWidthCm;
		double CourtyardDepth = HutongCanon::Compound::CourtyardDepthCm;

		// The 外院 is a shallow strip between the gate row and the 垂花門, not a second court.
		double TypicalOuterCourtDepth = HutongCanon::Compound::TypicalOuterCourtDepthCm;
		double TypicalRearCourtDepth = HutongCanon::Compound::TypicalRearCourtDepthCm;

		// All plans but the smallest enter through a 垂花門; only the largest has a court behind the hall.
		bool HasInnerGate() const { return Plan != EHutongCompoundPlan::OneCourtyard; }
		bool HasRearCourt() const { return Plan == EHutongCompoundPlan::ThreeCourtyards; }

		// Part of the 垂花門 on the 外院 side: to its door line plus half the wall. The rest lies inside the
		// 內院 depth; counting it whole grew the plot by the gate's projection (21 m deep for 15.5).
		double GetInnerGateOuterPart() const
		{
			if (InnerGateWallAt < 0.0) return InnerGateDepth;
			return FMath::Min(InnerGateWallAt, InnerGateDepth) + 0.5 * FMath::Max(CourtyardWallThickness, 0.0);
		}

		// Forecourt this plan needs; the 影壁 can push it past the nominal one.
		double GetOuterCourtDepth() const
		{
			if (!bHasScreenWall) return OuterCourtDepth;
			// Room to enter, meet the screen, walk round it.
			return FMath::Max(OuterCourtDepth,
				ScreenClearance + ScreenDepth + 2.0 * FMath::Max(Gap, 0.0) + 60.0);
		}

		// Plot for these settings with courts at the given sizes.
		void GetPlotFor(double CourtW, double CourtD, double OuterD, double RearD,
			double& OutWidth, double& OutDepth) const
		{
			const double G = FMath::Max(Gap, 0.0);

			// Across: court plus both 廂房 and their colonnades.
			double Between = CourtW;
			if (HasRingCorridor()) Between += 2.0 * (CorridorDepth + G);
			const double CourtWidth = 2.0 * WingDepth + Between;

			// Street frontage seats the gate, the 門房, and a usable 倒座房.
			const double StreetWidth = GateFrontage + (bHasGateLodge ? GateLodgeFrontage : 0.0) + 500.0;
			OutWidth = FMath::Max(CourtWidth, StreetWidth);

			// Front to back: street row, court, hall, plus forecourt and 垂花門 if any, plus 後院 and 後罩房 on 三進.
			// Wing run: 廂房 plus one 廂耳房 at the south; the rest of the court is 小天井.
			double WingRun = MinWingFrontage;
			if (bHasWingEarRooms)
			{
				WingRun = FMath::Max(WingRun, WingFrontage + MinEarRoomFrontage + G);
			}
			// Court = wall to hall front, gaps included; the wing range adds its own gaps.
			double Court = FMath::Max(WingRun + 2.0 * G, CourtD);
			if (HasRingCorridor()) Court += 2.0 * (CorridorDepth + G);
			if (HasInnerGate())
			{
				Court += FMath::Max(OuterD, GetOuterCourtDepth()) + GetInnerGateOuterPart() + G;
			}
			// No forecourt: the screen stands in the court, which must still be a court beyond it.
			if (Plan == EHutongCompoundPlan::OneCourtyard && bHasScreenWall)
			{
				Court += ScreenClearance + ScreenDepth + G;
			}
			OutDepth = FMath::Max(FrontRowDepth, GateDepth) + G + Court + G + HallDepth;
			if (HasRearCourt()) OutDepth += FMath::Max(RearD, RearCourtDepth) + RearRowDepth;
		}

		// Smallest plot for these settings; the tool clamps the drag to it.
		void GetMinimumPlot(double& OutWidth, double& OutDepth) const
		{
			GetPlotFor(MinCourtyardWidth, MinCourtyardDepth, OuterCourtDepth, RearCourtDepth,
				OutWidth, OutDepth);
		}

		// Ordinary plot for this plan; the drag snaps to it.
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

	// Lays out the plot in build order; empty if too small.
	TArray<FHutongCompoundSlot> LayOutCompound(const FCompoundInput& In);

	// Writes a wall slot's doorway onto that run's params.
	void ApplySlotDoorway(FHutongWallParams& P, const FHutongCompoundSlot& Slot, double RunLength);
}
