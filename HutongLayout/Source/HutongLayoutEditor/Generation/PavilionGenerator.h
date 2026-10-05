#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongProportions.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRoofType.h"
#include "Generation/HutongRoofTile.h"
#include "PavilionGenerator.generated.h"

// The two plans of the 則例: 卷二十二's square and 卷二十三's round.
UENUM(BlueprintType)
enum class EHutongPavilionPlan : uint8
{
	Square  UMETA(DisplayName = "Square, Four Columns (四角方亭)"),
	Round   UMETA(DisplayName = "Round, Six Columns (六柱圓亭)"),
};

// 亭: garden pavilion, or 井亭 over a well.
USTRUCT(BlueprintType)
struct FHutongPavilionParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pavilion", meta=(HutongBasic, DisplayName="Plan", ToolTip="Square on four columns, or round on six."))
	EHutongPavilionPlan Plan = EHutongPavilionPlan::Square;

	// Off: the fields below the switch are used as set. The raw defaults are the figure's at its own 面闊.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pavilion", meta=(DisplayName="Proportions From the Qing Regulations (則例)", ToolTip="Sizes every part from the column spacing (面闊), per the Qing building regulations (則例)."))
	bool bDeriveProportions = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pavilion", meta=(EditCondition="!bDeriveProportions", UIMin="220", UIMax="400", ClampMin="120", Units="cm", ToolTip="Height of the column tops above the ground, in cm."))
	double EaveHeight = 301.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pavilion", meta=(DisplayName="Floor Height (臺基)", EditCondition="!bDeriveProportions", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="Height of the platform (臺基) above the ground, in cm."))
	double FloorHeight = 38.4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pavilion", meta=(DisplayName="Platform Overhang", EditCondition="!bDeriveProportions", UIMin="0", UIMax="90", ClampMin="0", Units="cm", ToolTip="How far the platform projects past the columns' outer faces, in cm."))
	double PlatformOverhang = 50.2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pavilion", meta=(HutongBasic, DisplayName="Steps (如意踏跺)", ToolTip="Builds steps at the middle of each side."))
	bool bHasSteps = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pavilion", meta=(HutongBasic, DisplayName="Square Posts (方柱)", ToolTip="Builds square chamfered posts instead of round columns."))
	bool bSquarePosts = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pavilion", meta=(DisplayName="Column Diameter", EditCondition="!bDeriveProportions", UIMin="12", UIMax="35", ClampMin="5", Units="cm", ToolTip="Diameter of each column at its base, in cm."))
	double ColumnDiameter = 22.4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pavilion", meta=(DisplayName="Column Taper (收分)", UIMin="0", UIMax="0.02", ClampMin="0", ClampMax="0.05", ToolTip="How much each column narrows toward its top, as a fraction of its height."))
	double ColumnTaperRatio = HutongCanon::Module::ColumnTaperRatio;

	// --- 楣子 ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Has Hanging Frieze (倒掛楣子)", ToolTip="Builds a hanging frieze (倒掛楣子) below the lintel on each side."))
	bool bHasFrieze = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Frieze Drop", EditCondition="bHasFrieze", UIMin="15", UIMax="80", ClampMin="5", Units="cm", ToolTip="How far the hanging frieze drops below the lintel, in cm."))
	double FriezeDrop = 36.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Frieze Bar Section", EditCondition="bHasFrieze", UIMin="2", UIMax="10", ClampMin="1", Units="cm", ToolTip="Thickness of each bar in the hanging frieze, in cm."))
	double FriezeBarSection = 4.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Frieze Bars Per Side", EditCondition="bHasFrieze", UIMin="2", UIMax="14", ClampMin="0", ClampMax="24", ToolTip="Number of vertical bars in the hanging frieze on each side."))
	int32 FriezeBars = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(HutongBasic, DisplayName="Has Bench (坐凳欄杆)", ToolTip="Builds a bench rail (坐凳欄杆) from each column, open at the middle of every side."))
	bool bHasBench = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Bench Height", EditCondition="bHasBench && !bDeriveProportions", UIMin="30", UIMax="70", ClampMin="15", Units="cm", ToolTip="Height of the bench seat above the platform, in cm."))
	double BenchHeight = 51.2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frieze", meta=(DisplayName="Bench Depth", EditCondition="bHasBench && !bDeriveProportions", UIMin="15", UIMax="55", ClampMin="8", Units="cm", ToolTip="Depth of the bench seat, in cm."))
	double BenchDepth = 22.4;

	// --- Roof ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(HutongBasic, DisplayName="Roof Type", EditCondition="Plan == EHutongPavilionPlan::Square", EditConditionHides, ToolTip="Roof form of the pavilion."))
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
	EHutongRoofTile RoofTile = EHutongRoofTile::Tong;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Row Spacing (壟)", UIMin="12", UIMax="60", ClampMin="6", Units="cm", ToolTip="Spacing between tile rows (壟) along the eave, in cm."))
	double TileRowSpacing = HutongGen::RoofTile::DefaultRowSpacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(EditCondition="!bDeriveProportions", UIMin="30", UIMax="150", ClampMin="0", Units="cm", ToolTip="How far the eave projects past the column line, in cm."))
	double RoofOverhang = 76.8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="0", UIMax="250", ClampMin="0", Units="cm", ToolTip="Height of the ridge above the eave, in cm; 0 for automatic."))
	double RoofRise = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Slope Steps (舉架)", ToolTip="Number of roof pitch steps (舉架) between the eave and the apex."))
	EHutongPurlins Purlins = EHutongPurlins::Five;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Corner Flare Rise (翼角)", EditCondition="!bDeriveProportions", UIMin="0", UIMax="100", ClampMin="0", Units="cm", ToolTip="Lift of each roof corner into its upturned corner (翼角), in cm."))
	double RoofFlareRise = 26.9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Corner Flare Run", EditCondition="!bDeriveProportions", UIMin="0", UIMax="80", ClampMin="0", Units="cm", ToolTip="Outward push of each roof corner into its upturned corner (翼角), in cm."))
	double RoofFlareRun = 20.2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Depth", UIMin="0", UIMax="25", ClampMin="0", Units="cm", ToolTip="Depth of the fascia board along the eave, in cm."))
	double EaveFasciaDepth = 9.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Courses (壟)", ToolTip="Models each tile course down the roof instead of drawing it in the texture."))
	bool bHasTileRuns = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Section (椽頭)", EditCondition="!bDeriveProportions", UIMin="0", UIMax="15", ClampMin="0", Units="cm", ToolTip="Size of the rafter ends under the eave, in cm; zero omits them."))
	double RafterEndSection = 6.7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Spacing", EditCondition="!bDeriveProportions && RafterEndSection > 0", UIMin="10", UIMax="50", ClampMin="4", Units="cm", ToolTip="Spacing between rafter ends along the eave, in cm."))
	double RafterEndSpacing = 13.4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Eave Segments", UIMin="2", UIMax="16", ClampMin="2", ClampMax="32", ToolTip="Number of mesh segments along each eave."))
	int32 RoofEaveSegments = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Slope Segments", UIMin="1", UIMax="8", ClampMin="1", ClampMax="16", ToolTip="Number of mesh segments up each roof slope."))
	int32 RoofSlopeSegments = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(HutongBasic, DisplayName="Has Finial (寶頂)", EditCondition="Plan == EHutongPavilionPlan::Round || RoofType == EHutongRoofType::Cuanjian", ToolTip="Builds a roof finial (寶頂) at the apex of a pyramidal roof (攢尖)."))
	bool bHasFinial = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Finial Height", EditCondition="bHasFinial && !bDeriveProportions && (Plan == EHutongPavilionPlan::Round || RoofType == EHutongRoofType::Cuanjian)", UIMin="20", UIMax="160", ClampMin="5", Units="cm", ToolTip="Height of the roof finial (寶頂) above its base course, in cm."))
	double FinialHeight = 102.4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Finial Width", EditCondition="bHasFinial && !bDeriveProportions && (Plan == EHutongPavilionPlan::Round || RoofType == EHutongRoofType::Cuanjian)", UIMin="10", UIMax="120", ClampMin="4", Units="cm", ToolTip="Width of the roof finial (寶頂) at its widest, in cm."))
	double FinialWidth = 86.4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Exposed Frame (徹上明造)", EditCondition="Plan == EHutongPavilionPlan::Round || RoofType == EHutongRoofType::Cuanjian", ToolTip="No ceiling: shows the roof frame (梁架) open to view from below."))
	bool bExposedFrame = true;

	// Set by the tool from the drag rect: the columns' outer faces.
	double Width = 342.4;
	double Depth = 342.4;

	// Every dimension through these; the raw fields only when bDeriveProportions is off.
	// 面闊: column centre to centre over the shorter side, the module the figure scales on.
	bool IsRound() const { return Plan == EHutongPavilionPlan::Round; }
	// A round roof is a 攢尖 whatever RoofType says.
	bool IsCuanjian() const { return IsRound() || RoofType == EHutongRoofType::Cuanjian; }

	double GetBay() const
	{
		const double Side = FMath::Max(FMath::Min(Width, Depth), 1.0);
		return bDeriveProportions ? Side / (1.0 + HutongCanon::Pavilion::ColumnPerBay)
			: FMath::Max(Side - FMath::Max(ColumnDiameter, 1.0), 1.0);
	}
	double Derived(double PerBay, double Raw) const { return bDeriveProportions ? PerBay * GetBay() : Raw; }

	double GetColumnDiameter() const { return FMath::Max(Derived(HutongCanon::Pavilion::ColumnPerBay, ColumnDiameter), 5.0); }
	double GetColumnRadius() const { return FMath::Max(0.5 * GetColumnDiameter(), 4.0); }
	// As planted: never more than a sixth of the plan.
	double GetPlanColumnRadius() const { return FMath::Min(GetColumnRadius(), 0.15 * FMath::Max(FMath::Min(Width, Depth), 1.0)); }
	double GetFloorHeight() const { return FMath::Max(Derived(HutongCanon::Pavilion::PlatformHeightPerBay, FloorHeight), 0.0); }
	// 古鏡 on the 柱頂石: the column's foot.
	double GetColumnFoot() const { return GetFloorHeight() + (GetFloorHeight() > 0.0 ? HutongCanon::Pavilion::MirrorRise * GetColumnDiameter() : 0.0); }
	// Column top above the ground.
	double GetEaveHeight() const
	{
		return bDeriveProportions ? GetColumnFoot() + HutongCanon::Pavilion::ColumnHeightPerBay * GetBay() : FMath::Max(EaveHeight, 60.0);
	}
	double GetColumnHeight() const { return FMath::Max(GetEaveHeight() - GetColumnFoot(), 1.0); }
	// Past the columns' outer faces.
	double GetPlatformOverhang() const
	{
		const double Reach = IsRound() ? HutongCanon::Pavilion::RoundPlatformReachPerBay : HutongCanon::Pavilion::PlatformReachPerBay;
		return bDeriveProportions ? FMath::Max(Reach * GetBay() - GetPlanColumnRadius(), 0.0)
			: FMath::Max(PlatformOverhang, 0.0);
	}
	double GetRoofOverhang() const { return FMath::Max(Derived(HutongCanon::Pavilion::EaveOverhangPerBay, RoofOverhang), 0.0); }
	double GetBenchHeight() const
	{
		return FMath::Max(Derived(IsRound() ? HutongCanon::Pavilion::RoundBenchHeightPerBay : HutongCanon::Pavilion::BenchHeightPerBay, BenchHeight), 10.0);
	}
	double GetBenchDepth() const { return FMath::Max(Derived(HutongCanon::Pavilion::BenchDepthPerBay, BenchDepth), 5.0); }
	double GetRafterSection() const
	{
		return bDeriveProportions ? HutongCanon::Pavilion::RafterDiameter * GetColumnDiameter() : FMath::Max(RafterEndSection, 0.0);
	}
	double GetRafterSpacing() const
	{
		return bDeriveProportions ? HutongCanon::Pavilion::RafterSpacing * GetColumnDiameter() : FMath::Max(RafterEndSpacing, 4.0);
	}
	double GetFlareRun() const { return bDeriveProportions ? HutongCanon::Pavilion::CornerRun * GetRafterSection() : FMath::Max(RoofFlareRun, 0.0); }
	double GetFlareRise() const { return bDeriveProportions ? HutongCanon::Pavilion::CornerRise * GetRafterSection() : FMath::Max(RoofFlareRise, 0.0); }
	double GetFinialHeight() const { return FMath::Max(Derived(HutongCanon::Pavilion::FinialHeightPerBay, FinialHeight), 4.0); }
	double GetFinialWidth() const { return FMath::Max(Derived(HutongCanon::Pavilion::FinialWidthPerBay, FinialWidth), 2.0); }

	// 舉: the figure's 伍舉 then 柒伍舉 on 五檁, the canonical sequence otherwise.
	TArray<double> GetJu() const
	{
		return Purlins == EHutongPurlins::Five ? TArray<double>(HutongCanon::Pavilion::Ju, UE_ARRAY_COUNT(HutongCanon::Pavilion::Ju))
			: HutongGen::Jiajia::DefaultRatios(Purlins);
	}
	double GetEaveJu() const { return GetJu().Num() > 0 ? GetJu()[0] : 0.5; }

	// Roof figures shared by generator, massing block, preview and ridge estimate.
	// Roof above the column tops (墊板, 檐桁, rafter and cover less the overhang's rise), the ceiling at
	// the column line, and the roof's base (HutongGen::Proportions::RoofLift, on the figure's members).
	double GetRoofLift() const
	{
		namespace C = HutongCanon::Pavilion;
		const double Over = (C::BoardHeight + C::PurlinDiameter + C::RafterDiameter + HutongCanon::Frame::RoofCover) * GetColumnDiameter();
		return FMath::Max(Over - GetEaveJu() * GetRoofOverhang(), 0.0);
	}
	double GetUndersideRise() const
	{
		if (GetRoofLift() <= 0.0) return 0.0;
		return GetEaveJu() * GetRoofOverhang() - HutongCanon::Frame::RoofCover * GetColumnDiameter();
	}
	double GetRoofBaseHeight() const { return GetEaveHeight() + GetRoofLift(); }
	// Column line to column line across Depth, then the overhang on the eave step's 舉.
	HutongGen::FHutongRoofSection GetRoofSection() const
	{
		const double Span = FMath::Max(Depth - 2.0 * GetPlanColumnRadius(), 1.0);
		return HutongGen::Jiajia::MakeSectionWithRatios(GetJu(), 0.5 * Span, GetRoofOverhang(), RoofApexRoll);
	}
	double GetRoofRise() const { return RoofRise > 0.0 ? RoofRise : FMath::Max(GetRoofSection().Rise(), 10.0); }

	// Seeds every raw field from what is built now and stops deriving: a hand edit starts from the figure.
	void FreezeProportions()
	{
		if (!bDeriveProportions) return;
		const double Col = GetColumnDiameter();
		EaveHeight = GetEaveHeight();
		FloorHeight = GetFloorHeight();
		PlatformOverhang = GetPlatformOverhang();
		RoofOverhang = GetRoofOverhang();
		BenchHeight = GetBenchHeight();
		BenchDepth = GetBenchDepth();
		RafterEndSection = GetRafterSection();
		RafterEndSpacing = GetRafterSpacing();
		RoofFlareRun = GetFlareRun();
		RoofFlareRise = GetFlareRise();
		FinialHeight = GetFinialHeight();
		FinialWidth = GetFinialWidth();
		bDeriveProportions = false;
		ColumnDiameter = Col;
	}
};

namespace HutongGen
{
	void BuildPavilion(UE::Geometry::FDynamicMesh3& Mesh, const FHutongPavilionParams& P);
}
