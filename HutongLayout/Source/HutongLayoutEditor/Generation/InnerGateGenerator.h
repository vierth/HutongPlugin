#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongProportions.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongDoorStone.h"
#include "Generation/HutongDoor.h"
#include "InnerGateGenerator.generated.h"

// Which 垂花門.
UENUM(BlueprintType)
enum class EHutongInnerGateStyle : uint8
{
	SinglePost UMETA(DisplayName="Single Post (獨立柱擔梁式)", ToolTip="One column pair in the middle of the depth carrying a beam cantilevered both ways, a hanging post at each of its four ends; the doors between the columns."),
	OneHallOneRoll UMETA(DisplayName="Hall and Roll (一殿一卷式)", ToolTip="Two column rows: the front carries the doors in the wall line with hanging posts cantilevered ahead of it under a gable roof, the rear a screen door (屏門) under a rolled roof; the sides between open onto the covered walk."),
};

// 垂花門: inner gate to the family's private courtyard.
USTRUCT(BlueprintType)
struct FHutongInnerGateParams
{
	GENERATED_BODY()

	FHutongInnerGateParams()
	{
		// Small: inner-gate stones are not the public face; the shared defaults are a street gate's.
		DoorStones.BlockHeight = HutongCanon::Stone::InnerGateHeightCm;
		DoorStones.Projection = HutongCanon::Stone::InnerGateProjectionCm;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(HutongBasic, DisplayName="Style", ToolTip="Which inner gate (垂花門): the single-post form, or the hall-and-roll form of a large court."))
	EHutongInnerGateStyle Style = EHutongInnerGateStyle::SinglePost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Hanging Post Reach", EditCondition="Style == EHutongInnerGateStyle::OneHallOneRoll", UIMin="40", UIMax="120", ClampMin="20", Units="cm", ToolTip="How far ahead of the front columns the beams carry the hanging posts (垂蓮柱), in cm."))
	double CantileverLength = HutongCanon::Gate::InnerGateCantileverCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(HutongBasic, DisplayName="Screen Door Open (屏門)", EditCondition="Style == EHutongInnerGateStyle::OneHallOneRoll", ToolTip="Folds the rear screen door (屏門) open so the way runs straight through; shut, it turns the way aside onto the covered walk."))
	bool bScreenDoorOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Constrain To Historical Size", ToolTip="Holds the footprint and eave height within the inner gate's (垂花門) size band."))
	bool bConstrainToHistoricalSize = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(UIMin="240", UIMax="420", ClampMin="120", Units="cm", ToolTip="Height of the eave above the ground, in cm."))
	double EaveHeight = 310.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Floor Height (臺基)", UIMin="0", UIMax="70", ClampMin="0", Units="cm", ToolTip="Height of the platform (臺基) above the ground, in cm."))
	double FloorHeight = 32.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Platform Overhang", UIMin="0", UIMax="80", ClampMin="0", Units="cm", ToolTip="Platform projection past the column line, front and back, in cm."))
	double PlatformOverhang = 20.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Step Count", UIMin="0", UIMax="4", ClampMin="0", ClampMax="6", ToolTip="Number of steps (踏跺) in the flight on each face."))
	int32 StepCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Step Tread", UIMin="20", UIMax="45", ClampMin="10", Units="cm", ToolTip="Depth of each step tread, in cm."))
	double StepTread = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Column Diameter", UIMin="15", UIMax="45", ClampMin="6", Units="cm", ToolTip="Diameter of each column at its base, in cm."))
	double ColumnDiameter = 26.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Column Taper (收分)", UIMin="0", UIMax="0.02", ClampMin="0", ClampMax="0.05", ToolTip="How much each column narrows toward its top, as a fraction of its height."))
	double ColumnTaperRatio = HutongCanon::Module::ColumnTaperRatio;

	// --- 垂蓮柱 ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hanging Posts", meta=(DisplayName="Has Hanging Posts (垂蓮柱)", ToolTip="Builds the hanging lotus posts (垂蓮柱) off the beam ends."))
	bool bHasHangingPosts = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hanging Posts", meta=(DisplayName="Post Drop", EditCondition="bHasHangingPosts", UIMin="30", UIMax="120", ClampMin="10", Units="cm", ToolTip="How far each hanging post drops below the beam, in cm."))
	double HangingPostDrop = 72.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hanging Posts", meta=(DisplayName="Post Diameter", EditCondition="bHasHangingPosts", UIMin="10", UIMax="40", ClampMin="4", Units="cm", ToolTip="Diameter of each hanging post's shaft, in cm."))
	double HangingPostDiameter = 27.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hanging Posts", meta=(DisplayName="Bud Fraction", EditCondition="bHasHangingPosts", UIMin="0.25", UIMax="0.7", ClampMin="0.1", ClampMax="0.85", ToolTip="Fraction of each hanging post's drop taken by the lotus bud at its end."))
	double BudFraction = 0.45;

	// --- Front ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Front", meta=(DisplayName="Has Frieze Panel (花板)", ToolTip="Builds the fretwork panel (花板) band between the beam and the eave."))
	bool bHasFriezePanel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Front", meta=(DisplayName="Has Brackets (雀替)", ToolTip="Builds a sparrow brace (雀替) in each top corner between beam and post."))
	bool bHasBrackets = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Front", meta=(DisplayName="Bracket Reach", EditCondition="bHasBrackets", UIMin="15", UIMax="80", ClampMin="5", Units="cm", ToolTip="Length of each sparrow brace (雀替) along the beam, in cm."))
	double BracketReach = 26.0;

	// --- Door ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(DisplayName="Door Width Fraction", UIMin="0.35", UIMax="0.8", ClampMin="0.2", ClampMax="1", ToolTip="Width of the doorway as a fraction of the bay width."))
	double DoorWidthFraction = 0.52;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(DisplayName="Door Head Height", UIMin="180", UIMax="320", ClampMin="100", Units="cm", ToolTip="Height of the underside of the door head above the platform, in cm."))
	double DoorHeadHeight = 235.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(DisplayName="Door Frame Thickness", UIMin="4", UIMax="20", ClampMin="2", Units="cm", ToolTip="Thickness of the door jambs and head, in cm."))
	double DoorFrameThickness = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(DisplayName="Threshold Height (門檻)", UIMin="0", UIMax="30", ClampMin="0", Units="cm", ToolTip="Height of the threshold (門檻) above the platform, in cm."))
	double ThresholdHeight = 14.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(HutongBasic, DisplayName="Door Leaves Open", ToolTip="Folds both door leaves open flat; off shuts them across the doorway."))
	bool bDoorLeavesOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(DisplayName="Door Pegs (門簪)", UIMin="0", UIMax="4", ClampMin="0", ClampMax="6", ToolTip="Number of door pegs (門簪) across the door head."))
	int32 DoorPegCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(HutongBasic, DisplayName="Door Stones (門枕石)", ToolTip="Settings for the door pivot stones (門枕石) at the foot of each jamb."))
	FHutongDoorStoneParams DoorStones;

	// --- Roof ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="20", UIMax="140", ClampMin="0", Units="cm", ToolTip="How far the eave projects past the column line, in cm."))
	double RoofOverhang = 56.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Gable Overhang (懸山)", UIMin="0", UIMax="90", ClampMin="0", Units="cm", ToolTip="How far the roof extends past each gable end, in cm."))
	double GableOverhang = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="30", UIMax="200", ClampMin="10", Units="cm", ToolTip="Height of the ridge above the eave, in cm."))
	double RoofRise = 82.0;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Apex Roll (捲棚)", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="Rounding of the roof apex into a rolled ridge (捲棚); 0 keeps the fold."))
	double RoofApexRoll = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Has Ridge Course (正脊)", ToolTip="Builds a main ridge (正脊) course along the roof apex."))
	bool bHasRidgeCourse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(EditCondition="bHasRidgeCourse", UIMin="5", UIMax="50", ClampMin="1", Units="cm", ToolTip="Height of the ridge course above the roof surface, in cm."))
	double RidgeCourseHeight = 13.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(EditCondition="bHasRidgeCourse", UIMin="5", UIMax="50", ClampMin="1", Units="cm", ToolTip="Width of the ridge course, in cm."))
	double RidgeCourseWidth = 13.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge End Kick (蠍子尾)", EditCondition="bHasRidgeCourse", UIMin="0", UIMax="120", ClampMin="0", Units="cm", ToolTip="Rise of the ridge-end tail (蠍子尾) at each end, in cm."))
	double RidgeEndKick = 32.5;   // HutongCanon::Roof::TailRiseInCourses × the ridge course

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Depth", UIMin="0", UIMax="25", ClampMin="0", Units="cm", ToolTip="Depth of the fascia board along each eave, in cm."))
	double EaveFasciaDepth = 9.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Width", UIMin="4", UIMax="30", ClampMin="1", Units="cm", ToolTip="Width of the fascia board along each eave, in cm."))
	double EaveFasciaWidth = 13.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Section (椽頭)", UIMin="0", UIMax="18", ClampMin="0", Units="cm", ToolTip="Size of each rafter end (椽頭), in cm; 0 builds none."))
	double RafterEndSection = 7.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Courses (壟)", ToolTip="Model each course of tiles running down the roof, rather than leaving the texture to draw it."))
	bool bHasTileRuns = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Exposed Frame (徹上明造)", ToolTip="No ceiling: the roof is a shell on rafters carried by the roof frame (梁架: beams, posts and purlins) over the columns, open to view from inside. Close detail levels only; it adds triangles only an interior shows."))
	bool bExposedFrame = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Spacing", EditCondition="RafterEndSection > 0", UIMin="10", UIMax="50", ClampMin="4", Units="cm", ToolTip="Spacing between rafter ends along the eave, in cm."))
	double RafterEndSpacing = 22.0;

	// Set by the tool from the drag rect.
	double Width = 330.0;
	double Depth = 140.0;

	// Set by the compound when walk returns abut the cheeks on the same floor: the 臺基 stops flush,
	// else its side lips lie coplanar on the walk floor and z-fight.
	bool bFlushSides = false;

	double GetColumnRadius() const { return FMath::Max(0.5 * ColumnDiameter, 4.0); }

	bool IsHallAndRoll() const { return Style == EHutongInnerGateStyle::OneHallOneRoll; }

	// As many steps as the platform height needs (a gate lifted onto a walk floor kept two and rose 20 cm a riser).
	int32 GetStepCount() const
	{
		constexpr double MaxRiserCm = 17.0;
		const int32 Needed = FMath::CeilToInt32(FMath::Max(FloorHeight, 0.0) / MaxRiserCm) - 1;
		return FMath::Max(StepCount, Needed);
	}

	// Door line across the depth = its wall line: middle for the single-post form, front columns for the other.
	double GetWallLineY() const
	{
		const double D = FMath::Max(Depth, 1.0);
		if (!IsHallAndRoll()) return 0.5 * D;
		return FMath::Clamp(FMath::Max(CantileverLength, 20.0), GetColumnRadius(), 0.4 * D);
	}
	double GetRearColumnY() const { return FMath::Max(Depth, 1.0) - GetColumnRadius(); }
	// 天溝 where the front 殿 roof meets the rear 捲棚, both at the eave.
	double GetValleyY() const
	{
		return FMath::Clamp(HutongCanon::Gate::InnerGateFrontRoofShare * GetRearColumnY(),
			GetWallLineY() + 20.0, GetRearColumnY() - 20.0);
	}
	double GetColumnHeight() const { return FMath::Max(GetEaveHeight() - FloorHeight, 1.0); }

	// Walked through upright: standing head height over the threshold, not a low room's crouch.
	double GetDoorHeadHeight() const
	{
		return FMath::Max(DoorHeadHeight, FloorHeight + FMath::Clamp(ThresholdHeight, 0.0, 40.0)
			+ HutongCanon::Openings::WalkerHeightCm + 10.0);
	}

	// The 擔梁 rides above the head; the eave clears the whole stack.
	double GetMinEaveHeight() const
	{
		const double PT = FMath::Max(2.0 * GetColumnRadius() * 0.8, 8.0);
		// At least the tool's 45 cm eave margin, or that clamp pulls the head back through this clearance.
		return GetDoorHeadHeight()
			+ FMath::Max(FMath::Max(DoorFrameThickness, 2.0) + 1.2 * PT, 45.0);
	}

	// 80 = the generator's former private floor, now visible here.
	double GetEaveHeight() const { return FMath::Max3(EaveHeight, GetMinEaveHeight(), 80.0); }
	// Roof above the column tops, the ceiling at the column line, and the roof's base
	// (HutongGen::Proportions::RoofLift).
	// The eave step's 舉 of the roof as built: the section scaled to the fixed rise.
	double GetEaveJu() const { return HutongGen::Jiajia::BuiltEaveJu(HutongGen::Jiajia::MakeSection(EHutongPurlins::Three, 0.5 * Depth, FMath::Max(RoofOverhang, 0.0), RoofApexRoll), GetRoofRise()); }
	double GetRoofLift() const { return HutongGen::Proportions::RoofLift(FMath::Max(ColumnDiameter, 2.0), FMath::Max(RoofOverhang, 0.0), GetEaveJu()); }
	double GetUndersideRise() const { return HutongGen::Proportions::UndersideRise(FMath::Max(ColumnDiameter, 2.0), FMath::Max(RoofOverhang, 0.0), GetEaveJu()); }
	double GetRoofBaseHeight() const { return GetEaveHeight() + GetRoofLift(); }
	double GetRoofRise() const { return FMath::Max(RoofRise, 10.0); }

	// Size band from the canon (same as the gate house); all zero when off.
	using FSizeRange = HutongCanon::Gate::FSizeBand;

	// UNVERIFIED like the gate house's bands: widen rather than trust the centimetres.
	FSizeRange GetSizeRange() const
	{
		FSizeRange R;
		if (!bConstrainToHistoricalSize)
		{
			return R;
		}
		// Depth: the 擔梁's full run with columns halfway, or for the grand form the hanging-post reach
		// plus the run back to the rear columns.
		R = IsHallAndRoll() ? HutongCanon::Gate::InnerGateGrandSize : HutongCanon::Gate::InnerGateSize;
		return R;
	}
};

namespace HutongGen
{
	void BuildInnerGate(UE::Geometry::FDynamicMesh3& Mesh, const FHutongInnerGateParams& P);
}
