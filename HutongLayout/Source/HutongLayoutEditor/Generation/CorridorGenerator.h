#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongJiajia.h"
#include "CorridorGenerator.generated.h"

// 遊廊: the roofed walk linking the buildings round a courtyard, and the piece that turns four separate halls into a compound.
USTRUCT(BlueprintType)
struct FHutongCorridorParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(UIMin="200", UIMax="400", ClampMin="120", Units="cm", ToolTip="Height of the eave above the ground, in cm."))
	double EaveHeight = 265.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Min Walk Width", UIMin="80", UIMax="160", ClampMin="70", Units="cm", ToolTip="Smallest clear walk width the dragged footprint may set, in cm."))
	double WalkWidthMin = 95.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Max Walk Width", UIMin="160", UIMax="500", ClampMin="90", Units="cm", ToolTip="Largest clear walk width the dragged footprint may set, in cm."))
	double WalkWidthMax = 300.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Bay Spacing", UIMin="120", UIMax="300", ClampMin="80", Units="cm", ToolTip="Spacing between posts along the run, in cm; sets the bay count."))
	double BaySpacing = 180.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Column Diameter", UIMin="10", UIMax="30", ClampMin="5", Units="cm", ToolTip="Diameter of the posts, in cm."))
	double ColumnDiameter = 17.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Column Taper (收分)", UIMin="0", UIMax="0.02", ClampMin="0", ClampMax="0.05", ToolTip="Column taper (收分) as the fraction of column height lost from the diameter at the head."))
	double ColumnTaperRatio = HutongCanon::Module::ColumnTaperRatio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Floor Height (臺基)", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="Height of the walk platform (臺基) above the ground, in cm."))
	double FloorHeight = 22.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Floor Overhang", UIMin="0", UIMax="40", ClampMin="0", Units="cm", ToolTip="How far the platform projects past the posts on each side, in cm."))
	double FloorOverhang = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Closed On One Side", ToolTip="Closes one side of the walk, omitting its posts, architrave (額枋) and frieze."))
	bool bClosedSide = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Builds Its Own Back Wall", EditCondition="bClosedSide", ToolTip="Builds a wall along the closed side."))
	bool bBuildBackWall = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Back Wall Thickness", EditCondition="bClosedSide && bBuildBackWall", UIMin="10", UIMax="50", ClampMin="5", Units="cm", ToolTip="Thickness of the back wall, in cm."))
	double WallThickness = 24.0;

	// --- 楣子 ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Has Hanging Frieze (倒掛楣子)", ToolTip="Builds a hanging frieze (倒掛楣子) under the architrave (額枋) in each bay."))
	bool bHasFrieze = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Frieze Drop", EditCondition="bHasFrieze", UIMin="15", UIMax="80", ClampMin="5", Units="cm", ToolTip="Height the frieze hangs down from the architrave (額枋), in cm."))
	double FriezeDrop = 34.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Frieze Bar Section", EditCondition="bHasFrieze", UIMin="2", UIMax="10", ClampMin="1", Units="cm", ToolTip="Cross-section size of the frieze bars, in cm."))
	double FriezeBarSection = 4.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Frieze Bars Per Bay", EditCondition="bHasFrieze", UIMin="2", UIMax="12", ClampMin="0", ClampMax="24", ToolTip="Number of vertical bars in the frieze of each bay."))
	int32 FriezeBars = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Has Bench Rail (坐凳楣子)", ToolTip="Builds a bench rail (坐凳楣子) along each open side."))
	bool bHasBench = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Bench Height", EditCondition="bHasBench", UIMin="30", UIMax="70", ClampMin="15", Units="cm", ToolTip="Height of the bench seat above the floor, in cm."))
	double BenchHeight = 46.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Bench Depth", EditCondition="bHasBench", UIMin="15", UIMax="50", ClampMin="8", Units="cm", ToolTip="Depth of the bench seat, in cm."))
	double BenchDepth = 26.0;

	// --- Roof ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="10", UIMax="90", ClampMin="0", Units="cm", ToolTip="How far the eave projects past the posts, in cm."))
	double RoofOverhang = 42.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Gable Overhang (懸山)", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="How far the roof projects past each end of the run, in cm; 0 keeps a flush gable."))
	double GableOverhang = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="15", UIMax="150", ClampMin="5", Units="cm", ToolTip="Height of the ridge above the eave, in cm."))
	double RoofRise = 58.0;

	double GetEaveHeight() const { return FMath::Max(EaveHeight, 60.0); }
	double GetRoofRise() const { return FMath::Max(RoofRise, 5.0); }


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Apex Roll (捲棚)", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="How much the roof apex is rounded into a rolled ridge (捲棚) crown; 0 keeps a sharp fold."))
	double RoofApexRoll = 0.45;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Depth", UIMin="0", UIMax="20", ClampMin="0", Units="cm", ToolTip="Vertical depth of the fascia band along the eave, in cm."))
	double EaveFasciaDepth = 7.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Width", UIMin="4", UIMax="30", ClampMin="1", Units="cm", ToolTip="Horizontal width of the fascia band along the eave, in cm."))
	double EaveFasciaWidth = 11.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Section (椽頭)", UIMin="0", UIMax="14", ClampMin="0", Units="cm", ToolTip="Cross-section size of the rafter ends (椽頭) under the eave, in cm; 0 omits them."))
	double RafterEndSection = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Spacing", EditCondition="RafterEndSection > 0", UIMin="8", UIMax="40", ClampMin="4", Units="cm", ToolTip="Spacing between rafter ends along the eave, in cm."))
	double RafterEndSpacing = 16.0;

	// Set by the tool from the drag rect; not user-editable.
	double Length = 800.0;
	double Width = 150.0;

	// Where the 坐凳楣子 breaks so the walk can be stepped onto, as a fraction along the run; negative leaves it unbroken.
	double BenchGapAt = -1.0;

	// Whole bays are cleared, so this only has to name the opening.
	double BenchGapWidth = 130.0;

	double GetColumnRadius() const { return FMath::Max(0.5 * ColumnDiameter, 3.0); }

	// 步 spacing: the bays fall out of the run rather than being counted, and the posts are inset
	// by a radius at each end so they sit inside the platform. The generator and the plan both
	// read the posts out of here.
	int32 GetBayCount(double RunLength) const
	{
		return FMath::Max(FMath::RoundToInt32(FMath::Max(RunLength, 1.0) / FMath::Max(BaySpacing, 40.0)), 1);
	}

	double GetBayBoundary(int32 Index, int32 BayCount, double RunLength) const
	{
		const double ColR = GetColumnRadius();
		const double T = double(Index) / double(FMath::Max(BayCount, 1));
		return ColR + T * FMath::Max(RunLength - 2.0 * ColR, 1.0);
	}

	// Everything across the run that is not the walk: the back wall if there is one, the platform's
	// lip either side, and the open column line's own radius — the walk is measured from the column
	// centre, so a footprint that leaves the radius out is one the built platform overruns, and in
	// the compound's ring that is a run standing in the building it abuts.
	double GetCrossExtras() const
	{
		const double Wall = (bClosedSide && bBuildBackWall) ? FMath::Max(WallThickness, 1.0) : 0.0;
		return Wall + 2.0 * FMath::Max(FloorOverhang, 0.0) + GetColumnRadius();
	}

	double GetFootprintDepth() const { return FMath::Max(Width, 1.0) + GetCrossExtras(); }

	// Clear walk width for a dragged footprint depth, held inside the band.
	double WalkWidthFromFootprint(double FootprintDepth) const
	{
		const double Lo = FMath::Max(WalkWidthMin, 10.0);
		const double Hi = FMath::Max(WalkWidthMax, Lo + 1.0);
		return FMath::Clamp(FootprintDepth - GetCrossExtras(), Lo, Hi);
	}
};

namespace HutongGen
{
	void BuildCorridor(UE::Geometry::FDynamicMesh3& Mesh, const FHutongCorridorParams& P);
}
