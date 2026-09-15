#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRoofType.h"
#include "Generation/HutongRoofTile.h"
#include "PavilionGenerator.generated.h"

// 亭: the garden pavilion and the 井亭 over a well.
USTRUCT(BlueprintType)
struct FHutongPavilionParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pavilion", meta=(UIMin="220", UIMax="400", ClampMin="120", Units="cm", ToolTip="Height of the eave above the ground, in cm."))
	double EaveHeight = 285.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pavilion", meta=(DisplayName="Floor Height (臺基)", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="Height of the platform (臺基) above the ground, in cm."))
	double FloorHeight = 26.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pavilion", meta=(DisplayName="Platform Overhang", UIMin="0", UIMax="70", ClampMin="0", Units="cm", ToolTip="How far the platform projects past the column line on every side, in cm."))
	double PlatformOverhang = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pavilion", meta=(DisplayName="Column Diameter", UIMin="12", UIMax="35", ClampMin="5", Units="cm", ToolTip="Diameter of each column at its base, in cm."))
	double ColumnDiameter = 21.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pavilion", meta=(DisplayName="Column Taper (收分)", UIMin="0", UIMax="0.02", ClampMin="0", ClampMax="0.05", ToolTip="How much each column narrows toward its top, as a fraction of its height."))
	double ColumnTaperRatio = HutongCanon::Module::ColumnTaperRatio;

	// --- 楣子 ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Has Hanging Frieze (倒掛楣子)", ToolTip="Builds a hanging frieze (倒掛楣子) below the lintel on each side."))
	bool bHasFrieze = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Frieze Drop", EditCondition="bHasFrieze", UIMin="15", UIMax="80", ClampMin="5", Units="cm", ToolTip="How far the hanging frieze drops below the lintel, in cm."))
	double FriezeDrop = 36.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Frieze Bar Section", EditCondition="bHasFrieze", UIMin="2", UIMax="10", ClampMin="1", Units="cm", ToolTip="Thickness of each bar in the hanging frieze, in cm."))
	double FriezeBarSection = 4.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Frieze Bars Per Side", EditCondition="bHasFrieze", UIMin="2", UIMax="14", ClampMin="0", ClampMax="24", ToolTip="Number of vertical bars in the hanging frieze on each side."))
	int32 FriezeBars = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Has Bench (坐凳楣子)", ToolTip="Builds a bench rail (坐凳楣子) between the columns on the closed sides."))
	bool bHasBench = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Open Sides", EditCondition="bHasBench", UIMin="0", UIMax="4", ClampMin="0", ClampMax="4", ToolTip="Number of sides left without a bench."))
	int32 OpenSides = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Bench Height", EditCondition="bHasBench", UIMin="30", UIMax="70", ClampMin="15", Units="cm", ToolTip="Height of the bench seat above the platform, in cm."))
	double BenchHeight = 45.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Bench Depth", EditCondition="bHasBench", UIMin="15", UIMax="55", ClampMin="8", Units="cm", ToolTip="Depth of the bench seat, in cm."))
	double BenchDepth = 30.0;

	// --- Roof ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Type", ToolTip="Roof form of the pavilion."))
	EHutongRoofType RoofType = EHutongRoofType::Cuanjian;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Apex Roll (捲棚)", UIMin="0", UIMax="0.6", ClampMin="0", ClampMax="1", ToolTip="Rounding of the roof apex into a rolled ridge (捲棚); 0 keeps it sharp."))
	double RoofApexRoll = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Gable Inset (收山, 歇山)", EditCondition="RoofType == EHutongRoofType::Xieshan", UIMin="40", UIMax="160", ClampMin="5", Units="cm", ToolTip="How far in from each end the gable face (山花) stands, in cm."))
	double ShouInset = HutongCanon::Roof::PavilionShouInsetCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge Course Height (歇山)", EditCondition="RoofType == EHutongRoofType::Xieshan && RoofApexRoll <= 0", UIMin="0", UIMax="40", ClampMin="0", Units="cm", ToolTip="Height of the ridge courses on a hip-and-gable roof (歇山), in cm."))
	double RidgeCourseHeight = 17.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge Course Width (歇山)", EditCondition="RoofType == EHutongRoofType::Xieshan && RoofApexRoll <= 0", UIMin="0", UIMax="30", ClampMin="0", Units="cm", ToolTip="Width of the ridge courses on a hip-and-gable roof (歇山), in cm."))
	double RidgeCourseWidth = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Bargeboard Depth (博風板, 歇山)", EditCondition="RoofType == EHutongRoofType::Xieshan", UIMin="0", UIMax="45", ClampMin="0", Units="cm", ToolTip="Depth of the bargeboard (博風板) down each gable rake, in cm."))
	double BargeBoardDepth = 24.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Bargeboard Thickness (博風板, 歇山)", EditCondition="RoofType == EHutongRoofType::Xieshan", UIMin="0", UIMax="15", ClampMin="0", Units="cm", ToolTip="Thickness of the bargeboard (博風板), in cm."))
	double BargeBoardThickness = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Tile (瓦作)", ToolTip="Tile the roof is laid in."))
	EHutongRoofTile RoofTile = EHutongRoofTile::He;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Row Spacing (壟)", UIMin="12", UIMax="60", ClampMin="6", Units="cm", ToolTip="Spacing between tile rows (壟) along the eave, in cm."))
	double TileRowSpacing = HutongGen::RoofTile::DefaultRowSpacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="30", UIMax="150", ClampMin="0", Units="cm", ToolTip="How far the eave projects past the column line, in cm."))
	double RoofOverhang = 72.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="0", UIMax="250", ClampMin="0", Units="cm", ToolTip="Height of the ridge above the eave, in cm; zero derives it."))
	double RoofRise = 0.0;

	// The figures the roof is built with, read by generator, massing block, preview and ridge
	// estimate alike. The rise is the section's own unless RoofRise names one: the generator used
	// to read the field and the block the section, which is the gate house's fault over again.
	double GetEaveHeight() const { return FMath::Max(EaveHeight, 60.0); }
	double GetRoofRise(double OverDepth) const
	{
		if (RoofRise > 0.0) return RoofRise;
		const HutongGen::FHutongRoofSection S = HutongGen::Jiajia::MakeSection(
			Purlins, 0.5 * (FMath::Max(OverDepth, 1.0) + 2.0 * FMath::Max(RoofOverhang, 0.0)), 0.0, RoofApexRoll);
		return FMath::Max(S.Rise(), 10.0);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Slope Steps (舉架)", ToolTip="Number of roof pitch steps (舉架) between the eave and the apex."))
	EHutongPurlins Purlins = EHutongPurlins::Five;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Corner Flare Rise (翼角)", UIMin="0", UIMax="100", ClampMin="0", Units="cm", ToolTip="Lift of each roof corner into its upturned corner (翼角), in cm."))
	double RoofFlareRise = 42.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Corner Flare Run", UIMin="0", UIMax="80", ClampMin="0", Units="cm", ToolTip="Outward push of each roof corner into its upturned corner (翼角), in cm."))
	double RoofFlareRun = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Depth", UIMin="0", UIMax="25", ClampMin="0", Units="cm", ToolTip="Depth of the fascia board along the eave, in cm."))
	double EaveFasciaDepth = 9.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Eave Segments", UIMin="2", UIMax="16", ClampMin="2", ClampMax="32", ToolTip="Number of mesh segments along each eave."))
	int32 RoofEaveSegments = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Slope Segments", UIMin="1", UIMax="8", ClampMin="1", ClampMax="16", ToolTip="Number of mesh segments up each roof slope."))
	int32 RoofSlopeSegments = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Has Finial (寶頂)", EditCondition="RoofType == EHutongRoofType::Cuanjian", ToolTip="Builds a roof finial (寶頂) at the apex of a pyramidal roof (攢尖)."))
	bool bHasFinial = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Finial Height", EditCondition="bHasFinial && RoofType == EHutongRoofType::Cuanjian", UIMin="20", UIMax="110", ClampMin="5", Units="cm", ToolTip="Height of the roof finial (寶頂) above the roof apex, in cm."))
	double FinialHeight = 52.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Finial Width", EditCondition="bHasFinial && RoofType == EHutongRoofType::Cuanjian", UIMin="10", UIMax="60", ClampMin="4", Units="cm", ToolTip="Width of the roof finial (寶頂) at its widest, in cm."))
	double FinialWidth = 27.0;

	// Set by the tool from the drag rect; not user-editable.
	double Width = 300.0;
	double Depth = 300.0;

	double GetColumnRadius() const { return FMath::Max(0.5 * ColumnDiameter, 4.0); }
	double GetColumnHeight() const { return FMath::Max(EaveHeight - FloorHeight, 1.0); }
};

namespace HutongGen
{
	void BuildPavilion(UE::Geometry::FDynamicMesh3& Mesh, const FHutongPavilionParams& P);
}
