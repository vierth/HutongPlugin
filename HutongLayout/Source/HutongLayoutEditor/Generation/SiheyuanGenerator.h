#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongBays.h"
#include "Generation/HutongCanon.h"
#include "Generation/HutongProportions.h"
#include "Generation/HutongDoor.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRoofTile.h"
#include "Generation/HutongRearEave.h"
#include "SiheyuanGenerator.generated.h"

// The face of the 檻牆 under the windows (四合院建築及其構造 p.90).
UENUM()
enum class EHutongSillWall : uint8
{
	Plain UMETA(DisplayName = "Plain Ground Brick (干擺)", ToolTip="Ground and rubbed brick laid close."),
	Pool UMETA(DisplayName = "Framed Brick Pool (海棠池子)", ToolTip="Diagonal square bricks framed by a broad brick border and a fine line."),
};

USTRUCT(BlueprintType)
struct FHutongSiheyuanParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Derive From Canonical Proportions", ToolTip="Derives dimensions from the column height and diameter."))
	bool bDeriveProportions = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Column Height in Diameters (柱高/柱徑)", EditCondition="bDeriveProportions", UIMin="8", UIMax="14", ClampMin="4", ClampMax="30", ToolTip="Column height as a multiple of the column diameter."))
	double ColumnHeightInDiameters = HutongCanon::Module::ColumnHeightInDiameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Platform Height in Diameters (臺明高)", EditCondition="bDeriveProportions", UIMin="0", UIMax="5", ClampMin="0", ClampMax="10", ToolTip="Platform height as a multiple of the column diameter."))
	double PlatformHeightInDiameters = HutongCanon::Module::PlatformHeightInDiameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Front Eave Overhang / Column Height (上檐出)", EditCondition="bDeriveProportions", UIMin="0.15", UIMax="0.45", ClampMin="0", ClampMax="1", ToolTip="Front roof overhang as a fraction of the column height."))
	double EaveOverhangRatio = HutongCanon::Module::EaveOverhangRatio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(HutongBasic, DisplayName="Rear Eave (後檐)", ToolTip="What the back of the building faces."))
	EHutongRearEave RearEave = EHutongRearEave::Lane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Rear Eave Overhang / Column Height (封護檐)", EditCondition="bDeriveProportions && RearEave == EHutongRearEave::Lane", UIMin="0", UIMax="0.1", ClampMin="0", ClampMax="1", ToolTip="Rear roof overhang as a fraction of the column height."))
	double RearEaveOverhangRatio = HutongCanon::Module::LaneEaveOverhangRatio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(HutongBasic, DisplayName="Rear Cornice (封護檐)", EditCondition="RearEave == EHutongRearEave::Lane", ToolTip="Brick cornice under the drip course of a sealed rear eave (封護檐)."))
	EHutongSealedCornice RearCornice = EHutongSealedCornice::IceTray;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(HutongBasic, DisplayName="Has Front Veranda (前廊)", ToolTip="Sets the facade back behind a front colonnade to form a veranda (前廊)."))
	bool bHasFrontVeranda = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(HutongBasic, DisplayName="Has Rear Veranda (後廊)", EditCondition="bHasFrontVeranda", ToolTip="Frame with front and rear verandas (前後廊), the rear one enclosed by the back wall."))
	bool bHasRearVeranda = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Eave From Central Bay Width (檐柱高 = 8/10 明間)", EditCondition="bDeriveProportions", ToolTip="Derives the column height from the width of the central bay."))
	bool bDeriveEaveFromBays = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Column Height / Central Bay", EditCondition="bDeriveProportions && bDeriveEaveFromBays", UIMin="0.65", UIMax="1.0", ClampMin="0.4", ClampMax="1.5", ToolTip="Column height as a fraction of the central bay width."))
	double ColumnHeightPerBay = HutongCanon::Module::ColumnHeightPerCentralBay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Minimum Column Height (柱高)", EditCondition="bDeriveProportions && bDeriveEaveFromBays", UIMin="180", UIMax="300", ClampMin="100", Units="cm", ToolTip="Lowest column height when derived from the bay width, in cm."))
	double MinColumnHeight = HutongCanon::Module::MinColumnHeightCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Purlins (檁數)", ToolTip="Number of purlins across the roof section."))
	EHutongPurlins Purlins = EHutongPurlins::Five;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Step Run (步架)", UIMin="90", UIMax="200", ClampMin="40", Units="cm", ToolTip="Horizontal run of one purlin bay (步架), in cm."))
	double StepRun = 125.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Side Bay / Central Bay (次間/明間)", EditCondition="bDeriveProportions", UIMin="0.7", UIMax="1.0", ClampMin="0.3", ClampMax="1", ToolTip="Width of each side bay as a fraction of the central bay."))
	double SideBayWidthRatio = HutongCanon::Module::SideBayWidthRatio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Architrave Depth in Column Diameters (額枋/柱徑)", EditCondition="bDeriveProportions", UIMin="0.7", UIMax="1.6", ClampMin="0.3", ClampMax="3", ToolTip="Depth of the architrave (額枋) as a multiple of the column diameter."))
	double ArchitraveInDiameters = HutongCanon::Openings::ArchitraveInDiameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout", meta=(DisplayName="Suggested Frontage (面闊)", UIMin="0", UIMax="2000", ClampMin="0", Units="cm", ToolTip="Frontage the drag snaps to, in cm; zero leaves the drag free."))
	double SuggestedFrontage = 0.0;

	// The frontage the type's own eave is read at (GetTypeEaveHeight), set with the preset; not the drag
	// snap, so clearing SuggestedFrontage to draw freely keeps the band. Zero falls back to
	// SuggestedFrontage (placements from before the field).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Type Frontage (面闊)", UIMin="0", UIMax="2000", ClampMin="0", Units="cm", ToolTip="Frontage at which the type's eave height is set, in cm; zero uses the suggested frontage."))
	double TypeFrontage = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout", meta=(DisplayName="Suggested Depth (進深)", UIMin="0", UIMax="1200", ClampMin="0", Units="cm", ToolTip="Depth the drag snaps to, in cm; zero derives it."))
	double SuggestedDepth = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout", meta=(DisplayName="Snap To Suggested", EditCondition="SuggestedFrontage > 0 || SuggestedDepth > 0", ToolTip="Snaps the dragged footprint to the suggested frontage and depth."))
	bool bSnapToSuggested = true;

	double GetSuggestedDepth() const
	{
		if (SuggestedDepth > 0.0) return SuggestedDepth;
		return bDeriveProportions ? GetCanonicalDepth() : 0.0;
	}

	// Snaps a dragged extent to the nearest suggestion; FrontageExtra is what a composition adds beside the room.
	double SnapExtent(double Extent, double FrontageExtra = 0.0) const
	{
		if (!bSnapToSuggested) return Extent;

		double Best = Extent;
		double BestDelta = TNumericLimits<double>::Max();
		for (const double Target : { SuggestedFrontage > 0.0 ? SuggestedFrontage + FrontageExtra : 0.0, GetSuggestedDepth() })
		{
			if (Target <= 0.0) continue;
			const double Delta = FMath::Abs(Extent - Target);
			// Proportional with a floor: a tolerance fit for 12 m is invisible on 3 m.
			if (Delta < BestDelta && Delta <= FMath::Max(60.0, 0.1 * Target))
			{
				Best = Target;
				BestDelta = Delta;
			}
		}
		return Best;
	}

	// After a panel edit: resolves contradictions the generator would clamp silently by pushing the
	// field the user did not touch, so the panel shows what is built.
	void ClampAfterEdit(FName Changed)
	{
		if (Changed == TEXT("MinBayWidth"))      MaxBayWidth = FMath::Max(MaxBayWidth, MinBayWidth);
		else if (Changed == TEXT("MaxBayWidth")) MinBayWidth = FMath::Min(MinBayWidth, MaxBayWidth);

		// Openings stack: sill < window top <= door top < eave.
		DoorTopHeight = FMath::Min(DoorTopHeight, EaveHeight - 10.0);
		if (Changed == TEXT("WindowSillHeight")) WindowTopHeight = FMath::Max(WindowTopHeight, WindowSillHeight + 10.0);
		else                                     WindowSillHeight = FMath::Min(WindowSillHeight, WindowTopHeight - 10.0);
		WindowTopHeight = FMath::Min(WindowTopHeight, DoorTopHeight);
		FloorHeight = FMath::Min(FloorHeight, EaveHeight * 0.2);

		// Door leaves end below the lintel, or the transom inverts.
		DoorLeafTopHeight = FMath::Clamp(DoorLeafTopHeight, FloorHeight + 50.0,
			FMath::Max(FloorHeight + 60.0, DoorTopHeight - DoorFrameThickness));

		// A doorway the player cannot walk through is a bug.
		if (bHasFrontDoorCenter)
		{
			if (!bDeriveProportions)
			{
				DoorLeafTopHeight = FMath::Max(DoorLeafTopHeight, HutongGen::Passage::MinHeadZ(FloorHeight, DoorThresholdHeight));
				DoorTopHeight = FMath::Max(DoorTopHeight, DoorLeafTopHeight + DoorFrameThickness);
			}
			EaveHeight = FMath::Max(EaveHeight, GetMinEaveHeight());
		}
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(UIMin="100", UIMax="600", ClampMin="50", Units="cm", ToolTip="Narrowest bay width allowed, in cm."))
	double MinBayWidth = 280.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(UIMin="150", UIMax="800", ClampMin="50", Units="cm", ToolTip="Preferred bay width the facade is divided by, in cm."))
	double MaxBayWidth = 380.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="150", UIMax="600", ClampMin="50", Units="cm", ToolTip="Height of the eave above the ground, in cm."))
	double EaveHeight = 340.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Rise", EditCondition="!bDeriveProportions", UIMin="40", UIMax="400", ClampMin="10", Units="cm", ToolTip="Rise of the ridge above the eave, in cm."))
	double RidgeHeight = 180.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Front Roof Overhang", EditCondition="!bDeriveProportions", UIMin="0", UIMax="200", Units="cm", ToolTip="How far the roof projects past the facade, in cm."))
	double RoofOverhang = 60.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rear Roof Overhang", EditCondition="!bDeriveProportions && RearEave == EHutongRearEave::Lane", UIMin="0", UIMax="60", Units="cm", ToolTip="Roof projection past the back wall when the back faces a lane, in cm."))
	double RearRoofOverhang = 0.0;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Apex Roll", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="Fraction of each half-span rounded into a rolled crown; zero keeps the fold."))
	double RoofApexRoll = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Segments", UIMin="1", UIMax="12", ClampMin="1", ClampMax="16", ToolTip="Number of facets used to round each slope's apex roll."))
	int32 RoofSlopeSegments = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Depth", UIMin="0", UIMax="30", Units="cm", ToolTip="How far the drip course hangs below the eave line, in cm."))
	double EaveFasciaDepth = 8.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Width", UIMin="4", UIMax="40", Units="cm", ToolTip="How far back from the eave edge the fascia runs, in cm."))
	double EaveFasciaWidth = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Section (椽頭)", UIMin="0", UIMax="20", ClampMin="0", Units="cm", ToolTip="Side length of the square rafter ends, in cm; zero removes them."))
	double RafterEndSection = 7.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Courses (壟)", ToolTip="Models each course of tiles down the roof as geometry."))
	bool bHasTileRuns = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Exposed Frame (徹上明造)", ToolTip="Shows the roof frame (梁架) from inside, with no ceiling; close detail levels only."))
	bool bExposedFrame = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Spacing", EditCondition="RafterEndSection > 0", UIMin="10", UIMax="60", ClampMin="5", Units="cm", ToolTip="Distance between rafter ends along the eave, in cm."))
	double RafterEndSpacing = 24.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Flying Rafters (飛椽)", EditCondition="RafterEndSection > 0", ToolTip="Adds square flying rafters (飛椽) over the eave rafters."))
	bool bHasFlyingRafters = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(HutongBasic, DisplayName="Roof Tile (瓦作)", ToolTip="Tile type laid on the roof."))
	EHutongRoofTile RoofTile = EHutongRoofTile::He;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Row Spacing (壟)", UIMin="12", UIMax="60", ClampMin="6", Units="cm", ToolTip="Width of one tile row (壟) along the eave, in cm."))
	double TileRowSpacing = HutongGen::RoofTile::DefaultRowSpacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Gable Rake (博縫排山)", ToolTip="Adds the brick band (博縫) and tile course (排山勾滴) along each gable edge."))
	bool bHasGableRake = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Has Ridge Course (清水脊)", ToolTip="Adds a tile ridge course (清水脊) along the roof apex."))
	bool bHasRidgeCourse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge Course Height", EditCondition="bHasRidgeCourse", UIMin="0", UIMax="80", Units="cm", ToolTip="Height of the ridge course above the roof apex, in cm."))
	double RidgeCourseHeight = 22.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge Course Width", EditCondition="bHasRidgeCourse", UIMin="10", UIMax="60", Units="cm", ToolTip="Thickness of the ridge course across the roof, in cm."))
	double RidgeCourseWidth = 20.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge End Kick (蠍子尾)", EditCondition="bHasRidgeCourse", UIMin="0", UIMax="120", Units="cm", ToolTip="Rise of each ridge end past the gable, in cm; zero squares it."))
	double RidgeEndKick = 55.0;   // HutongCanon::Roof::TailRiseInCourses × the ridge course

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(EditCondition="!bDeriveProportions", UIMin="10", UIMax="80", Units="cm", ToolTip="Thickness of the walls, in cm."))
	double WallThickness = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(EditCondition="!bDeriveProportions", UIMin="10", UIMax="80", Units="cm", ToolTip="Width of the posts (抱框) at the columns, in cm."))
	double PostThickness = 22.0;



	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interior", meta=(DisplayName="Interior Plaster (白灰)", ToolTip="Adds a lime plaster skim to the inside of the side and rear walls."))
	bool bHasInteriorPlaster = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interior", meta=(DisplayName="Plaster Thickness", EditCondition="bHasInteriorPlaster", UIMin="1", UIMax="6", ClampMin="0.5", Units="cm", ToolTip="Thickness of the interior plaster skim, in cm."))
	double InteriorPlasterThickness = 2.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interior", meta=(DisplayName="Partitions (板壁)", ToolTip="Adds a timber partition at each interior bay boundary."))
	bool bHasInteriorPartitions = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interior", meta=(DisplayName="Partition Thickness", EditCondition="bHasInteriorPartitions", UIMin="6", UIMax="20", ClampMin="2", Units="cm", ToolTip="Thickness of each interior partition, in cm."))
	double InteriorPartitionThickness = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interior", meta=(DisplayName="Partition Doorway Width", EditCondition="bHasInteriorPartitions", UIMin="70", UIMax="140", ClampMin="0", Units="cm", ToolTip="Width of the doorway through each interior partition, in cm."))
	double InteriorDoorWidth = 95.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Veranda Depth", EditCondition="bHasFrontVeranda && !bDeriveProportions", UIMin="60", UIMax="250", ClampMin="0", Units="cm", ToolTip="Depth of the front veranda, in cm."))
	double VerandaDepth = 110.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Derive Column Diameter", EditCondition="!bDeriveProportions", ToolTip="Derives the column diameter from the column height."))
	bool bDeriveColumnDiameter = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Column Diameter Ratio", EditCondition="!bDeriveProportions && bDeriveColumnDiameter", UIMin="0.04", UIMax="0.16", ClampMin="0.01", ClampMax="0.5", ToolTip="Column diameter as a fraction of the column height."))
	double ColumnDiameterRatio = HutongCanon::Module::ColumnDiameterRatio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(EditCondition="!bDeriveProportions && !bDeriveColumnDiameter", UIMin="10", UIMax="160", ClampMin="2", Units="cm", ToolTip="Diameter of the columns, in cm."))
	double ColumnDiameter = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Column Taper (收分)", UIMin="0", UIMax="0.02", ClampMin="0", ClampMax="0.05", ToolTip="Column taper as a fraction of the column height."))
	double ColumnTaperRatio = HutongCanon::Module::ColumnTaperRatio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(EditCondition="!bDeriveProportions", UIMin="0", UIMax="120", Units="cm", ToolTip="Height of the platform (臺基) above the ground, in cm."))
	double FloorHeight = 35.0;

	// A court whose verandas and corridors form one walk holds every building on it to one floor; zero = derived.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Floor Height Held At", UIMin="0", UIMax="120", ClampMin="0", Units="cm", ToolTip="Fixed platform (臺基) height, in cm; zero uses the proportions."))
	double FloorHeightHeldAt = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Platform Front Overhang", UIMin="0", UIMax="120", Units="cm", ToolTip="How far the platform projects in front of the facade, in cm."))
	double PlatformOverhang = 35.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(UIMin="0", UIMax="5", ClampMin="0", ClampMax="8", ToolTip="Number of steps (踏跺) in front of the door bay."))
	int32 StepCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(UIMin="15", UIMax="80", ClampMin="5", Units="cm", ToolTip="Depth of each step tread, in cm."))
	double StepTread = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Veranda End Doorways (廊門筒子)", EditCondition="bHasFrontVeranda", ToolTip="Opens a doorway in each gable wall at the front veranda (前廊), for a corridor (遊廊)."))
	bool bHasVerandaEndDoorways = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Has Gable Pier (墀頭)", ToolTip="Adds a corbelled brick pier (墀頭) at each gable's front corner."))
	bool bHasChitou = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Chitou Projection", EditCondition="bHasChitou", UIMin="0", UIMax="60", Units="cm", ToolTip="How far each chitou projects forward of the facade, in cm."))
	double ChitouProjection = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Chitou Corbel Courses", EditCondition="bHasChitou", UIMin="0", UIMax="6", ClampMin="0", ClampMax="10", ToolTip="Number of corbel courses stepping out at the top of each chitou."))
	int32 ChitouCorbelSteps = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Base Course Height", UIMin="0", UIMax="200", Units="cm", ToolTip="Height of the base course (下鹼) above the platform, in cm; zero derives it."))
	double BaseCourseHeight = 0.0;

	double GetBaseCourseHeight() const
	{
		return BaseCourseHeight > 0.0 ? BaseCourseHeight : FMath::Max(HutongCanon::BaseCourse::TopCm - GetFloorHeight(), 25.0);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Base Course Projection", UIMin="0", UIMax="15", Units="cm", ToolTip="How far the base course stands proud of the wall face, in cm."))
	double BaseCourseProjection = 3.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(EditCondition="!bDeriveProportions", UIMin="100", UIMax="400", Units="cm", ToolTip="Height of the underside of the door lintel above the ground, in cm."))
	double DoorTopHeight = 270.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(EditCondition="!bDeriveProportions", UIMin="0", UIMax="250", Units="cm", ToolTip="Height of the knee wall top under the windows, in cm."))
	double WindowSillHeight = 105.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(EditCondition="!bDeriveProportions", UIMin="50", UIMax="400", Units="cm", ToolTip="Height of the window head above the ground, in cm."))
	double WindowTopHeight = 240.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Has Window Lattice (支摘窗)", ToolTip="Adds a lattice of mullions and rails across each window opening."))
	bool bHasWindowLattice = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Window Mullions", EditCondition="bHasWindowLattice", UIMin="0", UIMax="6", ClampMin="0", ClampMax="12", ToolTip="Number of vertical lattice bars in each upper sash (支窗) and door lattice (槅心)."))
	int32 WindowMullions = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Window Rails", EditCondition="bHasWindowLattice", UIMin="0", UIMax="6", ClampMin="0", ClampMax="12", ToolTip="Number of horizontal lattice bars in each upper sash (支窗) and door lattice (槅心)."))
	int32 WindowRails = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Lattice Bar Thickness", EditCondition="bHasWindowLattice", UIMin="2", UIMax="12", ClampMin="1", Units="cm", ToolTip="Thickness of each lattice bar, in cm."))
	double WindowLatticeThickness = 4.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="High Windows (高窗) in the Back Wall", ToolTip="Adds a row of small high windows (高窗) in the back wall, one per bay."))
	bool bHasRearHighWindows = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="High Window Width (高窗)", EditCondition="bHasRearHighWindows", UIMin="40", UIMax="120", ClampMin="20", Units="cm", ToolTip="Width of each high window, in cm."))
	double RearWindowWidth = HutongCanon::Openings::RearWindowWidthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="High Window Height (高窗)", EditCondition="bHasRearHighWindows", UIMin="25", UIMax="80", ClampMin="10", Units="cm", ToolTip="Height of each high window, in cm."))
	double RearWindowHeight = HutongCanon::Openings::RearWindowHeightCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="High Window Head Below Eave (高窗)", EditCondition="bHasRearHighWindows", UIMin="20", UIMax="120", ClampMin="5", Units="cm", ToolTip="Distance from the eave down to the head of each high window, in cm."))
	double RearWindowHeadDrop = HutongCanon::Openings::RearWindowHeadBelowEaveCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(HutongBasic, DisplayName="Has Front Door", ToolTip="Opens a door bay in the facade."))
	bool bHasFrontDoorCenter = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Door Bay", EditCondition="bHasFrontDoorCenter", UIMin="-1", UIMax="8", ClampMin="-1", ClampMax="31", ToolTip="Which bay holds the door, from the origin end; -1 centres it."))
	int32 DoorBayIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Has Door Frame (抱框)", EditCondition="bHasFrontDoorCenter", ToolTip="Fills the door bay with four lattice doors (隔扇) on a sill (下檻) under a transom (橫陂)."))
	bool bHasDoorFrame = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Door Leaf Top Height", EditCondition="bHasDoorFrame && !bDeriveProportions", UIMin="180", UIMax="320", ClampMin="50", Units="cm", ToolTip="Height of the top of the door leaves above the ground, in cm."))
	double DoorLeafTopHeight = 245.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(HutongBasic, DisplayName="Door Leaves Open", EditCondition="bHasDoorFrame", ToolTip="Swings the middle two lattice doors (隔扇) into the room and the screen door (風門) out onto the courtyard."))
	bool bDoorLeavesOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Door Frame Thickness", EditCondition="bHasDoorFrame && !bDeriveProportions", UIMin="4", UIMax="20", ClampMin="2", Units="cm", ToolTip="Height of the rail over the door leaves, in cm."))
	double DoorFrameThickness = 9.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Threshold Height", EditCondition="bHasDoorFrame && !bDeriveProportions", UIMin="0", UIMax="30", ClampMin="0", Units="cm", ToolTip="Height of the bottom sill (下檻) under the lattice doors, in cm."))
	double DoorThresholdHeight = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Window Paper (窗紙)", ToolTip="Adds a paper pane (窗紙) behind each window lattice."))
	bool bHasWindowPaper = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Transom Panes (橫陂)", EditCondition="bHasFrontVeranda", UIMin="1", UIMax="5", ClampMin="1", ClampMax="8", ToolTip="Number of fixed transom panes (橫陂) in each bay over the lattice doors."))
	int32 TransomPanes = HutongCanon::Joinery::TransomPanes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Curtain Frame and Screen Door (簾架, 風門)", EditCondition="bHasDoorFrame", ToolTip="Sets a curtain frame (簾架) with a screen door (風門) before the door bay's lattice doors (隔扇)."))
	bool bHasCurtainFrame = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(HutongBasic, DisplayName="Sill Wall Face (檻牆)", ToolTip="Face of the low brick wall (檻牆) under the windows."))
	EHutongSillWall SillWallFinish = EHutongSillWall::Plain;

	// Shared by generator and tool preview so the highlighted bay is the built one.
	int32 GetDoorBayIndex(int32 BayCount) const
	{
		if (BayCount <= 0) return 0;
		return (DoorBayIndex < 0) ? BayCount / 2 : FMath::Clamp(DoorBayIndex, 0, BayCount - 1);
	}

	// Lowest eave carrying a walkable doorway; zero with no front door.
	double GetMinEaveHeight() const
	{
		if (!bHasFrontDoorCenter)
		{
			return 0.0;
		}
		namespace J = HutongCanon::Joinery;
		if (!bDeriveProportions)
		{
			// No frame: the bay is open to the lintel, so only clearance counts.
			const double Need = bHasDoorFrame
				? FMath::Clamp(DoorThresholdHeight, 0.0, 40.0) + HutongGen::Passage::MinClearHeight + FMath::Max(DoorFrameThickness, 2.0)
				: HutongGen::Passage::MinClearHeight;
			// BuildSiheyuan holds the lintel 10 cm below the eave; 20 keeps that clamp off the door.
			return (FloorHeightHeldAt > 0.0 ? FloorHeightHeldAt : FloorHeight) + Need + 20.0;
		}
		// The 下檻 and the rail are in 柱徑, so they come off the column with the headroom (表十三).
		const double Need = HutongGen::Passage::MinClearHeight;
		const double Rail = GetHeadRailInD();
		const double Sill = bHasDoorFrame ? J::LowerSillInD : 0.0;
		// A held floor does not scale with the eave: the headroom is bought above it.
		if (FloorHeightHeldAt > 0.0)
		{
			return HutongGen::Proportions::MinEaveForHeadroomOnFloor(Need, FloorHeightHeldAt,
				ColumnHeightInDiameters, ArchitraveInDiameters, Rail, Sill);
		}
		return HutongGen::Proportions::MinEaveForHeadroom(Need,
			ColumnHeightInDiameters, PlatformHeightInDiameters, ArchitraveInDiameters, Rail, Sill);
	}

	// 表十三: the rail at the 檐枋's line is the 中檻 where 橫陂 stand over it (behind a 前廊), else the 上檻.
	double GetHeadRailInD() const
	{
		return bHasFrontVeranda ? HutongCanon::Joinery::MiddleSillInD : HutongCanon::Joinery::UpperSillInD;
	}
	double GetHeadRailHeight() const
	{
		return bDeriveProportions ? GetHeadRailInD() * GetColumnDiameter() : FMath::Max(DoorFrameThickness, 2.0);
	}
	// 下檻 under the 隔扇, ⅘ 柱徑.
	double GetLowerSillHeight() const
	{
		return bDeriveProportions ? HutongCanon::Joinery::LowerSillInD * GetColumnDiameter() : FMath::Max(DoorThresholdHeight, 0.0);
	}
	// One 隔扇 of the 明間's four (清式營造則例 表十四 sizes its parts from this width).
	double GetGeshanWidth() const
	{
		return FMath::Max(GetCentralBayWidth() - GetPostThickness(), 40.0) / HutongCanon::Joinery::GeshanPerBay;
	}
	// 清式營造則例 表十二: 山牆 2.4 柱徑 thick, the 檻牆 on the facade and the 檐牆 behind 1½.
	double GetGableWallThickness() const
	{
		return bDeriveProportions ? HutongCanon::Masonry::GableWallInD * GetColumnDiameter() : WallThickness;
	}
	double GetFacadeWallThickness() const
	{
		return bDeriveProportions ? HutongCanon::Masonry::SillWallInD * GetColumnDiameter() : WallThickness;
	}
	double GetRearWallThickness() const
	{
		return bDeriveProportions ? HutongCanon::Masonry::RearWallInD * GetColumnDiameter() : WallThickness;
	}
	// 抱框 at the columns, ⅔ 柱徑 wide.
	double GetPostThickness() const
	{
		return bDeriveProportions ? HutongCanon::Joinery::PostInD * GetColumnDiameter() : PostThickness;
	}

	int32 GetBayCount() const
	{
		return (BayCountOverride > 0)
			? BayCountOverride
			: HutongGen::ComputeBayCount(Width, MinBayWidth, MaxBayWidth);
	}

	// 明間面闊, the module for 檐柱高; same arithmetic as BayBoundary.
	double GetCentralBayWidth() const
	{
		return HutongGen::CentralBayWidth(Width, GetBayCount(),
			bDeriveProportions ? SideBayWidthRatio : 1.0);
	}

	// 檐柱高 = 8/10 明間面闊, via the same platform-and-column solve as GetFloorHeight, held within
	// DerivedEaveBandShare of the type's eave (GetTypeEaveHeight): the drawn width sets the bays, the
	// type sets the height, so near-equal footprints stand near-equal.
	double GetEaveHeightFromBays() const
	{
		const double Drawn = EaveFromCentralBay(GetCentralBayWidth());
		const double Type = GetTypeEaveHeight();
		if (Type <= 0.0) return Drawn;
		const double Band = HutongCanon::Module::DerivedEaveBandShare;
		return FMath::Clamp(Drawn, Type * (1.0 - Band), Type * (1.0 + Band));
	}

	// The eave the rule gives at the type's frontage; zero without one.
	double GetTypeEaveHeight() const
	{
		const double Frontage = TypeFrontage > 0.0 ? TypeFrontage : SuggestedFrontage;
		if (Frontage <= 0.0) return 0.0;
		const int32 N = HutongGen::ComputeBayCount(Frontage, MinBayWidth, MaxBayWidth);
		return EaveFromCentralBay(HutongGen::CentralBayWidth(Frontage, N,
			bDeriveProportions ? SideBayWidthRatio : 1.0));
	}

	double EaveFromCentralBay(double CentralBay) const
	{
		const double Col = HutongGen::Proportions::ColumnFromCentralBay(CentralBay, ColumnHeightPerBay, MinColumnHeight);
		return HutongGen::Proportions::EaveFromColumn(Col, ColumnHeightInDiameters, PlatformHeightInDiameters);
	}

	double GetEaveHeight() const
	{
		const double Asked = (bDeriveProportions && bDeriveEaveFromBays)
			? GetEaveHeightFromBays() : EaveHeight;
		return FMath::Max(Asked, GetMinEaveHeight());
	}

	// 臺明高 = k 柱徑 and 柱高 = R 柱徑 share the eave, so Floor = k·Eave/(R+k).
	double GetFloorHeight() const
	{
		if (FloorHeightHeldAt > 0.0) return FloorHeightHeldAt;
		if (!bDeriveProportions)
		{
			return FloorHeight;
		}
		return HutongGen::Proportions::FloorFromEave(GetEaveHeight(),
			ColumnHeightInDiameters, PlatformHeightInDiameters);
	}

	// 柱高: platform top to eave.
	double GetColumnHeight() const
	{
		return FMath::Max(GetEaveHeight() - GetFloorHeight(), 1.0);
	}

	// Column radius as the generator floors it.
	double GetColumnRadius() const { return FMath::Max(0.5 * GetColumnDiameter(), 1.0); }

	// Pier projection: at least covers the corner column.
	double GetChitouProjection() const
	{
		return HutongGen::Proportions::ChitouProjection(ChitouProjection, GetColumnRadius());
	}

	double GetColumnDiameter() const
	{
		if (bDeriveProportions)
		{
			return HutongGen::Proportions::ColumnDiameter(GetColumnHeight(), ColumnHeightInDiameters);
		}
		return bDeriveColumnDiameter
			? FMath::Max(EaveHeight * ColumnDiameterRatio, 2.0)
			: ColumnDiameter;
	}

	double GetRoofOverhang() const
	{
		return bDeriveProportions
			? HutongGen::Proportions::EaveOverhang(GetColumnHeight(), EaveOverhangRatio)
			: FMath::Max(RoofOverhang, 0.0);
	}

	// A back onto the courtyard is a front.
	double GetRearRoofOverhang() const
	{
		if (RearEave == EHutongRearEave::Courtyard)
		{
			return GetRoofOverhang();
		}
		return bDeriveProportions
			? HutongGen::Proportions::EaveOverhang(GetColumnHeight(), RearEaveOverhangRatio)
			: FMath::Max(RearRoofOverhang, 0.0);
	}

	// 廊步: facade setback from the outer column line.
	double GetVerandaDepth() const
	{
		if (!bHasFrontVeranda) return 0.0;
		return FMath::Max(bDeriveProportions ? FMath::Max(StepRun, 40.0)
											 : VerandaDepth, 0.0);
	}

	// 後廊: rear 金柱 one 廊步 in from the back wall (on the 後檐柱 line).
	double GetRearVerandaDepth() const
	{
		return (bHasFrontVeranda && bHasRearVeranda) ? GetVerandaDepth() : 0.0;
	}

	// 廊步 built on this depth, front and rear: under four column radii two rows' curved faces z-fight,
	// and verandas never eat the room.
	void GetBuiltVerandaDepths(double InDepth, double WallT, double& OutFront, double& OutRear) const
	{
		const double MinRow = 4.0 * GetColumnRadius();
		// The facade and back walls (表十二) where derived; else the thickness asked.
		const double Walls = bDeriveProportions ? GetFacadeWallThickness() + GetRearWallThickness() : 2.0 * WallT;
		const double Room = FMath::Max(InDepth - Walls - 100.0, 0.0);
		// 徹上明造: the 廊 on the frame's own lines, evenly spaced across the depth (FrameLayout::Make), which the
		// shell's creases follow — whole or not at all, so its columns never stand off the frame's beams; the rear
		// one goes first where the room cannot hold both.
		const int32 Lines = HutongGen::Jiajia::PurlinCount(Purlins);
		if (bExposedFrame && bHasFrontVeranda && Lines >= 5)
		{
			const double Step = FMath::Max(InDepth, 1.0) / (Lines - 1);
			OutFront = (Step >= MinRow && Step <= Room) ? Step : 0.0;
			OutRear = (OutFront > 0.0 && bHasRearVeranda && Step >= MinRow && OutFront + Step <= Room) ? Step : 0.0;
			return;
		}
		OutFront = GetVerandaDepth();
		if (OutFront < MinRow) OutFront = 0.0;
		OutFront = FMath::Clamp(OutFront, 0.0, Room);
		OutRear = (OutFront > 0.0) ? GetRearVerandaDepth() : 0.0;
		if (OutRear < MinRow) OutRear = 0.0;
		OutRear = FMath::Clamp(OutRear, 0.0, Room - OutFront);
	}

	// 舉架: eave overhang, then one segment per 步架.
	HutongGen::FHutongRoofSection GetRoofSection() const
	{
		return HutongGen::Jiajia::MakeSection(
			Purlins, 0.5 * FMath::Max(Depth, 1.0), GetRoofOverhang(), RoofApexRoll);
	}

	// Depth from this 檁數 and 步架; the drag snaps to it.
	double GetCanonicalDepth() const
	{
		return HutongGen::Jiajia::DepthFor(Purlins, StepRun);
	}

	// Roof above the column tops (HutongGen::Proportions::RoofLift) and the roof's base: its eave line.
	double GetRoofLift() const
	{
		const HutongGen::FHutongRoofSection S = GetRoofSection();
		return HutongGen::Proportions::RoofLift(GetColumnDiameter(), GetRoofOverhang(), HutongGen::Jiajia::BuiltEaveJu(S, GetRoofRise()));
	}
	double GetRoofBaseHeight() const { return GetEaveHeight() + GetRoofLift(); }
	double GetUndersideRise() const
	{
		const HutongGen::FHutongRoofSection S = GetRoofSection();
		return HutongGen::Proportions::UndersideRise(GetColumnDiameter(), GetRoofOverhang(), HutongGen::Jiajia::BuiltEaveJu(S, GetRoofRise()));
	}

	// Derived: the sum of the 舉 sequence over its runs.
	double GetRoofRise() const
	{
		return bDeriveProportions
			? FMath::Max(GetRoofSection().Rise(), 1.0)
			: FMath::Max(RidgeHeight, 1.0);
	}

	double GetWindowSillHeight() const
	{
		return bDeriveProportions
			? GetFloorHeight() + GetLowerSillHeight() + HutongCanon::Joinery::GeshanLowerOfWidth * GetGeshanWidth()
			: WindowSillHeight;
	}

	// 中檻: window head and door leaf head, one member so one height.
	double GetMiddleRailHeight() const
	{
		return HutongGen::Proportions::MiddleRail(GetArchitraveBottom(), GetColumnDiameter(), GetHeadRailInD());
	}

	double GetWindowTopHeight() const
	{
		return bDeriveProportions ? GetMiddleRailHeight() : WindowTopHeight;
	}

	// 額枋 underside: 柱高 less the beam depth (則例).
	double GetArchitraveBottom() const
	{
		if (!bDeriveProportions) return DoorTopHeight;
		return HutongGen::Proportions::ArchitraveBottom(GetFloorHeight(), GetColumnHeight(),
			ColumnHeightInDiameters, ArchitraveInDiameters);
	}

	// Door bay head.
	double GetDoorTopHeight() const
	{
		const double Derived = GetArchitraveBottom();
		if (!bHasFrontDoorCenter)
		{
			return Derived;
		}
		const double Need = bHasDoorFrame
			? GetDoorLeafTopHeight() + GetHeadRailHeight()
			: GetFloorHeight() + HutongGen::Passage::MinClearHeight;
		return FMath::Max(Derived, Need);
	}

	// 隔扇 head: the 中檻, where the windows stop.
	double GetDoorLeafTopHeight() const
	{
		const double Derived = bDeriveProportions ? GetMiddleRailHeight() : DoorLeafTopHeight;
		return (bHasFrontDoorCenter && bHasDoorFrame)
			? FMath::Max(Derived, HutongGen::Passage::MinHeadZ(GetFloorHeight(), GetLowerSillHeight()))
			: Derived;
	}

	// X of bay boundary Index; end boundaries inset by the column radius so corner columns sit in the side walls.
	double GetBayBoundary(int32 Index, int32 BayCount, double FacadeLength, double ColumnRadius) const
	{
		return HutongGen::BayBoundary(Index, BayCount, FacadeLength, ColumnRadius,
			bDeriveProportions ? SideBayWidthRatio : 1.0,
			GetDoorBayIndex(BayCount));
	}

	// Set by the tool from the drag rect.
	double Width = 800.0;
	double Depth = 500.0;

	// Forces the bay count; zero derives. Driven by the bracket keys.
	int32 BayCountOverride = 0;

	// Set by a composite: the roof continues past that gable onto a stretch roofed by
	// AppendHouseRoofRun, so the gable gets no rake.
	bool bRoofRunsOnLow = false;
	bool bRoofRunsOnHigh = false;
};

namespace HutongGen
{
	void BuildSiheyuan(UE::Geometry::FDynamicMesh3& Mesh, const FHutongSiheyuanParams& P);

	// The house roof (section, eave, tiles) over another stretch [X0, X1] of the same depth, past one
	// gable. RowPhase is a tile row's X, so rows run through the join.
	void AppendHouseRoofRun(UE::Geometry::FDynamicMesh3& Mesh, const FHutongSiheyuanParams& P,
		double X0, double X1, bool bOpenLow, bool bOpenHigh, double RowPhase);
}
