#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongProportions.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongJiajia.h"
#include "CorridorGenerator.generated.h"

// 遊廊: roofed walk linking the buildings round a courtyard.
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Bay Spacing", UIMin="120", UIMax="300", ClampMin="80", Units="cm", ToolTip="Spacing between posts along the run, in cm."))
	double BaySpacing = 180.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Column Diameter", UIMin="10", UIMax="30", ClampMin="5", Units="cm", ToolTip="Diameter of the posts, in cm."))
	double ColumnDiameter = 17.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Column Taper (收分)", UIMin="0", UIMax="0.02", ClampMin="0", ClampMax="0.05", ToolTip="Column taper (收分) as a fraction of the column height."))
	double ColumnTaperRatio = HutongCanon::Module::ColumnTaperRatio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Floor Height (臺基)", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="Height of the walk platform (臺基) above the ground, in cm."))
	double FloorHeight = 22.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Floor Overhang", UIMin="0", UIMax="40", ClampMin="0", Units="cm", ToolTip="How far the platform projects past the posts on each side, in cm."))
	double FloorOverhang = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(HutongBasic, DisplayName="Closed On One Side", ToolTip="Closes one side of the walk."))
	bool bClosedSide = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Builds Its Own Back Wall", EditCondition="bClosedSide", ToolTip="Builds a wall along the closed side."))
	bool bBuildBackWall = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corridor", meta=(DisplayName="Back Wall Thickness", EditCondition="bClosedSide && bBuildBackWall", UIMin="10", UIMax="50", ClampMin="5", Units="cm", ToolTip="Thickness of the back wall, in cm."))
	double WallThickness = 24.0;

	// --- 楣子 ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(HutongBasic, DisplayName="Has Hanging Frieze (倒掛楣子)", ToolTip="Builds a hanging frieze (倒掛楣子) under the architrave (額枋)."))
	bool bHasFrieze = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Frieze Drop", EditCondition="bHasFrieze", UIMin="15", UIMax="80", ClampMin="5", Units="cm", ToolTip="Height the frieze hangs down from the architrave (額枋), in cm."))
	double FriezeDrop = 34.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Frieze Bar Section", EditCondition="bHasFrieze", UIMin="2", UIMax="10", ClampMin="1", Units="cm", ToolTip="Cross-section size of the frieze bars, in cm."))
	double FriezeBarSection = 4.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Frieze Bars Per Bay", EditCondition="bHasFrieze", UIMin="2", UIMax="12", ClampMin="0", ClampMax="24", ToolTip="Number of vertical bars in the frieze of each bay."))
	int32 FriezeBars = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(HutongBasic, DisplayName="Has Bench Seat", ToolTip="Lays a bench seat on the lattice rail along each open side."))
	bool bHasBench = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(HutongBasic, DisplayName="Lattice Rail (坐凳楣子)", ToolTip="Fills each bay of the open side with a low lattice panel, under the bench seat if there is one."))
	bool bHasBenchLattice = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Bench Height", EditCondition="bHasBench", UIMin="30", UIMax="70", ClampMin="15", Units="cm", ToolTip="Height of the bench seat above the floor, in cm."))
	double BenchHeight = 46.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Bench Depth", EditCondition="bHasBench", UIMin="15", UIMax="50", ClampMin="8", Units="cm", ToolTip="Depth of the bench seat, in cm."))
	double BenchDepth = 26.0;

	// --- Roof ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="10", UIMax="90", ClampMin="0", Units="cm", ToolTip="How far the eave projects past the posts, in cm."))
	double RoofOverhang = 42.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Gable Overhang (懸山)", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="Roof projection past each end of the run, in cm; 0 keeps a flush gable."))
	double GableOverhang = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="15", UIMax="150", ClampMin="5", Units="cm", ToolTip="Height of the ridge above the eave, in cm."))
	double RoofRise = 58.0;

	double GetEaveHeight() const { return FMath::Max(EaveHeight, 60.0); }
	// Roof above the column tops, the ceiling at the column line, and the roof's base
	// (HutongGen::Proportions::RoofLift).
	// The eave step's 舉 of the roof as built: the section scaled to the fixed rise.
	double GetEaveJu() const { return HutongGen::Jiajia::BuiltEaveJu(HutongGen::Jiajia::MakeSection(EHutongPurlins::Three, 0.5 * GetFootprintDepth(), FMath::Max(RoofOverhang, 0.0), GetRoofRoll()), GetRoofRise()); }
	double GetRoofLift() const { return HutongGen::Proportions::RoofLift(FMath::Max(ColumnDiameter, 2.0), FMath::Max(RoofOverhang, 0.0), GetEaveJu()); }
	double GetUndersideRise() const { return HutongGen::Proportions::UndersideRise(FMath::Max(ColumnDiameter, 2.0), FMath::Max(RoofOverhang, 0.0), GetEaveJu()); }
	double GetRoofBaseHeight() const { return GetEaveHeight() + GetRoofLift(); }
	double GetRoofRise() const { return FMath::Max(RoofRise, 5.0); }


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Apex Roll (捲棚)", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="Rounding of the roof apex into a rolled ridge (捲棚); 0 keeps the fold. Never wider than the top step (頂步) between the two top purlins."))
	double RoofApexRoll = 0.45;

	// The roll as built: a 四檁卷棚's crown spans only its 頂步, a fifth of the depth between the two 頂檁
	// (圖18, and the 抄手遊廊 section), as a fraction of the half span (overhang included).
	double GetRoofRoll() const
	{
		if (RoofApexRoll <= 0.0) return 0.0;
		const double Dp = GetFootprintDepth();
		return FMath::Min(RoofApexRoll, 0.1 * Dp / FMath::Max(0.5 * Dp + FMath::Max(RoofOverhang, 0.0), 1.0));
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Depth", UIMin="0", UIMax="20", ClampMin="0", Units="cm", ToolTip="Vertical depth of the fascia band along the eave, in cm."))
	double EaveFasciaDepth = 7.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Width", UIMin="4", UIMax="30", ClampMin="1", Units="cm", ToolTip="Horizontal width of the fascia band along the eave, in cm."))
	double EaveFasciaWidth = 11.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Section (椽頭)", UIMin="0", UIMax="14", ClampMin="0", Units="cm", ToolTip="Section size of the rafter ends (椽頭), in cm; 0 omits them."))
	double RafterEndSection = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Courses (壟)", ToolTip="Model each course of tiles running down the roof, rather than leaving the texture to draw it."))
	bool bHasTileRuns = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Exposed Frame (徹上明造)", ToolTip="No ceiling: the roof is a shell on rafters carried by the roof frame (梁架: beams, posts and purlins), open to view from below."))
	bool bExposedFrame = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Spacing", EditCondition="RafterEndSection > 0", UIMin="8", UIMax="40", ClampMin="4", Units="cm", ToolTip="Spacing between rafter ends along the eave, in cm."))
	double RafterEndSpacing = 16.0;

	// Set by the tool from the drag rect.
	double Length = 800.0;
	double Width = 150.0;

	// 坐凳楣子 break for stepping on, as a fraction along the run; negative = unbroken.
	double BenchGapAt = -1.0;

	// Whole bays are cleared; this only names the opening.
	double BenchGapWidth = 130.0;

	// Set from the component: the low/high end post (footprint frame) belongs to the neighbouring
	// run round a corner; not built twice.
	bool bOmitLowEndPost = false;
	bool bOmitHighEndPost = false;

	// Set from the component: no bench in that end bay, where the walk turns through a doorway on
	// the bench side (a 廂房's 廊門筒子).
	bool bNoBenchAtLowEnd = false;
	bool bNoBenchAtHighEnd = false;

	double GetColumnRadius() const { return FMath::Max(0.5 * ColumnDiameter, 3.0); }

	// 步 spacing: bays fall out of the run, not counted; posts inset by a radius so they sit on the
	// platform. Generator and plan both read posts from here.
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

	// Cross-run extras beside the walk: back wall, platform lip each side, open column radius. The walk
	// is measured from column centre; omit the radius and the platform overruns the footprint (in the
	// compound ring, into the abutting building).
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
