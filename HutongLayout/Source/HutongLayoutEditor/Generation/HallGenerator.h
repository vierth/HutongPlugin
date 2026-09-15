#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongBays.h"
#include "Generation/HutongRoofType.h"
#include "Generation/HutongRoofTile.h"
#include "HallGenerator.generated.h"

// 殿: the hall of a small temple, and this type exists because of its roof.
USTRUCT(BlueprintType)
struct FHutongHallParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hall", meta=(DisplayName="Roof Type", ToolTip="Roof form built over the hall."))
	EHutongRoofType RoofType = EHutongRoofType::Xieshan;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hall", meta=(UIMin="300", UIMax="650", ClampMin="150", Units="cm", ToolTip="Height of the eave above the ground, in cm."))
	double EaveHeight = 430.0;

	// The figures the roof is built with, read by generator, massing block, preview and ridge estimate alike.
	double GetEaveHeight() const { return FMath::Max(EaveHeight, 100.0); }
	// A consequence of the 舉架 over the hall's own depth; no field overrides it.
	double GetRoofRise(double OverDepth) const
	{
		const HutongGen::FHutongRoofSection S = HutongGen::Jiajia::MakeSection(
			Purlins, 0.5 * FMath::Max(OverDepth, 1.0), FMath::Max(RoofOverhang, 0.0), RoofApexRoll);
		return FMath::Max(S.Rise(), 20.0);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hall", meta=(UIMin="20", UIMax="80", ClampMin="8", Units="cm", ToolTip="Thickness of the walls, in cm."))
	double WallThickness = 42.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hall", meta=(DisplayName="Floor Height (臺基)", UIMin="20", UIMax="150", ClampMin="0", Units="cm", ToolTip="Height of the platform (臺基) above the ground, in cm."))
	double FloorHeight = 78.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hall", meta=(DisplayName="Platform Front Overhang", UIMin="0", UIMax="200", ClampMin="0", Units="cm", ToolTip="How far the platform projects in front of the facade, in cm."))
	double PlatformOverhang = 95.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hall", meta=(DisplayName="Step Count (踏跺)", UIMin="0", UIMax="8", ClampMin="0", ClampMax="12", ToolTip="Number of steps (踏跺) up to the platform."))
	int32 StepCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hall", meta=(DisplayName="Step Tread", UIMin="20", UIMax="50", ClampMin="10", Units="cm", ToolTip="Depth of each step tread, in cm."))
	double StepTread = 34.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hall", meta=(DisplayName="Base Course Height (下鹼)", UIMin="0", UIMax="200", ClampMin="0", Units="cm", ToolTip="Height of the base course (下鹼) above the floor, in cm; zero derives it."))
	double BaseCourseHeight = 0.0;

	double GetBaseCourseHeight() const
	{
		return BaseCourseHeight > 0.0 ? BaseCourseHeight : FMath::Max(HutongCanon::BaseCourse::TopCm - FMath::Max(FloorHeight, 0.0), 25.0);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hall", meta=(DisplayName="Base Course Projection", UIMin="0", UIMax="15", ClampMin="0", Units="cm", ToolTip="How far the base course stands proud of the wall face, in cm."))
	double BaseCourseProjection = 4.0;

	// --- Bays ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(DisplayName="Min Bay Width", UIMin="200", UIMax="500", ClampMin="100", Units="cm", ToolTip="Smallest bay width when deriving the bay count, in cm."))
	double MinBayWidth = 300.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(DisplayName="Max Bay Width", UIMin="250", UIMax="600", ClampMin="120", Units="cm", ToolTip="Largest bay width when deriving the bay count, in cm."))
	double MaxBayWidth = 420.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(DisplayName="Side Bay / Central Bay (次間/明間)", UIMin="0.6", UIMax="1.0", ClampMin="0.3", ClampMax="1", ToolTip="Width of each side bay (次間) as a fraction of the central bay (明間) width."))
	double SideBayWidthRatio = 0.78;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(DisplayName="Column Diameter", UIMin="25", UIMax="70", ClampMin="10", Units="cm", ToolTip="Diameter of the columns, in cm."))
	double ColumnDiameter = 42.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(DisplayName="Column Taper (收分)", UIMin="0", UIMax="0.02", ClampMin="0", ClampMax="0.05", ToolTip="Column taper (收分) as a fraction of the column height."))
	double ColumnTaperRatio = HutongCanon::Module::ColumnTaperRatio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(DisplayName="Has Brackets (雀替)", ToolTip="Builds sparrow braces (雀替) in the top corners of each bay."))
	bool bHasBrackets = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(DisplayName="Bracket Reach", EditCondition="bHasBrackets", UIMin="20", UIMax="90", ClampMin="5", Units="cm", ToolTip="Length of each sparrow brace (雀替) along the beam, in cm."))
	double BracketReach = 40.0;

	// --- Facade ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facade", meta=(DisplayName="Door Width Fraction", UIMin="0.4", UIMax="0.95", ClampMin="0.2", ClampMax="1", ToolTip="Width of the lattice door (隔扇) opening as a fraction of the central bay (明間) width."))
	double DoorWidthFraction = 0.72;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facade", meta=(DisplayName="Door Head / Column Height", UIMin="0.5", UIMax="0.9", ClampMin="0.2", ClampMax="0.95", ToolTip="Height of the door head as a fraction of the column height."))
	double DoorHeadRatio = 0.74;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facade", meta=(DisplayName="Door Frame Thickness", UIMin="5", UIMax="25", ClampMin="2", Units="cm", ToolTip="Thickness of the door jambs and head, in cm."))
	double DoorFrameThickness = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facade", meta=(DisplayName="Threshold Height (門檻)", UIMin="0", UIMax="40", ClampMin="0", Units="cm", ToolTip="Height of the threshold (門檻) above the floor, in cm."))
	double ThresholdHeight = 22.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facade", meta=(DisplayName="Door Leaves Open", ToolTip="Builds the door leaves folded open instead of shut."))
	bool bDoorLeavesOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facade", meta=(DisplayName="Door Pegs (門簪)", UIMin="0", UIMax="4", ClampMin="0", ClampMax="6", ToolTip="Number of door pegs (門簪) projecting above the door head."))
	int32 DoorPegCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facade", meta=(DisplayName="Sill Height Above Floor (檻牆)", UIMin="60", UIMax="120", ClampMin="20", Units="cm", ToolTip="Height of the sill wall (檻牆) under the windows, in cm."))
	double SillHeightAboveFloor = 90.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facade", meta=(DisplayName="Lattice Bar Thickness", UIMin="2", UIMax="12", ClampMin="1", Units="cm", ToolTip="Thickness of the window lattice bars, in cm."))
	double LatticeBarThickness = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facade", meta=(DisplayName="Lattice Mullions", UIMin="2", UIMax="14", ClampMin="0", ClampMax="24", ToolTip="Number of vertical lattice bars in each window."))
	int32 LatticeMullions = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facade", meta=(DisplayName="Lattice Rails", UIMin="1", UIMax="8", ClampMin="0", ClampMax="16", ToolTip="Number of horizontal lattice bars in each window."))
	int32 LatticeRails = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facade", meta=(DisplayName="Window Paper (窗紙)", ToolTip="Builds a window paper (窗紙) pane behind each window lattice."))
	bool bHasWindowPaper = true;

	// --- Roof ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Tile (瓦作)", ToolTip="Tile type laid on the roof."))
	EHutongRoofTile RoofTile = EHutongRoofTile::Tong;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Row Spacing (壟)", UIMin="12", UIMax="60", ClampMin="6", Units="cm", ToolTip="Spacing of tile rows (壟) across the roof, in cm."))
	double TileRowSpacing = HutongGen::RoofTile::DefaultRowSpacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Overhang (上檐出)", UIMin="60", UIMax="220", ClampMin="10", Units="cm", ToolTip="How far the eave projects past the wall on every side, in cm."))
	double RoofOverhang = 135.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Purlins (檁數)", ToolTip="Number of purlins (檁) across the roof section."))
	EHutongPurlins Purlins = EHutongPurlins::Seven;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Apex Roll (捲棚)", UIMin="0", UIMax="0.6", ClampMin="0", ClampMax="1", ToolTip="Rounding of the roof apex into a rolled ridge (捲棚); 0 keeps the fold."))
	double RoofApexRoll = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Gable Inset (收山)", EditCondition="RoofType == EHutongRoofType::Xieshan", UIMin="60", UIMax="260", ClampMin="5", Units="cm", ToolTip="Distance the gable face (山花) stands in from each end eave, in cm."))
	double ShouInset = HutongCanon::Roof::HallShouInsetCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge Course Height", EditCondition="RoofType == EHutongRoofType::Xieshan && RoofApexRoll <= 0", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="Height of the ridge course, in cm."))
	double RidgeCourseHeight = 26.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge Course Width", EditCondition="RoofType == EHutongRoofType::Xieshan && RoofApexRoll <= 0", UIMin="0", UIMax="40", ClampMin="0", Units="cm", ToolTip="Width of the ridge course, in cm."))
	double RidgeCourseWidth = 18.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Bargeboard Depth (博風板)", EditCondition="RoofType == EHutongRoofType::Xieshan", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="Depth of the bargeboard (博風板) along each gable rake, in cm."))
	double BargeBoardDepth = 34.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Bargeboard Thickness (博風板)", EditCondition="RoofType == EHutongRoofType::Xieshan", UIMin="0", UIMax="18", ClampMin="0", Units="cm", ToolTip="Thickness of the bargeboard (博風板), in cm."))
	double BargeBoardThickness = 7.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Corner Flare Rise (翼角)", UIMin="0", UIMax="140", ClampMin="0", Units="cm", ToolTip="Lift of each eave corner into its upturned corner (翼角), in cm."))
	double RoofFlareRise = 62.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Corner Flare Run", UIMin="0", UIMax="120", ClampMin="0", Units="cm", ToolTip="Outward push of each eave corner into its upturned corner (翼角), in cm."))
	double RoofFlareRun = 46.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Depth (勾頭滴水)", UIMin="0", UIMax="35", ClampMin="0", Units="cm", ToolTip="Depth of the eave cap and drip tile (勾頭滴水) band, in cm."))
	double EaveFasciaDepth = 14.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Eave Segments", UIMin="4", UIMax="20", ClampMin="2", ClampMax="32", ToolTip="Number of segments each eave is divided into when building the roof."))
	int32 RoofEaveSegments = 9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Slope Segments", UIMin="3", UIMax="12", ClampMin="2", ClampMax="16", ToolTip="Number of segments each slope is divided into from eave to ridge."))
	int32 RoofSlopeSegments = 6;

	double GetColumnRadius() const { return FMath::Max(0.5 * ColumnDiameter, 4.0); }

	// The radius the columns are actually laid on, which a narrow plan holds down. The bay
	// boundaries are inset by it, so the plan has to ask for the same number the generator uses.
	double GetColumnRadiusFor(double Frontage, double PlanDepth) const
	{
		return FMath::Min(GetColumnRadius(), 0.2 * FMath::Min(FMath::Max(Frontage, 1.0), FMath::Max(PlanDepth, 1.0)));
	}
	double GetColumnHeight() const { return FMath::Max(EaveHeight - FloorHeight, 1.0); }

	// Bay spacing goes through the shared helper.
	double GetBayBoundary(int32 Index, int32 BayCount, double FacadeLength, double ColumnRadius) const
	{
		return HutongGen::BayBoundary(Index, BayCount, FacadeLength, ColumnRadius,
			SideBayWidthRatio, BayCount / 2);
	}

	// Set by the tool from the drag rect; not user-editable.
	double Width = 900.0;
	double Depth = 620.0;

	// Forces the bay count instead of deriving it. Driven by the bracket keys during placement.
	int32 BayCountOverride = 0;
};

namespace HutongGen
{
	void BuildHall(UE::Geometry::FDynamicMesh3& Mesh, const FHutongHallParams& P);
}
