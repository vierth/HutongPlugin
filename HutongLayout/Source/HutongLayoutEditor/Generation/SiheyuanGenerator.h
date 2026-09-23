#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongBays.h"
#include "Generation/HutongCanon.h"
#include "Generation/HutongProportions.h"
#include "Generation/HutongDoor.h"
#include "Generation/HutongApron.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRoofTile.h"
#include "Generation/HutongRearEave.h"
#include "SiheyuanGenerator.generated.h"

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rear Eave (後檐)", ToolTip="What the back of the building faces."))
	EHutongRearEave RearEave = EHutongRearEave::Lane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Rear Eave Overhang / Column Height (封護檐)", EditCondition="bDeriveProportions && RearEave == EHutongRearEave::Lane", UIMin="0", UIMax="0.1", ClampMin="0", ClampMax="1", ToolTip="Rear roof overhang as a fraction of the column height."))
	double RearEaveOverhangRatio = HutongCanon::Module::LaneEaveOverhangRatio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Has Front Veranda (前廊)", ToolTip="Sets the facade back behind a front colonnade to form a veranda (前廊)."))
	bool bHasFrontVeranda = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Has Rear Veranda (後廊)", EditCondition="bHasFrontVeranda", ToolTip="Makes the frame 前後廊: a row of rear 金柱 one 廊步 inside the back wall, the rear veranda enclosed in the room and the ridge over the middle of the plan."))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Sill Height Above Floor (檻牆)", EditCondition="bDeriveProportions", UIMin="60", UIMax="120", ClampMin="20", Units="cm", ToolTip="Height of the window sill above the interior floor, in cm."))
	double SillHeightAboveFloor = HutongCanon::Openings::SillHeightAboveFloorCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proportions", meta=(DisplayName="Transom Band (橫披窗)", EditCondition="bDeriveProportions", UIMin="0", UIMax="0.3", ClampMin="0", ClampMax="0.5", ToolTip="Share of the opening height taken by the transom band."))
	double TransomBandFraction = HutongCanon::Openings::TransomBandFraction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout", meta=(DisplayName="Suggested Frontage (面闊)", UIMin="0", UIMax="2000", ClampMin="0", Units="cm", ToolTip="Frontage the drag snaps to, in cm; zero leaves the drag free."))
	double SuggestedFrontage = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout", meta=(DisplayName="Suggested Depth (進深)", UIMin="0", UIMax="1200", ClampMin="0", Units="cm", ToolTip="Depth the drag snaps to, in cm; zero derives it."))
	double SuggestedDepth = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Layout", meta=(DisplayName="Snap To Suggested", EditCondition="SuggestedFrontage > 0 || SuggestedDepth > 0", ToolTip="Snaps the dragged footprint to the suggested frontage and depth."))
	bool bSnapToSuggested = true;

	double GetSuggestedDepth() const
	{
		if (SuggestedDepth > 0.0) return SuggestedDepth;
		return bDeriveProportions ? GetCanonicalDepth() : 0.0;
	}

	// Snaps one dragged extent to whichever suggestion it is nearest; FrontageExtra is what a
	// composition adds beside the room, so its frontage target is the room's plus that.
	double SnapExtent(double Extent, double FrontageExtra = 0.0) const
	{
		if (!bSnapToSuggested) return Extent;

		double Best = Extent;
		double BestDelta = TNumericLimits<double>::Max();
		for (const double Target : { SuggestedFrontage > 0.0 ? SuggestedFrontage + FrontageExtra : 0.0, GetSuggestedDepth() })
		{
			if (Target <= 0.0) continue;
			const double Delta = FMath::Abs(Extent - Target);
			// Proportional with a floor: a tolerance that suits a 12 m frontage is invisible on 3 m.
			if (Delta < BestDelta && Delta <= FMath::Max(60.0, 0.1 * Target))
			{
				Best = Target;
				BestDelta = Delta;
			}
		}
		return Best;
	}

	// After a field is edited in a panel: the contradictions the generator would clamp silently,
	// resolved by pushing whichever field the user did not just touch, so the panel shows what is built.
	void ClampAfterEdit(FName Changed)
	{
		if (Changed == TEXT("MinBayWidth"))      MaxBayWidth = FMath::Max(MaxBayWidth, MinBayWidth);
		else if (Changed == TEXT("MaxBayWidth")) MinBayWidth = FMath::Min(MinBayWidth, MaxBayWidth);

		// Openings stack bottom-to-top: sill < window top <= door top < eave.
		DoorTopHeight = FMath::Min(DoorTopHeight, EaveHeight - 10.0);
		if (Changed == TEXT("WindowSillHeight")) WindowTopHeight = FMath::Max(WindowTopHeight, WindowSillHeight + 10.0);
		else                                     WindowSillHeight = FMath::Min(WindowSillHeight, WindowTopHeight - 10.0);
		WindowTopHeight = FMath::Min(WindowTopHeight, DoorTopHeight);
		FloorHeight = FMath::Min(FloorHeight, EaveHeight * 0.2);

		// The door leaves have to end below the lintel, or the transom above them inverts.
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Spacing", EditCondition="RafterEndSection > 0", UIMin="10", UIMax="60", ClampMin="5", Units="cm", ToolTip="Distance between rafter ends along the eave, in cm."))
	double RafterEndSpacing = 24.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Flying Rafters (飛椽)", EditCondition="RafterEndSection > 0", ToolTip="Adds square flying rafters (飛椽) over the eave rafters."))
	bool bHasFlyingRafters = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Tile (瓦作)", ToolTip="Tile type laid on the roof."))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge End Kick (蠍子尾)", EditCondition="bHasRidgeCourse", UIMin="0", UIMax="80", Units="cm", ToolTip="Rise of each ridge end past the gable, in cm; zero squares it."))
	double RidgeEndKick = 32.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(UIMin="10", UIMax="80", Units="cm", ToolTip="Thickness of the side and rear walls, in cm."))
	double WallThickness = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(UIMin="10", UIMax="80", Units="cm", ToolTip="Thickness of the facade infill panels between the columns, in cm."))
	double PostThickness = 22.0;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Apron Paving (散水)", ShowOnlyInnerProperties, ToolTip="Settings for the paved apron (散水) round the foot of the building."))
	FHutongApronParams Apron;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Platform Front Overhang", UIMin="0", UIMax="120", Units="cm", ToolTip="How far the platform projects in front of the facade, in cm."))
	double PlatformOverhang = 35.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(UIMin="0", UIMax="5", ClampMin="0", ClampMax="8", ToolTip="Number of steps (踏跺) in front of the door bay."))
	int32 StepCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(UIMin="15", UIMax="80", ClampMin="5", Units="cm", ToolTip="Depth of each step tread, in cm."))
	double StepTread = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Veranda End Doorways (廊門筒子)", EditCondition="bHasFrontVeranda", ToolTip="Opens a doorway through each gable wall across the front veranda (前廊), so a corridor (遊廊) meeting the gable walks straight on along the veranda."))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Window Mullions", EditCondition="bHasWindowLattice", UIMin="0", UIMax="12", ClampMin="0", ClampMax="24", ToolTip="Number of vertical bars across each window."))
	int32 WindowMullions = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Window Rails", EditCondition="bHasWindowLattice", UIMin="0", UIMax="8", ClampMin="0", ClampMax="16", ToolTip="Number of horizontal bars across each window."))
	int32 WindowRails = 1;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Has Front Door", ToolTip="Opens a door bay in the facade."))
	bool bHasFrontDoorCenter = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Door Bay", EditCondition="bHasFrontDoorCenter", UIMin="-1", UIMax="8", ClampMin="-1", ClampMax="31", ToolTip="Which bay holds the door, from the origin end; -1 centres it."))
	int32 DoorBayIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Has Door Frame (抱框)", EditCondition="bHasFrontDoorCenter", ToolTip="Adds jambs, a threshold, leaves and a transom to the door bay."))
	bool bHasDoorFrame = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Door Width Fraction", EditCondition="bHasDoorFrame", UIMin="0.3", UIMax="1.0", ClampMin="0.15", ClampMax="1.0", ToolTip="Doorway width as a fraction of the door bay's width."))
	double DoorWidthFraction = HutongCanon::Openings::DoorWidthFraction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Door Leaf Top Height", EditCondition="bHasDoorFrame && !bDeriveProportions", UIMin="180", UIMax="320", ClampMin="50", Units="cm", ToolTip="Height of the top of the door leaves above the ground, in cm."))
	double DoorLeafTopHeight = 245.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Door Leaves Open", EditCondition="bHasDoorFrame", ToolTip="Swings the door leaves open onto the courtyard."))
	bool bDoorLeavesOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Door Frame Thickness", EditCondition="bHasDoorFrame", UIMin="4", UIMax="20", ClampMin="2", Units="cm", ToolTip="Thickness of the door jambs and head, in cm."))
	double DoorFrameThickness = 9.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Threshold Height", EditCondition="bHasDoorFrame", UIMin="0", UIMax="30", ClampMin="0", Units="cm", ToolTip="Height of the door threshold (門檻), in cm."))
	double DoorThresholdHeight = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Window Paper (窗紙)", ToolTip="Adds a paper pane (窗紙) behind each window lattice."))
	bool bHasWindowPaper = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Door Transom Mullions", EditCondition="bHasDoorFrame", UIMin="0", UIMax="12", ClampMin="0", ClampMax="24", ToolTip="Number of bars across the transom above the door leaves."))
	int32 DoorTransomMullions = 3;

	// Single source of truth for the generator and the tool's preview, which have to agree or the highlighted bay is not the built one.
	int32 GetDoorBayIndex(int32 BayCount) const
	{
		if (BayCount <= 0) return 0;
		return (DoorBayIndex < 0) ? BayCount / 2 : FMath::Clamp(DoorBayIndex, 0, BayCount - 1);
	}

	// The lowest eave that can carry a walkable doorway, zero when there is no front door.
	double GetMinEaveHeight() const
	{
		if (!bHasFrontDoorCenter)
		{
			return 0.0;
		}
		// Without a frame the bay is an open hole up to the lintel, so only the clearance counts.
		const double Need = bHasDoorFrame
			? FMath::Clamp(DoorThresholdHeight, 0.0, 40.0) + HutongGen::Passage::MinClearHeight
				+ FMath::Max(DoorFrameThickness, 2.0)
			: HutongGen::Passage::MinClearHeight;

		if (!bDeriveProportions)
		{
			// BuildSiheyuan holds the lintel 10 cm below the eave; 20 keeps that clamp off the door.
			return FloorHeight + Need + 20.0;
		}
		return HutongGen::Proportions::MinEaveForHeadroom(Need,
			ColumnHeightInDiameters, PlatformHeightInDiameters,
			ArchitraveInDiameters, TransomBandFraction);
	}

	int32 GetBayCount() const
	{
		return (BayCountOverride > 0)
			? BayCountOverride
			: HutongGen::ComputeBayCount(Width, MinBayWidth, MaxBayWidth);
	}

	// 明間面闊, the module 檐柱高 is set against. Falls out of the same arithmetic BayBoundary uses.
	double GetCentralBayWidth() const
	{
		return HutongGen::CentralBayWidth(Width, GetBayCount(),
			bDeriveProportions ? SideBayWidthRatio : 1.0);
	}

	// 檐柱高 = 8/10 明間面闊, through the same platform-and-column solve GetFloorHeight does.
	double GetEaveHeightFromBays() const
	{
		const double Col = HutongGen::Proportions::ColumnFromCentralBay(
			GetCentralBayWidth(), ColumnHeightPerBay, MinColumnHeight);
		return HutongGen::Proportions::EaveFromColumn(Col,
			ColumnHeightInDiameters, PlatformHeightInDiameters);
	}

	double GetEaveHeight() const
	{
		const double Asked = (bDeriveProportions && bDeriveEaveFromBays)
			? GetEaveHeightFromBays() : EaveHeight;
		return FMath::Max(Asked, GetMinEaveHeight());
	}

	// 臺明高 = k 柱徑 and 柱高 = R 柱徑 measure off the same eave, so solving the two together gives Floor = k·Eave/(R+k).
	double GetFloorHeight() const
	{
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

	// The radius the generator lays the columns on, floored the way it floors it.
	double GetColumnRadius() const { return FMath::Max(0.5 * GetColumnDiameter(), 1.0); }

	// The projection the pier is built with: never less than covers the corner column.
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

	// A back giving onto the courtyard is a front.
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

	// 廊步: how far the facade retreats from the outer column line.
	double GetVerandaDepth() const
	{
		if (!bHasFrontVeranda) return 0.0;
		return FMath::Max(bDeriveProportions ? FMath::Max(StepRun, 40.0)
											 : VerandaDepth, 0.0);
	}

	// 後廊: the rear 金柱 stand one 廊步 in from the back wall, which is on the 後檐柱 line.
	double GetRearVerandaDepth() const
	{
		return (bHasFrontVeranda && bHasRearVeranda) ? GetVerandaDepth() : 0.0;
	}

	// The 廊步 actually built on a footprint this deep, front and rear: under four column radii two
	// rows' curved faces nearly coincide and z-fight, and the verandas never eat the room.
	void GetBuiltVerandaDepths(double InDepth, double WallT, double& OutFront, double& OutRear) const
	{
		const double MinRow = 4.0 * GetColumnRadius();
		const double Room = FMath::Max(InDepth - 2.0 * WallT - 100.0, 0.0);
		OutFront = GetVerandaDepth();
		if (OutFront < MinRow) OutFront = 0.0;
		OutFront = FMath::Clamp(OutFront, 0.0, Room);
		OutRear = (OutFront > 0.0) ? GetRearVerandaDepth() : 0.0;
		if (OutRear < MinRow) OutRear = 0.0;
		OutRear = FMath::Clamp(OutRear, 0.0, Room - OutFront);
	}

	// 舉架: the eave overhang, then one segment per 步架 in from it.
	HutongGen::FHutongRoofSection GetRoofSection() const
	{
		return HutongGen::Jiajia::MakeSection(
			Purlins, 0.5 * FMath::Max(Depth, 1.0), GetRoofOverhang(), RoofApexRoll);
	}

	// The depth this 檁數 and 步架 come to. What the drag snaps to.
	double GetCanonicalDepth() const
	{
		return HutongGen::Jiajia::DepthFor(Purlins, StepRun);
	}

	// A consequence, not a setting: whatever the 舉 sequence adds up to over the runs it is given.
	double GetRoofRise() const
	{
		return bDeriveProportions
			? FMath::Max(GetRoofSection().Rise(), 1.0)
			: FMath::Max(RidgeHeight, 1.0);
	}

	double GetWindowSillHeight() const
	{
		return bDeriveProportions
			? GetFloorHeight() + FMath::Max(SillHeightAboveFloor, 10.0)
			: WindowSillHeight;
	}

	// 中檻, where both the window head and the door leaf head sit — one member across the bay, so one height.
	double GetMiddleRailHeight() const
	{
		return HutongGen::Proportions::MiddleRail(GetArchitraveBottom(), GetFloorHeight(),
			TransomBandFraction);
	}

	double GetWindowTopHeight() const
	{
		return bDeriveProportions ? GetMiddleRailHeight() : WindowTopHeight;
	}

	// 額枋's underside: 柱高 less the beam's own depth, which is what 則例 states.
	double GetArchitraveBottom() const
	{
		if (!bDeriveProportions) return DoorTopHeight;
		return HutongGen::Proportions::ArchitraveBottom(GetFloorHeight(), GetColumnHeight(),
			ColumnHeightInDiameters, ArchitraveInDiameters);
	}

	// The head of the door bay.
	double GetDoorTopHeight() const
	{
		const double Derived = GetArchitraveBottom();
		if (!bHasFrontDoorCenter)
		{
			return Derived;
		}
		const double Need = bHasDoorFrame
			? GetDoorLeafTopHeight() + FMath::Max(DoorFrameThickness, 2.0)
			: GetFloorHeight() + HutongGen::Passage::MinClearHeight;
		return FMath::Max(Derived, Need);
	}

	// 隔扇 head — the 中檻 again, the same member the windows stop at.
	double GetDoorLeafTopHeight() const
	{
		const double Derived = bDeriveProportions ? GetMiddleRailHeight() : DoorLeafTopHeight;
		return (bHasFrontDoorCenter && bHasDoorFrame)
			? FMath::Max(Derived, HutongGen::Passage::MinHeadZ(GetFloorHeight(), DoorThresholdHeight))
			: Derived;
	}

	// X of bay boundary Index, with the end boundaries inset by the column radius so the corner columns sit inside the side walls.
	double GetBayBoundary(int32 Index, int32 BayCount, double FacadeLength, double ColumnRadius) const
	{
		return HutongGen::BayBoundary(Index, BayCount, FacadeLength, ColumnRadius,
			bDeriveProportions ? SideBayWidthRatio : 1.0,
			GetDoorBayIndex(BayCount));
	}

	// Tool-driven from the drag rect; not user-editable.
	double Width = 800.0;
	double Depth = 500.0;

	// Forces the bay count instead of deriving it. Zero means derive. Driven by the bracket keys.
	int32 BayCountOverride = 0;
};

namespace HutongGen
{
	void BuildSiheyuan(UE::Geometry::FDynamicMesh3& Mesh, const FHutongSiheyuanParams& P);
}
