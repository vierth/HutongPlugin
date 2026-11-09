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
	WingVerandas UMETA(DisplayName = "Front Verandas (前廊) on the Side Houses (廂房)", ToolTip="Verandas on the side houses (廂房), with no encircling corridor (抄手遊廊)."),

	Corridor UMETA(DisplayName = "Encircling Corridor (抄手遊廊)", ToolTip="A covered walk (抄手遊廊) from each side of the inner gate (垂花門) to the side houses (廂房) and on to the main hall (正房)."),

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

	Standard UMETA(DisplayName = "Standard (標準) — Fig 2-9.1, 22 m wide", ToolTip="The standard three-court compound, at its reference proportions."),

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

struct FHutongCompoundSlot
{
	EHutongCompoundPiece Piece = EHutongCompoundPiece::Wall;

	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Size = FVector2D::ZeroVector;

	EHutongBaySide Facing = EHutongBaySide::MinusY;

	bool bLengthAlongY = false;

	EHutongWallRole WallRole = EHutongWallRole::Perimeter;

	bool bFlipOpenSide = false;

	double CorridorBenchGapAt = -1.0;

	double WallGateAt = -1.0;

	bool bUnderEave = false;

	FHutongFootprintSkew Skew;

	// 遊廊 slots only: this end's post belongs to the other run at the corner.
	bool bOmitLowEndPost = false;
	bool bOmitHighEndPost = false;

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

		double HallDepth = 600.0;

		double HallFrontage = HutongCanon::Compound::HallFrontageCm;
		double EarRoomDepth = 340.0;
		bool bHasEarRooms = true;

		double MinEarRoomFrontage = HutongCanon::Compound::MinEarRoomFrontageCm;

		double MinLightWellDepth = HutongCanon::Compound::MinLightWellDepthCm;

		double WingFrontage = HutongCanon::Compound::WingFrontageCm;
		double MaxWingFrontage = HutongCanon::Compound::MaxWingFrontageCm;

		double WingEarRoomFrontage = HutongCanon::Compound::WingEarRoomFrontageCm;
		bool bHasWingEarRooms = true;
		double WingDepth = 450.0;
		double FrontRowDepth = 360.0;

		double RearRowDepth = HutongCanon::Compound::RearRowDepthCm;
		double RearCourtDepth = HutongCanon::Compound::RearCourtDepthCm;

		double PassageWidth = HutongCanon::Compound::PassageWidthCm;

		double PassageBearing = HutongCanon::Compound::PassageBearingCm;

		double GateFrontage = 360.0;

		double GateLodgeFrontage = HutongCanon::Compound::GateLodgeFrontageCm;
		double GateDepth = 320.0;
		double InnerGateFrontage = 330.0;
		double InnerGateDepth = 140.0;

		// Negative: the middle of the cross wall's depth.
		double InnerGateWallAt = -1.0;
		double InnerGateRearReach = 0.0;
		double WallThickness = HutongCanon::Wall::PerimeterThicknessCm;

		double CourtyardWallThickness = HutongCanon::Wall::CourtyardThicknessCm;
		double CorridorDepth = 175.0;

		double CorridorWalkWidth = HutongCanon::Compound::CorridorWalkWidthCm;

		double LinkShift = 0.0;
		double PathWidth = 130.0;
		double ScreenLength = 300.0;
		double ScreenDepth = 60.0;

		double ScreenSideProjection = 20.0;

		// 須彌座 projection: how far the screen body sits inside its footprint.
		double ScreenPlinthProjection = HutongCanon::Screen::PlinthProjectionCm;

		double ScreenClearance = 320.0;

		double OuterYardWidth = HutongCanon::Compound::OuterYardWidthCm;
		bool bHasOuterYard = true;

		double Gap = HutongCanon::Compound::GapCm;

		bool bGateAtEastEnd = true;

		EHutongCourtWalk CourtWalk = EHutongCourtWalk::WingVerandas;
		bool bHasPath = true;

		bool HasRingCorridor() const { return CourtWalk == EHutongCourtWalk::Corridor; }
		bool HasLinkedVerandas() const { return CourtWalk == EHutongCourtWalk::Linked; }

		bool bHasCourtyardFurnishing = true;
		double WaterJarSpan = 92.0;
		double FlowerBedSizeX = 200.0;
		double FlowerBedSizeY = 150.0;
		bool bHasScreenWall = true;
		bool bHasGateLodge = true;

		// 內院; the whole minimum derives from it.
		double MinCourtyardWidth = HutongCanon::Compound::MinCourtyardWidthCm;
		double MinCourtyardDepth = HutongCanon::Compound::MinCourtyardDepthCm;

		double MinWingFrontage = 560.0;

		double OuterCourtDepth = HutongCanon::Compound::OuterCourtDepthCm;

		double CourtyardWidth = HutongCanon::Compound::CourtyardWidthCm;
		double CourtyardDepth = HutongCanon::Compound::CourtyardDepthCm;

		double TypicalOuterCourtDepth = HutongCanon::Compound::TypicalOuterCourtDepthCm;
		double TypicalRearCourtDepth = HutongCanon::Compound::TypicalRearCourtDepthCm;

		bool HasInnerGate() const { return Plan != EHutongCompoundPlan::OneCourtyard; }
		bool HasRearCourt() const { return Plan == EHutongCompoundPlan::ThreeCourtyards; }

		// The rest of the 垂花門 lies inside the 內院 depth.
		double GetInnerGateOuterPart() const
		{
			if (InnerGateWallAt < 0.0) return InnerGateDepth;
			return FMath::Min(InnerGateWallAt, InnerGateDepth) + 0.5 * FMath::Max(CourtyardWallThickness, 0.0);
		}

		double GetOuterCourtDepth() const
		{
			if (!bHasScreenWall) return OuterCourtDepth;
			// Room to enter, meet the screen, walk round it.
			return FMath::Max(OuterCourtDepth,
				ScreenClearance + ScreenDepth + 2.0 * FMath::Max(Gap, 0.0) + 60.0);
		}

		void GetPlotFor(double CourtW, double CourtD, double OuterD, double RearD,
			double& OutWidth, double& OutDepth) const
		{
			const double G = FMath::Max(Gap, 0.0);

			double Between = CourtW;
			if (HasRingCorridor()) Between += 2.0 * (CorridorDepth + G);
			const double CourtWidth = 2.0 * WingDepth + Between;

			const double StreetWidth = GateFrontage + (bHasGateLodge ? GateLodgeFrontage : 0.0) + 500.0;
			OutWidth = FMath::Max(CourtWidth, StreetWidth);

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

		void GetMinimumPlot(double& OutWidth, double& OutDepth) const
		{
			GetPlotFor(MinCourtyardWidth, MinCourtyardDepth, OuterCourtDepth, RearCourtDepth,
				OutWidth, OutDepth);
		}

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

	TArray<FHutongCompoundSlot> LayOutCompound(const FCompoundInput& In);

	void ApplySlotDoorway(FHutongWallParams& P, const FHutongCompoundSlot& Slot, double RunLength);
}
