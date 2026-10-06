#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "Generation/HutongProportions.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRoofTile.h"
#include "Generation/HutongBays.h"
#include "Generation/HutongRearEave.h"
#include "ShopfrontGenerator.generated.h"

// What stands in front of the shop: the street face that tells one shop from the next.
UENUM(BlueprintType)
enum class EHutongShopFront : uint8
{
	Plain   UMETA(DisplayName = "Plain Board Front", ToolTip = "Board doors (排板門) under a plain header, the name plaque on it."),
	Carved  UMETA(DisplayName = "Carved Fascia Front", ToolTip = "A deep painted hanging board (掛檐板) with fretwork brackets (花牙子) under the eave."),
	Platform UMETA(DisplayName = "Platform Front (拍子式)", ToolTip = "A flat canopy on posts across the front, an upward railing (朝天欄杆) along its edge."),
	Shed    UMETA(DisplayName = "Lattice Shed Front (涼棚)", ToolTip = "A light frame of thin posts with an open lattice top shading the street in front of the shop."),
	// A 牌樓式 front (沖天柱 rising past the eave) was removed 2026-10-05: neither view of the 萬壽圖 shows
	// one on a shop. Re-add only once a source of the period confirms it.
};

// The tall 招牌 a shop puts up for the street.
UENUM(BlueprintType)
enum class EHutongUprightSign : uint8
{
	None        UMETA(DisplayName = "None", ToolTip = "No upright signboard."),
	Freestanding UMETA(DisplayName = "Freestanding (沖天招牌)", ToolTip = "One tall board on its own post at the street edge, about as high as the eave, beside the way in."),
	OnPosts     UMETA(DisplayName = "On the Posts", ToolTip = "A pair of boards hung on the posts either side of the open bays."),
};

// Where the 櫃檯 stands in the open bays. Each leaves a way in at least a walker wide.
UENUM(BlueprintType)
enum class EHutongShopCounter : uint8
{
	None    UMETA(DisplayName = "None", ToolTip = "No counter: the open bays are clear."),
	Inside  UMETA(DisplayName = "Inside, L-Shaped (曲尺櫃台)", ToolTip = "Set back inside the shop: one arm parallel to the front, one running back, the customers' floor in front of it."),
	Street  UMETA(DisplayName = "At the Street", ToolTip = "Across the open bays at the front, served over from the street, with a gap at one end to walk in."),
};

// Paint of a shop's woodwork. Colours land where the building's palette keeps its default, so a colour
// set by hand still wins.
UENUM(BlueprintType)
enum class EHutongShopScheme : uint8
{
	Auto       UMETA(DisplayName = "Auto (varies by building)", ToolTip = "One of the schemes below, picked from the building's id, so a street of shops varies."),
	Vermilion  UMETA(DisplayName = "Vermilion (朱紅)", ToolTip = "Red columns and boards, blue-green painted fascia, black plaque."),
	Green      UMETA(DisplayName = "Green (綠)", ToolTip = "Green columns and boards, red painted fascia, blue plaque."),
	BlackGold  UMETA(DisplayName = "Black and Gold (黑金)", ToolTip = "Black lacquer with gilded fascia and brackets."),
	BlueGreen  UMETA(DisplayName = "Blue-Green Painted (青綠)", ToolTip = "Red columns, oiled boards, a blue fascia."),
	Natural    UMETA(DisplayName = "Oiled Timber (本色)", ToolTip = "Brown oiled timber throughout."),
};

// 鋪面房: a house's 硬山 shell with an open shop facade.
USTRUCT(BlueprintType)
struct FHutongShopfrontParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(HutongBasic, DisplayName="Front", ToolTip="What stands across the front of the shop."))
	EHutongShopFront Front = EHutongShopFront::Carved;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(HutongBasic, DisplayName="Colour Scheme", ToolTip="Paint of the shop's woodwork; Auto varies it from building to building."))
	EHutongShopScheme Scheme = EHutongShopScheme::Auto;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shop", meta=(UIMin="260", UIMax="450", ClampMin="120", Units="cm", ToolTip="Height of the eave above the ground, in cm."))
	double EaveHeight = 330.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shop", meta=(UIMin="15", UIMax="60", ClampMin="8", Units="cm", ToolTip="Thickness of the masonry walls, in cm."))
	double WallThickness = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shop", meta=(DisplayName="Floor Height (臺基)", UIMin="0", UIMax="45", ClampMin="0", Units="cm", ToolTip="Height of the platform (臺基) above the ground, in cm."))
	double FloorHeight = 16.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shop", meta=(DisplayName="Platform Overhang", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="How far the platform projects in front of the facade, in cm."))
	double PlatformOverhang = 22.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(UIMin="180", UIMax="360", ClampMin="80", Units="cm", ToolTip="Narrowest bay width allowed when dividing the frontage into bays, in cm."))
	double MinBayWidth = 220.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(UIMin="200", UIMax="420", ClampMin="80", Units="cm", ToolTip="Widest bay width allowed when dividing the frontage into bays, in cm."))
	double MaxBayWidth = 300.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(DisplayName="Side Bay / Central Bay", UIMin="0.8", UIMax="1.0", ClampMin="0.4", ClampMax="1", ToolTip="Width of each side bay as a fraction of the central bay's width."))
	double SideBayWidthRatio = 0.95;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(DisplayName="Column Diameter", UIMin="15", UIMax="45", ClampMin="6", Units="cm", ToolTip="Diameter of each column at its base, in cm."))
	double ColumnDiameter = 25.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(DisplayName="Column Taper (收分)", UIMin="0", UIMax="0.02", ClampMin="0", ClampMax="0.05", ToolTip="Taper of each column toward its top, as a fraction of its height."))
	double ColumnTaperRatio = HutongCanon::Module::ColumnTaperRatio;

	// --- 排板門 ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(DisplayName="Opening Head / Eave", UIMin="0.6", UIMax="0.92", ClampMin="0.3", ClampMax="0.95", ToolTip="Height of the shopfront opening's head as a fraction of the eave height."))
	double OpeningTopRatio = 0.8;

	// As the 萬壽圖: by day a shop's boards are down the whole front.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(HutongBasic, DisplayName="All Bays Open", ToolTip="Opens the whole front; off, Open Bays says how many."))
	bool bAllBaysOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(DisplayName="Open Bays", EditCondition="!bAllBaysOpen", UIMin="0", UIMax="5", ClampMin="0", ClampMax="12", ToolTip="Number of bays left open, counted outward from the middle; zero boards the shop up."))
	int32 OpenBayCount = 1;

	int32 GetOpenBayCount(int32 BayCount) const { return bAllBaysOpen ? BayCount : FMath::Clamp(OpenBayCount, 0, BayCount); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(DisplayName="Board Width", UIMin="15", UIMax="45", ClampMin="8", Units="cm", ToolTip="Width of each board of the board doors (排板門), in cm."))
	double BoardWidth = 26.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(DisplayName="Board Thickness", UIMin="3", UIMax="12", ClampMin="1", Units="cm", ToolTip="Thickness of each board of the board doors (排板門), in cm."))
	double BoardThickness = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(DisplayName="Counter (櫃檯)", ToolTip="Where the shop counter (櫃檯) stands in the open bays; every choice leaves a way in."))
	EHutongShopCounter Counter = EHutongShopCounter::Street;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(DisplayName="Counter Height", EditCondition="Counter != EHutongShopCounter::None", UIMin="70", UIMax="110", ClampMin="30", Units="cm", ToolTip="Height of the counter above the floor, in cm."))
	double CounterHeight = 88.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(DisplayName="Counter Depth", EditCondition="Counter != EHutongShopCounter::None", UIMin="30", UIMax="80", ClampMin="10", Units="cm", ToolTip="Depth of the counter from front to back, in cm."))
	double CounterDepth = 52.0;

	// --- 掛檐板 and 匾額 ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fascia", meta=(DisplayName="Has Hanging Board (掛檐板)", ToolTip="Adds an eave board (掛檐板) under the eave."))
	bool bHasHangingBoard = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fascia", meta=(DisplayName="Board Drop", EditCondition="bHasHangingBoard", UIMin="20", UIMax="80", ClampMin="8", Units="cm", ToolTip="How far the hanging board drops below the eave, in cm."))
	double HangingBoardDrop = 40.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fascia", meta=(DisplayName="Board Projection", EditCondition="bHasHangingBoard", UIMin="2", UIMax="20", ClampMin="1", Units="cm", ToolTip="How far the hanging board stands out from the facade, in cm."))
	double HangingBoardProjection = 8.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fascia", meta=(DisplayName="Has Spandrels (花牙子)", EditCondition="bHasHangingBoard", ToolTip="Adds fretwork brackets (花牙子) in the upper corners of each bay."))
	bool bHasSpandrels = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fascia", meta=(DisplayName="Spandrel Reach", EditCondition="bHasHangingBoard && bHasSpandrels", UIMin="15", UIMax="70", ClampMin="5", Units="cm", ToolTip="How far each spandrel extends from its column into the bay, in cm."))
	double SpandrelReach = 34.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fascia", meta=(HutongBasic, DisplayName="Has Signboard (匾額)", ToolTip="Adds a name plaque (匾額) as a signboard over the open bay."))
	bool bHasSignboard = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fascia", meta=(DisplayName="Signboard Height", EditCondition="bHasSignboard", UIMin="30", UIMax="90", ClampMin="10", Units="cm", ToolTip="Height of the signboard, in cm."))
	double SignboardHeight = 52.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Signs", meta=(DisplayName="Upright Signboard (招牌)", ToolTip="The tall signboard put up for the street."))
	EHutongUprightSign UprightSign = EHutongUprightSign::Freestanding;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Signs", meta=(DisplayName="Upright Signboard Height", EditCondition="UprightSign != EHutongUprightSign::None", UIMin="80", UIMax="300", ClampMin="30", Units="cm", ToolTip="Height of each upright signboard, in cm."))
	double UprightSignHeight = 210.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Signs", meta=(DisplayName="Upright Signboard Width", EditCondition="UprightSign != EHutongUprightSign::None", UIMin="20", UIMax="80", ClampMin="10", Units="cm", ToolTip="Width of each upright signboard, in cm."))
	double UprightSignWidth = 40.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Signs", meta=(DisplayName="Hanging Trade Sign (幌子)", ToolTip="Hangs a trade sign from an arm at one end of the front, turned to face along the street."))
	bool bHasTradeSign = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Signs", meta=(DisplayName="Trade Sign Width", EditCondition="bHasTradeSign", UIMin="20", UIMax="80", ClampMin="10", Units="cm", ToolTip="Width of the hanging trade sign, in cm."))
	double TradeSignWidth = 40.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Signs", meta=(DisplayName="Trade Sign Height", EditCondition="bHasTradeSign", UIMin="40", UIMax="160", ClampMin="15", Units="cm", ToolTip="Height of the hanging trade sign, in cm."))
	double TradeSignHeight = 90.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Signs", meta=(DisplayName="Trade Sign Reach", EditCondition="bHasTradeSign", UIMin="50", UIMax="200", ClampMin="20", Units="cm", ToolTip="How far the trade sign's arm reaches out from the front, in cm."))
	double TradeSignReach = 95.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Front Details", meta=(DisplayName="Canopy Depth (拍子)", EditCondition="Front == EHutongShopFront::Platform", UIMin="80", UIMax="250", ClampMin="40", Units="cm", ToolTip="How far the platform canopy reaches in front of the columns, in cm."))
	double CanopyDepth = 140.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Front Details", meta=(DisplayName="Shed Depth (涼棚)", EditCondition="Front == EHutongShopFront::Shed", UIMin="100", UIMax="400", ClampMin="40", Units="cm", ToolTip="How far the lattice shed reaches in front of the columns, in cm."))
	double ShedDepth = 220.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Front Details", meta=(DisplayName="Eave Lanterns", ToolTip="Hangs a round lantern over each open bay, under the eave or the canopy."))
	bool bHasLanterns = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Front Details", meta=(DisplayName="Raised Platform (高臺基)", ToolTip="Stands the shop on a high stone platform with broad steps up to the open bays."))
	bool bRaisedPlatform = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Front Details", meta=(DisplayName="Raised Platform Height", EditCondition="bRaisedPlatform", UIMin="30", UIMax="90", ClampMin="20", Units="cm", ToolTip="Height of the raised platform above the ground, in cm."))
	double RaisedPlatformHeight = 60.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Front Details", meta=(DisplayName="Fence (柵欄)", ToolTip="Puts a low picket fence along the front of the shut bays."))
	bool bHasFence = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Front Details", meta=(DisplayName="Railing Height (朝天欄杆)", EditCondition="Front == EHutongShopFront::Platform", UIMin="30", UIMax="100", ClampMin="10", Units="cm", ToolTip="Height of the railing along the canopy's edge, in cm."))
	double CanopyRailHeight = 55.0;



	// --- Shell ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Base Course Height (下鹼)", UIMin="0", UIMax="150", ClampMin="0", Units="cm", ToolTip="Height of the base course (下鹼) above the floor, in cm; zero for automatic."))
	double BaseCourseHeight = 0.0;

	double GetBaseCourseHeight() const
	{
		return BaseCourseHeight > 0.0 ? BaseCourseHeight : FMath::Max(HutongCanon::BaseCourse::TopCm - GetFloorHeight(), 25.0);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Base Course Projection", UIMin="0", UIMax="15", ClampMin="0", Units="cm", ToolTip="How far the base course stands proud of the wall face, in cm."))
	double BaseCourseProjection = 3.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Has Gable Pier (墀頭)", ToolTip="Adds a gable pier (墀頭) at the front corner of each side wall."))
	bool bHasChitou = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Gable Pier Projection", EditCondition="bHasChitou", UIMin="0", UIMax="50", ClampMin="0", Units="cm", ToolTip="How far each gable pier (墀頭) projects forward of the facade, in cm."))
	double ChitouProjection = 15.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Gable Pier Corbel Courses", EditCondition="bHasChitou", UIMin="0", UIMax="6", ClampMin="0", ClampMax="10", ToolTip="Number of corbel courses at the top of each gable pier (墀頭)."))
	int32 ChitouCorbelSteps = 3;

	// --- Roof ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(HutongBasic, DisplayName="Roof Tile (瓦作)", ToolTip="Type of tile laid on the roof."))
	EHutongRoofTile RoofTile = EHutongRoofTile::He;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Row Spacing (壟)", UIMin="12", UIMax="60", ClampMin="6", Units="cm", ToolTip="Spacing between tile rows across the roof, in cm."))
	double TileRowSpacing = HutongGen::RoofTile::DefaultRowSpacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(HutongBasic, DisplayName="Rear Eave (後檐)", ToolTip="What stands behind the building."))
	EHutongRearEave RearEave = EHutongRearEave::Courtyard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Front Roof Overhang", UIMin="20", UIMax="160", ClampMin="0", Units="cm", ToolTip="How far the front eave overhangs the facade, in cm."))
	double RoofOverhang = 85.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rear Roof Overhang", EditCondition="RearEave == EHutongRearEave::Lane", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="How far the rear eave overhangs the back wall, in cm."))
	double RearRoofOverhang = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="40", UIMax="250", ClampMin="10", Units="cm", ToolTip="Rise of the roof from eave to ridge, in cm."))
	double RoofRise = 150.0;

	// Roof figures shared by generator, massing block, preview and ridge estimate.
	double GetEaveHeight() const { return FMath::Max(EaveHeight, 60.0); }
	// The 臺基, raised when asked; never past a quarter of the eave.
	double GetFloorHeight() const
	{
		const double Asked = bRaisedPlatform ? FMath::Max(FloorHeight, RaisedPlatformHeight) : FloorHeight;
		return FMath::Clamp(Asked, 0.0, 0.25 * GetEaveHeight());
	}
	// Roof above the column tops, the ceiling at the column line, and the roof's base
	// (HutongGen::Proportions::RoofLift).
	// The eave step's 舉 of the roof as built: the section scaled to the fixed rise.
	double GetEaveJu() const { return HutongGen::Jiajia::BuiltEaveJu(HutongGen::Jiajia::MakeSection(EHutongPurlins::Five, 0.5 * Depth, FMath::Max(RoofOverhang, 0.0), RoofApexRoll), GetRoofRise()); }
	double GetRoofLift() const { return HutongGen::Proportions::RoofLift(FMath::Max(ColumnDiameter, 2.0), FMath::Max(RoofOverhang, 0.0), GetEaveJu()); }
	double GetUndersideRise() const { return HutongGen::Proportions::UndersideRise(FMath::Max(ColumnDiameter, 2.0), FMath::Max(RoofOverhang, 0.0), GetEaveJu()); }
	double GetRoofBaseHeight() const { return GetEaveHeight() + GetRoofLift(); }
	double GetRoofRise() const { return FMath::Max(RoofRise, 10.0); }


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Apex Roll (捲棚)", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="Rounding of the ridge into a rolled ridge (捲棚); zero keeps it sharp."))
	double RoofApexRoll = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Has Ridge Course (清水脊)", ToolTip="Adds a plain tile ridge (清水脊) course along the top of the roof."))
	bool bHasRidgeCourse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge Course Height", EditCondition="bHasRidgeCourse", UIMin="0", UIMax="60", Units="cm", ToolTip="Height of the ridge course, in cm."))
	double RidgeCourseHeight = 20.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge Course Width", EditCondition="bHasRidgeCourse", UIMin="10", UIMax="90", Units="cm", ToolTip="Width of the ridge course, in cm."))
	double RidgeCourseWidth = 20.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge End Kick (蠍子尾)", EditCondition="bHasRidgeCourse", UIMin="0", UIMax="120", Units="cm", ToolTip="Rise of each ridge-end tail (蠍子尾), in cm."))
	double RidgeEndKick = 50.0;   // HutongCanon::Roof::TailRiseInCourses × the ridge course

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Depth", UIMin="0", UIMax="25", ClampMin="0", Units="cm", ToolTip="Vertical depth of the fascia board along the eave, in cm."))
	double EaveFasciaDepth = 8.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Width", UIMin="4", UIMax="30", ClampMin="1", Units="cm", ToolTip="Horizontal thickness of the fascia board along the eave, in cm."))
	double EaveFasciaWidth = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Section (椽頭)", UIMin="0", UIMax="18", ClampMin="0", Units="cm", ToolTip="Section size of each exposed rafter end (椽頭), in cm; zero omits them."))
	double RafterEndSection = 7.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Courses (壟)", ToolTip="Models each tile course on the roof as geometry."))
	bool bHasTileRuns = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Spacing", EditCondition="RafterEndSection > 0", UIMin="10", UIMax="50", ClampMin="4", Units="cm", ToolTip="Spacing between rafter ends along the eave, in cm."))
	double RafterEndSpacing = 24.0;

	// Set by the tool from the drag rect.
	double Width = 900.0;
	double Depth = 500.0;
	int32 BayCountOverride = 0;

	double GetColumnRadius() const { return FMath::Max(0.5 * ColumnDiameter, 4.0); }

	// Column radius as laid, capped by the wall the posts stand in. Bay boundaries inset by it, so
	// the plan asks for the same value.
	double GetColumnRadiusFor(double Frontage, double PlanDepth) const
	{
		const double W = FMath::Max(Frontage, 1.0), D = FMath::Max(PlanDepth, 1.0);
		const double T = FMath::Clamp(WallThickness, 1.0, FMath::Min(W, D) * 0.2);
		return FMath::Min(GetColumnRadius(), 0.25 * T + 12.0);
	}

	// Pier projection: at least covers the corner column.
	double GetChitouProjectionFor(double Frontage, double PlanDepth) const
	{
		return HutongGen::Proportions::ChitouProjection(
			ChitouProjection, GetColumnRadiusFor(Frontage, PlanDepth));
	}

	double GetBayBoundary(int32 i, int32 BayCount, double Frontage, double ColR) const
	{
		return HutongGen::BayBoundary(i, BayCount, Frontage, ColR, SideBayWidthRatio, BayCount / 2);
	}

	double GetRearRoofOverhang() const
	{
		return (RearEave == EHutongRearEave::Courtyard)
			? FMath::Max(RoofOverhang, 0.0)
			: FMath::Max(RearRoofOverhang, 0.0);
	}
};

namespace HutongGen
{
	void BuildShopfront(UE::Geometry::FDynamicMesh3& Mesh, const FHutongShopfrontParams& P);
}
