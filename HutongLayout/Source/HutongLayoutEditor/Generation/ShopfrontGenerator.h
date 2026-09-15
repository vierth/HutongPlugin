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

// 鋪面房: the same 硬山 shell as a house with a facade that does the opposite thing.
USTRUCT(BlueprintType)
struct FHutongShopfrontParams
{
	GENERATED_BODY()

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(DisplayName="Open Bays", UIMin="0", UIMax="5", ClampMin="0", ClampMax="12", ToolTip="Number of bays left open, counted outward from the middle."))
	int32 OpenBayCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(DisplayName="Board Width", UIMin="15", UIMax="45", ClampMin="8", Units="cm", ToolTip="Width of each board of the board doors (排板門), in cm."))
	double BoardWidth = 26.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(DisplayName="Board Thickness", UIMin="3", UIMax="12", ClampMin="1", Units="cm", ToolTip="Thickness of each board of the board doors (排板門), in cm."))
	double BoardThickness = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(DisplayName="Has Counter (櫃檯)", ToolTip="Adds a shop counter (櫃檯) across each open bay."))
	bool bHasCounter = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(DisplayName="Counter Height", EditCondition="bHasCounter", UIMin="70", UIMax="110", ClampMin="30", Units="cm", ToolTip="Height of the counter above the floor, in cm."))
	double CounterHeight = 88.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(DisplayName="Counter Depth", EditCondition="bHasCounter", UIMin="30", UIMax="80", ClampMin="10", Units="cm", ToolTip="Depth of the counter from front to back, in cm."))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fascia", meta=(DisplayName="Has Signboard (匾額)", ToolTip="Adds a name plaque (匾額) as a signboard over the open bay."))
	bool bHasSignboard = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fascia", meta=(DisplayName="Signboard Height", EditCondition="bHasSignboard", UIMin="30", UIMax="90", ClampMin="10", Units="cm", ToolTip="Height of the signboard, in cm."))
	double SignboardHeight = 52.0;

	// --- Shell ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Base Course Height (下鹼)", UIMin="0", UIMax="150", ClampMin="0", Units="cm", ToolTip="Height of the base course (下鹼) above the floor, in cm; zero derives it."))
	double BaseCourseHeight = 0.0;

	double GetBaseCourseHeight() const
	{
		return BaseCourseHeight > 0.0 ? BaseCourseHeight : FMath::Max(HutongCanon::BaseCourse::TopCm - FMath::Max(FloorHeight, 0.0), 25.0);
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Tile (瓦作)", ToolTip="Type of tile laid on the roof."))
	EHutongRoofTile RoofTile = EHutongRoofTile::He;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Row Spacing (壟)", UIMin="12", UIMax="60", ClampMin="6", Units="cm", ToolTip="Spacing between tile rows across the roof, in cm."))
	double TileRowSpacing = HutongGen::RoofTile::DefaultRowSpacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rear Eave (後檐)", ToolTip="What stands behind the building."))
	EHutongRearEave RearEave = EHutongRearEave::Courtyard;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Front Roof Overhang", UIMin="20", UIMax="160", ClampMin="0", Units="cm", ToolTip="How far the front eave overhangs the facade, in cm."))
	double RoofOverhang = 85.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rear Roof Overhang", EditCondition="RearEave == EHutongRearEave::Lane", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="How far the rear eave overhangs the back wall, in cm."))
	double RearRoofOverhang = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="40", UIMax="250", ClampMin="10", Units="cm", ToolTip="Rise of the roof from eave to ridge, in cm."))
	double RoofRise = 150.0;

	// The figures the roof is built with, read by generator, massing block, preview and ridge
	// estimate alike; the floors used to live in the generator only, where nothing else saw them.
	double GetEaveHeight() const { return FMath::Max(EaveHeight, 60.0); }
	double GetRoofRise() const { return FMath::Max(RoofRise, 10.0); }


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Apex Roll (捲棚)", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="Rounding of the ridge into a rolled ridge (捲棚); zero keeps it sharp."))
	double RoofApexRoll = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Has Ridge Course (清水脊)", ToolTip="Adds a plain tile ridge (清水脊) course along the top of the roof."))
	bool bHasRidgeCourse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge Course Height", EditCondition="bHasRidgeCourse", UIMin="0", UIMax="60", Units="cm", ToolTip="Height of the ridge course, in cm."))
	double RidgeCourseHeight = 20.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge Course Width", EditCondition="bHasRidgeCourse", UIMin="10", UIMax="90", Units="cm", ToolTip="Width of the ridge course, in cm."))
	double RidgeCourseWidth = 20.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge End Kick (蠍子尾)", EditCondition="bHasRidgeCourse", UIMin="0", UIMax="60", Units="cm", ToolTip="Rise of each ridge-end tail (蠍子尾), in cm."))
	double RidgeEndKick = 28.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Depth", UIMin="0", UIMax="25", ClampMin="0", Units="cm", ToolTip="Vertical depth of the fascia board along the eave, in cm."))
	double EaveFasciaDepth = 8.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Width", UIMin="4", UIMax="30", ClampMin="1", Units="cm", ToolTip="Horizontal thickness of the fascia board along the eave, in cm."))
	double EaveFasciaWidth = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Section (椽頭)", UIMin="0", UIMax="18", ClampMin="0", Units="cm", ToolTip="Section size of each exposed rafter end (椽頭), in cm; zero omits them."))
	double RafterEndSection = 7.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Spacing", EditCondition="RafterEndSection > 0", UIMin="10", UIMax="50", ClampMin="4", Units="cm", ToolTip="Spacing between rafter ends along the eave, in cm."))
	double RafterEndSpacing = 24.0;

	// Set by the tool from the drag rect; not user-editable.
	double Width = 900.0;
	double Depth = 500.0;
	int32 BayCountOverride = 0;

	double GetColumnRadius() const { return FMath::Max(0.5 * ColumnDiameter, 4.0); }

	// The radius the columns are actually laid on: held down against the wall the shopfront's
	// posts stand in. The bay boundaries are inset by it, so the plan asks for the same number.
	double GetColumnRadiusFor(double Frontage, double PlanDepth) const
	{
		const double W = FMath::Max(Frontage, 1.0), D = FMath::Max(PlanDepth, 1.0);
		const double T = FMath::Clamp(WallThickness, 1.0, FMath::Min(W, D) * 0.2);
		return FMath::Min(GetColumnRadius(), 0.25 * T + 12.0);
	}

	// The projection the pier is built with: never less than covers the corner column.
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
