#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "Generation/HutongProportions.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRoofTile.h"
#include "Generation/HutongDoorStone.h"
#include "Generation/HutongDoor.h"
#include "GateHouseGenerator.generated.h"

// The four ordinary courtyard gates, in descending order of status.
UENUM()
enum class EHutongGateStyle : uint8
{
	Guangliang UMETA(DisplayName = "Wide-Hall Gate (廣亮大門)", ToolTip="Door plane on the centre column (中柱) line."),

	Jinzhu UMETA(DisplayName = "Inner-Column Gate (金柱大門)", ToolTip="Door plane on the front inner column (前金柱) line."),

	Manzi UMETA(DisplayName = "Flush Gate (蠻子門)", ToolTip="Door plane flush with the eave column (檐柱) line, with no recess."),

	Ruyi UMETA(DisplayName = "Ruyi Gate (如意門)", ToolTip="Door plane at the eave column (檐柱) line, under a hood."),
};

USTRUCT(BlueprintType)
struct FHutongGateHouseParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(HutongBasic, ToolTip="Which gate style to build."))
	EHutongGateStyle Style = EHutongGateStyle::Ruyi;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Constrain To Historical Size", ToolTip="Keeps the footprint and eave height within the style's size range."))
	bool bConstrainToHistoricalSize = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(UIMin="200", UIMax="600", ClampMin="80", Units="cm", ToolTip="Height of the eave above the ground, in cm."))
	double EaveHeight = 330.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(UIMin="10", UIMax="80", ClampMin="5", Units="cm", ToolTip="Thickness of the side walls, in cm."))
	double WallThickness = 34.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(UIMin="0", UIMax="90", ClampMin="0", Units="cm", ToolTip="Height of the platform (臺基) above the ground, in cm."))
	double FloorHeight = 45.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Platform Front Overhang", UIMin="0", UIMax="120", Units="cm", ToolTip="How far the platform projects in front of the facade, in cm."))
	double PlatformOverhang = 40.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(UIMin="0", UIMax="5", ClampMin="0", ClampMax="8", ToolTip="Number of steps (踏跺) up to the platform on each face."))
	int32 StepCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(UIMin="15", UIMax="80", ClampMin="5", Units="cm", ToolTip="Depth of each step tread, in cm."))
	double StepTread = 32.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Column Height in Diameters", UIMin="8", UIMax="14", ClampMin="4", ClampMax="30", ToolTip="Column height in column diameters."))
	double ColumnHeightInDiameters = HutongCanon::Module::ColumnHeightInDiameters;


	// --- Doorway ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(EditCondition="Style != EHutongGateStyle::Ruyi", UIMin="0.25", UIMax="0.9", ClampMin="0.1", ClampMax="1", ToolTip="Doorway clear width as a fraction of the bay (pier span on a Ruyi Gate (如意門))."))
	double DoorWidthFraction = 0.45;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(UIMin="180", UIMax="320", ClampMin="80", Units="cm", ToolTip="Height of the underside of the door head, in cm."))
	double DoorHeadHeight = 250.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(UIMin="4", UIMax="25", ClampMin="2", Units="cm", ToolTip="Thickness of the door jambs and head, in cm."))
	double DoorFrameThickness = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(UIMin="0", UIMax="45", ClampMin="0", Units="cm", ToolTip="Height of the threshold (門檻) above the floor, in cm."))
	double ThresholdHeight = 20.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(HutongBasic, DisplayName="Leaves Ajar", ToolTip="Builds the door leaves swung ajar instead of shut."))
	bool bLeavesOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(DisplayName="Ajar Angle Min", EditCondition="bLeavesOpen", UIMin="40", UIMax="140", ClampMin="10", ClampMax="170", Units="deg", ToolTip="Smallest inward swing angle a leaf can take, in degrees."))
	double AjarAngleMin = 78.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(DisplayName="Ajar Angle Max", EditCondition="bLeavesOpen", UIMin="40", UIMax="140", ClampMin="10", ClampMax="170", Units="deg", ToolTip="Largest inward swing angle a leaf can take, in degrees."))
	double AjarAngleMax = 100.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(DisplayName="Min Clear Width", EditCondition="bLeavesOpen", UIMin="0", UIMax="150", ClampMin="0", Units="cm", ToolTip="Minimum clear opening left between the ajar leaves, in cm."))
	double MinClearWidth = 90.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(DisplayName="Variation Seed", EditCondition="bLeavesOpen", ToolTip="Seed for the per-gate variation in leaf angles."))
	int32 RandomSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(DisplayName="Door Pegs (門簪)", UIMin="0", UIMax="4", ClampMin="0", ClampMax="6", ToolTip="Number of door pegs (門簪) projecting above the door head."))
	int32 DoorPegCount = 4;

	// 如意門 is held to two 門簪 however it is set.
	int32 GetDoorPegCount() const
	{
		return HasBrickScreen() ? FMath::Min(DoorPegCount, HutongCanon::Gate::RuyiDoorPegs) : DoorPegCount;
	}

	// --- 如意門 only ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(DisplayName="Door Head Projection (門頭)", EditCondition="Style == EHutongGateStyle::Ruyi", UIMin="0", UIMax="30", ClampMin="0", Units="cm", ToolTip="Projection of the brick door head (門頭) frieze from the wall, in cm; 0 omits it."))
	double DoorHeadProjection = 18.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(HutongBasic, DisplayName="Carved Door Head (雕花門頭)", EditCondition="Style == EHutongGateStyle::Ruyi", ToolTip="Carves the brick door head (門頭); off leaves it plain (素活)."))
	bool bCarvedDoorHead = true;

	// --- Shell ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Base Course Height", UIMin="0", UIMax="200", Units="cm", ToolTip="Height of the base course (下鹼) above the floor, in cm; 0 for automatic."))
	double BaseCourseHeight = 0.0;

	double GetBaseCourseHeight() const
	{
		return BaseCourseHeight > 0.0 ? BaseCourseHeight : FMath::Max(HutongCanon::BaseCourse::TopCm - FMath::Max(FloorHeight, 0.0), 25.0);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Base Course Projection", UIMin="0", UIMax="15", Units="cm", ToolTip="How far the base course stands proud of the wall face, in cm."))
	double BaseCourseProjection = 3.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(HutongBasic, DisplayName="Has Gable Pier (墀頭)", ToolTip="Builds a gable pier (墀頭) at the front corner of each side wall."))
	bool bHasChitou = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Chitou Projection", EditCondition="bHasChitou", UIMin="0", UIMax="60", Units="cm", ToolTip="How far each gable pier (墀頭) projects forward of the wall, in cm."))
	double ChitouProjection = 14.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Chitou Corbel Courses", EditCondition="bHasChitou", UIMin="0", UIMax="6", ClampMin="0", ClampMax="10", ToolTip="Number of corbel courses at the top of each gable pier (墀頭)."))
	int32 ChitouCorbelSteps = 3;

	// --- Roof ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Front Roof Overhang", UIMin="0", UIMax="200", Units="cm", ToolTip="How far the front eave projects past the wall, in cm; 0 for automatic."))
	double RoofOverhang = 0.0;

	// No rear overhang: the gate's rear eave faces the court and is built as a front eave.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Rise", UIMin="0", UIMax="300", ClampMin="0", Units="cm", ToolTip="Height of the ridge above the eave, in cm; 0 for automatic."))
	double RoofRise = 0.0;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Apex Roll", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="Rounding of the roof apex into a rolled ridge (捲棚); 0 keeps the fold."))
	double RoofApexRoll = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Has Ridge Course (正脊)", ToolTip="Builds a main ridge (正脊) course along the roof apex."))
	bool bHasRidgeCourse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(EditCondition="bHasRidgeCourse", UIMin="5", UIMax="60", Units="cm", ToolTip="Height of the ridge course, in cm."))
	double RidgeCourseHeight = 18.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(EditCondition="bHasRidgeCourse", UIMin="5", UIMax="60", Units="cm", ToolTip="Width of the ridge course, in cm."))
	double RidgeCourseWidth = 16.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge End Kick (蠍子尾)", EditCondition="bHasRidgeCourse", UIMin="0", UIMax="120", Units="cm", ToolTip="Rise of the ridge-end tail (蠍子尾) at each end of the ridge, in cm."))
	double RidgeEndKick = 45.0;   // HutongCanon::Roof::TailRiseInCourses × the ridge course

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Depth", UIMin="0", UIMax="30", Units="cm", ToolTip="Vertical depth of the fascia band along the eave, in cm."))
	double EaveFasciaDepth = 9.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Width", UIMin="4", UIMax="40", Units="cm", ToolTip="Horizontal width of the fascia band along the eave, in cm."))
	double EaveFasciaWidth = 13.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Row Spacing (壟)", UIMin="12", UIMax="60", ClampMin="6", Units="cm", ToolTip="Spacing of tile rows (壟) across the roof, in cm."))
	double TileRowSpacing = HutongGen::RoofTile::DefaultRowSpacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Section (椽頭)", UIMin="0", UIMax="20", ClampMin="0", Units="cm", ToolTip="Section size of the rafter ends (椽頭), in cm; 0 omits them."))
	double RafterEndSection = 7.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Courses (壟)", ToolTip="Models each tile course down the roof instead of drawing it in the texture."))
	bool bHasTileRuns = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Exposed Frame (徹上明造)", ToolTip="No ceiling: shows the roof frame (梁架) from inside; close detail levels only."))
	// On: a gate passage is open to its rafters (圖5-1-4.2 section).
	bool bExposedFrame = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(HutongBasic, DisplayName="Splayed Screen Walls (反八字影壁)", EditCondition="Style == EHutongGateStyle::Guangliang", EditConditionHides, ToolTip="Builds splayed screen walls (反八字影壁, 撇山影壁) before the gate, outside the footprint."))
	bool bSplayedScreens = false;

	bool HasSplayedScreens() const { return bSplayedScreens && Style == EHutongGateStyle::Guangliang; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Spacing", EditCondition="RafterEndSection > 0", UIMin="10", UIMax="60", ClampMin="5", Units="cm", ToolTip="Spacing between rafter ends along the eave, in cm."))
	double RafterEndSpacing = 22.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Column Taper (收分)", UIMin="0", UIMax="0.02", ClampMin="0", ClampMax="0.05", ToolTip="Column taper (收分) as a fraction of the column height."))
	double ColumnTaperRatio = HutongCanon::Module::ColumnTaperRatio;

	// Set by the tool from the drag rect.

	// Set by detail level: 筒瓦 here states rank; never a checkbox.
	bool bPlainTileForDetail = false;

	double Width = 400.0;
	double Depth = 400.0;

	double GetColumnHeight() const { return FMath::Max(GetEaveHeight() - FloorHeight, 1.0); }

	// Vertical counterpart of Min Clear Width.
	double GetDoorHeadHeight() const
	{
		return FMath::Max(DoorHeadHeight, HutongGen::Passage::MinHeadZ(FloorHeight, ThresholdHeight));
	}

	// Both generator and tool keep the door head this far below the eave for the 額枋.
	double GetMinEaveHeight() const { return GetDoorHeadHeight() + 20.0; }

	double GetEaveHeight() const { return FMath::Max(EaveHeight, GetMinEaveHeight()); }
	// Roof above the column tops, the ceiling at the column line, and the roof's base
	// (HutongGen::Proportions::RoofLift).
	double GetRoofLift() const { return HutongGen::Proportions::RoofLift(GetColumnDiameter(), GetRoofOverhang(), HutongGen::Jiajia::EaveJu(GetPurlins())); }
	double GetUndersideRise() const { return HutongGen::Proportions::UndersideRise(GetColumnDiameter(), GetRoofOverhang(), HutongGen::Jiajia::EaveJu(GetPurlins())); }
	double GetRoofBaseHeight() const { return GetEaveHeight() + GetRoofLift(); }

	// 柱徑, same module as the siheyuan.
	double GetColumnDiameter() const
	{
		return HutongGen::Proportions::ColumnDiameter(GetColumnHeight(), ColumnHeightInDiameters);
	}

	// Pier projection: at least covers the corner column.
	double GetChitouProjection() const
	{
		return HutongGen::Proportions::ChitouProjection(ChitouProjection, 0.5 * GetColumnDiameter());
	}

	// 上檐出 over the street by rank: house rule on 柱高, cut by gate rank. A 大門 in a 封護檐 street
	// face that projects nothing, so this is all that comes forward.
	double GetRoofOverhang() const
	{
		if (RoofOverhang > 0.0) return RoofOverhang;
		const double Share =
			(Style == EHutongGateStyle::Guangliang) ? HutongCanon::Gate::GuangliangEaveShare :
			(Style == EHutongGateStyle::Jinzhu)     ? HutongCanon::Gate::JinzhuEaveShare :
			(Style == EHutongGateStyle::Manzi)      ? HutongCanon::Gate::ManziEaveShare :
			                                          HutongCanon::Gate::RuyiEaveShare;
		return Share * HutongGen::Proportions::EaveOverhang(
			GetColumnHeight(), HutongCanon::Module::EaveOverhangRatio);
	}

	// Door plane setback from the 檐柱 line.
	double GetDoorPlaneFraction() const
	{
		switch (Style)
		{
		case EHutongGateStyle::Guangliang: return HutongCanon::Gate::GuangliangDoorPlane;
		case EHutongGateStyle::Jinzhu:     return HutongCanon::Gate::JinzhuDoorPlane;
		default:                           return HutongCanon::Gate::FlushDoorPlane;
		}
	}

	// 如意門 fills its bay with brick round a narrow doorway; the others open between columns.
	bool HasBrickScreen() const { return Style == EHutongGateStyle::Ruyi; }

	// 五檁中柱式 with the frame showing on the passage walls: 蠻子門 (圖5-1-4.2), 廣亮大門 (圖5-1-1 and its text,
	// the door on the 中柱).
	bool HasCentreColumn() const { return Style == EHutongGateStyle::Manzi || Style == EHutongGateStyle::Guangliang; }

	// 鑽金柱 frame, the door on the 前金柱 (圖5-1-3).
	bool HasFrontVeranda() const { return Style == EHutongGateStyle::Jinzhu; }

	// Columns proud of the passage walls, the cross frame and 廊心 between them showing.
	bool HasWallFrame() const { return HasCentreColumn() || HasFrontVeranda(); }

	// The structural fact the size band follows.
	EHutongPurlins GetPurlins() const
	{
		return (Style == EHutongGateStyle::Ruyi) ? EHutongPurlins::Three : EHutongPurlins::Five;
	}

	// Built rise: the 舉架 section's over this depth unless RoofRise is set. Single source for generator,
	// massing block and ridge estimate (estimate reading the section while the roof read the field put
	// a gate's ridge below its row's).
	double GetRoofRise(double OverDepth) const
	{
		if (RoofRise > 0.0) return RoofRise;
		const HutongGen::FHutongRoofSection S = HutongGen::Jiajia::MakeSection(
			GetPurlins(), 0.5 * FMath::Max(OverDepth, 1.0), GetRoofOverhang(), RoofApexRoll);
		return FMath::Max(S.Rise(), 1.0);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(HutongBasic, DisplayName="Door Stones (門墩)", ToolTip="Settings for the door stones (門墩) at the foot of the door jambs."))
	FHutongDoorStoneParams DoorStones;

	// 筒瓦 for the two gates announcing an official, 合瓦 for the other two.
	EHutongRoofTile GetRoofTile() const
	{
		if (bPlainTileForDetail) return EHutongRoofTile::He;
		return (Style == EHutongGateStyle::Guangliang || Style == EHutongGateStyle::Jinzhu)
			? EHutongRoofTile::Tong : EHutongRoofTile::He;
	}

	// 門墩 as built: any gate takes a drum or a block (the text beside 圖5-1-4: "或圓或方並無定式").
	FHutongDoorStoneParams GetDoorStones() const
	{
		FHutongDoorStoneParams S = DoorStones;
		if (Style == EHutongGateStyle::Manzi)
		{
			S.BlockHeight = FMath::Min(S.BlockHeight, HutongCanon::Gate::ManziDoorStoneCm);
		}
		return S;
	}

	// Size band for a style: 面闊 along the facing side, 進深, eave band. All zero when off.
	using FSizeRange = HutongCanon::Gate::FSizeBand;

	// UNVERIFIED, looser than the door-plane fractions.
	FSizeRange GetSizeRange() const
	{
		FSizeRange R;
		if (!bConstrainToHistoricalSize)
		{
			return R;
		}

		switch (Style)
		{
		case EHutongGateStyle::Guangliang: R = HutongCanon::Gate::GuangliangSize; break;
		case EHutongGateStyle::Jinzhu:     R = HutongCanon::Gate::JinzhuSize;     break;
		case EHutongGateStyle::Manzi:      R = HutongCanon::Gate::ManziSize;      break;
		default:                           R = HutongCanon::Gate::RuyiSize;       break;
		}
		return R;
	}
};

namespace HutongGen
{
	void BuildGateHouse(UE::Geometry::FDynamicMesh3& Mesh, const FHutongGateHouseParams& P);
}
