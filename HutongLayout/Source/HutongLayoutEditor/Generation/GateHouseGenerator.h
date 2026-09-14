#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "Generation/HutongProportions.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRoofTile.h"
#include "Generation/HutongDoorStone.h"
#include "Generation/HutongDoor.h"
#include "Generation/HutongApron.h"
#include "GateHouseGenerator.generated.h"

// The four ordinary courtyard gates, in descending order of status.
UENUM()
enum class EHutongGateStyle : uint8
{
	Guangliang UMETA(DisplayName = "Wide-Hall Gate (廣亮大門)", ToolTip="Door plane on the centre column (中柱) line, giving the deepest recess in front of the door."),

	Jinzhu UMETA(DisplayName = "Inner-Column Gate (金柱大門)", ToolTip="Door plane on the front inner column (前金柱) line, giving a shallower recess in front of the door."),

	Manzi UMETA(DisplayName = "Flush Gate (蠻子門)", ToolTip="Door plane flush with the eave column (檐柱) line, with no recess."),

	Ruyi UMETA(DisplayName = "Ruyi Gate (如意門)", ToolTip="Door plane at the eave column (檐柱) line, the bay filled with brick and a narrow doorway under a hood."),
};

USTRUCT(BlueprintType)
struct FHutongGateHouseParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(ToolTip="Which of the four courtyard gate styles to build; sets where the door plane sits."))
	EHutongGateStyle Style = EHutongGateStyle::Ruyi;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Constrain To Historical Size", ToolTip="Clamps the footprint and eave height to the size band of the chosen style."))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Column Height in Diameters", UIMin="8", UIMax="14", ClampMin="4", ClampMax="30", ToolTip="Column height expressed in column diameters; sets the column diameter."))
	double ColumnHeightInDiameters = HutongCanon::Module::ColumnHeightInDiameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Apron Paving (散水)", ShowOnlyInnerProperties, ToolTip="Settings for the apron paving (散水) band round the foot of the gate."))
	FHutongApronParams Apron;

	// --- Doorway ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(UIMin="0.25", UIMax="0.9", ClampMin="0.1", ClampMax="1", ToolTip="Clear width of the doorway as a fraction of the bay width."))
	double DoorWidthFraction = 0.55;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(UIMin="180", UIMax="320", ClampMin="80", Units="cm", ToolTip="Height of the underside of the door head, in cm."))
	double DoorHeadHeight = 250.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(UIMin="4", UIMax="25", ClampMin="2", Units="cm", ToolTip="Thickness of the door jambs and head, in cm."))
	double DoorFrameThickness = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(UIMin="0", UIMax="45", ClampMin="0", Units="cm", ToolTip="Height of the threshold (門檻) above the floor, in cm."))
	double ThresholdHeight = 20.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(DisplayName="Leaves Ajar", ToolTip="Builds the door leaves swung ajar instead of shut."))
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
	int32 DoorPegCount = 2;

	// --- 如意門 only ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(DisplayName="Hood Courses (門頭)", EditCondition="Style == EHutongGateStyle::Ruyi", UIMin="0", UIMax="5", ClampMin="0", ClampMax="8", ToolTip="Number of corbelled brick courses in the door head (門頭) hood over a ruyi gate (如意門) doorway."))
	int32 HoodCourses = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(DisplayName="Hood Projection", EditCondition="Style == EHutongGateStyle::Ruyi", UIMin="0", UIMax="40", ClampMin="0", Units="cm", ToolTip="How far the door head (門頭) hood projects from the wall face, in cm."))
	double HoodProjection = 14.0;

	// --- Shell ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Base Course Height", UIMin="0", UIMax="200", Units="cm", ToolTip="Height of the base course (下鹼) above the floor, in cm. Zero derives it so the band tops out at the canon line above the ground, the same line every piece of a frontage shares."))
	double BaseCourseHeight = 0.0;

	double GetBaseCourseHeight() const
	{
		return BaseCourseHeight > 0.0 ? BaseCourseHeight : FMath::Max(HutongCanon::BaseCourse::TopCm - FMath::Max(FloorHeight, 0.0), 25.0);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Base Course Projection", UIMin="0", UIMax="15", Units="cm", ToolTip="How far the base course stands proud of the wall face, in cm."))
	double BaseCourseProjection = 3.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Has Gable Pier (墀頭)", ToolTip="Builds a gable pier (墀頭) at the front corner of each side wall."))
	bool bHasChitou = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Chitou Projection", EditCondition="bHasChitou", UIMin="0", UIMax="60", Units="cm", ToolTip="How far each gable pier (墀頭) projects forward of the wall, in cm."))
	double ChitouProjection = 14.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Chitou Corbel Courses", EditCondition="bHasChitou", UIMin="0", UIMax="6", ClampMin="0", ClampMax="10", ToolTip="Number of corbel courses stepping out at the top of each gable pier (墀頭)."))
	int32 ChitouCorbelSteps = 3;

	// --- Roof ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Front Roof Overhang", UIMin="0", UIMax="200", Units="cm", ToolTip="How far the front eave projects past the wall, in cm."))
	double RoofOverhang = 75.0;

	// No rear overhang of its own: a gate fronts the lane and opens onto the courtyard, so its rear eave is a front eave.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Rise", UIMin="0", UIMax="300", ClampMin="0", Units="cm", ToolTip="Height of the ridge above the eave, in cm; zero takes the rise the roof section (舉架) gives over the gate's depth."))
	double RoofRise = 0.0;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Apex Roll", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="How much the roof apex is rounded into a rolled ridge (捲棚) crown; 0 keeps a sharp fold."))
	double RoofApexRoll = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Has Ridge Course (正脊)", ToolTip="Builds a main ridge (正脊) course along the roof apex."))
	bool bHasRidgeCourse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(EditCondition="bHasRidgeCourse", UIMin="5", UIMax="60", Units="cm", ToolTip="Height of the ridge course, in cm."))
	double RidgeCourseHeight = 18.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(EditCondition="bHasRidgeCourse", UIMin="5", UIMax="60", Units="cm", ToolTip="Width of the ridge course, in cm."))
	double RidgeCourseWidth = 16.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge End Kick (蠍子尾)", EditCondition="bHasRidgeCourse", UIMin="0", UIMax="60", Units="cm", ToolTip="Rise of the ridge-end tail (蠍子尾) at each end of the ridge, in cm."))
	double RidgeEndKick = 26.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Depth", UIMin="0", UIMax="30", Units="cm", ToolTip="Vertical depth of the fascia band along the eave, in cm."))
	double EaveFasciaDepth = 9.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Width", UIMin="4", UIMax="40", Units="cm", ToolTip="Horizontal width of the fascia band along the eave, in cm."))
	double EaveFasciaWidth = 13.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Row Spacing (壟)", UIMin="12", UIMax="60", ClampMin="6", Units="cm", ToolTip="Spacing of tile rows (壟) across the roof, in cm; sets the eave cap (勾頭) pitch and tile UV scale."))
	double TileRowSpacing = HutongGen::RoofTile::DefaultRowSpacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Section (椽頭)", UIMin="0", UIMax="20", ClampMin="0", Units="cm", ToolTip="Cross-section size of the rafter ends (椽頭) under the eave, in cm; 0 omits them."))
	double RafterEndSection = 7.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Spacing", EditCondition="RafterEndSection > 0", UIMin="10", UIMax="60", ClampMin="5", Units="cm", ToolTip="Spacing between rafter ends along the eave, in cm."))
	double RafterEndSpacing = 22.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Structure", meta=(DisplayName="Column Taper (收分)", UIMin="0", UIMax="0.02", ClampMin="0", ClampMax="0.05", ToolTip="Column taper (收分) as the fraction of column height lost from the diameter at the head."))
	double ColumnTaperRatio = HutongCanon::Module::ColumnTaperRatio;

	// Set by the tool from the drag rect; not user-editable.

	// Set by the detail level, not by anybody's hand — 筒瓦 here is a *rank* statement and must not become a checkbox.
	bool bPlainTileForDetail = false;

	double Width = 400.0;
	double Depth = 400.0;

	double GetColumnHeight() const { return FMath::Max(GetEaveHeight() - FloorHeight, 1.0); }

	// Min Clear Width's argument, vertically.
	double GetDoorHeadHeight() const
	{
		return FMath::Max(DoorHeadHeight, HutongGen::Passage::MinHeadZ(FloorHeight, ThresholdHeight));
	}

	// The 額枋 spans head-to-eave, and both the generator and the tool keep the head clear of the eave by their own margin.
	double GetMinEaveHeight() const { return GetDoorHeadHeight() + 20.0; }

	double GetEaveHeight() const { return FMath::Max(EaveHeight, GetMinEaveHeight()); }

	// 柱徑, from the same module the siheyuan uses.
	double GetColumnDiameter() const
	{
		return HutongGen::Proportions::ColumnDiameter(GetColumnHeight(), ColumnHeightInDiameters);
	}

	// The projection the pier is built with: never less than covers the corner column.
	double GetChitouProjection() const
	{
		return HutongGen::Proportions::ChitouProjection(ChitouProjection, 0.5 * GetColumnDiameter());
	}

	// How far back from the 檐柱 line the door plane sits.
	double GetDoorPlaneFraction() const
	{
		switch (Style)
		{
		case EHutongGateStyle::Guangliang: return HutongCanon::Gate::GuangliangDoorPlane;
		case EHutongGateStyle::Jinzhu:     return HutongCanon::Gate::JinzhuDoorPlane;
		default:                           return HutongCanon::Gate::FlushDoorPlane;
		}
	}

	// 如意門 fills its bay with brick and leaves only a narrow doorway; the others are open between the columns.
	bool HasBrickScreen() const { return Style == EHutongGateStyle::Ruyi; }

	// The structural fact the size band follows from.
	EHutongPurlins GetPurlins() const
	{
		return (Style == EHutongGateStyle::Ruyi) ? EHutongPurlins::Three : EHutongPurlins::Five;
	}

	// The rise the roof is built with: the 舉架 section's own over this depth unless RoofRise
	// names one. The single source for the generator, the massing block and the ridge estimate
	// a gate is lifted above a row by — the estimate reading the section while the roof read the
	// field is how a gate came out with its ridge below the row's.
	double GetRoofRise(double OverDepth) const
	{
		if (RoofRise > 0.0) return RoofRise;
		const HutongGen::FHutongRoofSection S = HutongGen::Jiajia::MakeSection(
			GetPurlins(), 0.5 * FMath::Max(OverDepth, 1.0), FMath::Max(RoofOverhang, 0.0), RoofApexRoll);
		return FMath::Max(S.Rise(), 1.0);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doorway", meta=(DisplayName="Door Stones (門墩)", ToolTip="Settings for the door stones (門墩) at the foot of the door jambs."))
	FHutongDoorStoneParams DoorStones;

	// 筒瓦 for the two gates that announce an official, 合瓦 for the two that do not.
	EHutongRoofTile GetRoofTile() const
	{
		if (bPlainTileForDetail) return EHutongRoofTile::He;
		return (Style == EHutongGateStyle::Guangliang || Style == EHutongGateStyle::Jinzhu)
			? EHutongRoofTile::Tong : EHutongRoofTile::He;
	}

	// The 門墩 this gate is entitled to.
	FHutongDoorStoneParams GetDoorStones() const
	{
		FHutongDoorStoneParams S = DoorStones;
		if (Style != EHutongGateStyle::Guangliang && Style != EHutongGateStyle::Jinzhu)
		{
			S.Style = EHutongDoorStone::Block;
		}
		return S;
	}

	// The size band a style occurs at. All zero when the constraint is off.
	// 面闊 along the facing side, 進深, and the eave band. The canon's own struct.
	using FSizeRange = HutongCanon::Gate::FSizeBand;

	// UNVERIFIED, more loosely than the door-plane fractions.
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
