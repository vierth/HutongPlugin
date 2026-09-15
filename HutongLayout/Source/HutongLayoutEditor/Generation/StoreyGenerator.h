#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRoofTile.h"
#include "Generation/HutongBays.h"
#include "Generation/HutongDoor.h"
#include "Generation/HutongRearEave.h"
#include "StoreyGenerator.generated.h"

// 樓: the two-storey street building the 乾隆京城全圖 draws with a second tier of bays over the
// shopfront — 酒樓, 茶樓, a 樓 over a shop. The same 硬山 shell and the same street facade as a
// 鋪面房 below, with a tiled skirt (腰檐) marking the story line and a railed gallery over it.
USTRUCT(BlueprintType)
struct FHutongStoreyParams
{
	GENERATED_BODY()

	// --- Storeys ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stories", meta=(DisplayName="Lower Story Height", UIMin="240", UIMax="420", ClampMin="150", Units="cm", ToolTip="Height of the ground story from its floor to the story line, in cm."))
	double LowerStoreyHeight = 330.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stories", meta=(DisplayName="Upper Story Height", UIMin="200", UIMax="380", ClampMin="140", Units="cm", ToolTip="Height of the upper story from the gallery deck to the eave, in cm."))
	double UpperStoreyHeight = 275.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stories", meta=(UIMin="15", UIMax="60", ClampMin="8", Units="cm", ToolTip="Thickness of the masonry walls, in cm."))
	double WallThickness = 34.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stories", meta=(DisplayName="Floor Height (臺基)", UIMin="0", UIMax="45", ClampMin="0", Units="cm", ToolTip="Height of the platform (臺基) above the ground, in cm."))
	double FloorHeight = 18.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stories", meta=(DisplayName="Platform Overhang", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="How far the platform projects in front of the facade, in cm."))
	double PlatformOverhang = 24.0;

	// --- Bays ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(UIMin="180", UIMax="360", ClampMin="80", Units="cm", ToolTip="Narrowest bay width allowed when dividing the frontage into bays, in cm."))
	double MinBayWidth = 220.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(UIMin="200", UIMax="420", ClampMin="80", Units="cm", ToolTip="Widest bay width allowed when dividing the frontage into bays, in cm."))
	double MaxBayWidth = 300.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(DisplayName="Side Bay / Central Bay", UIMin="0.8", UIMax="1.0", ClampMin="0.4", ClampMax="1", ToolTip="Width of each side bay as a fraction of the central bay's width."))
	double SideBayWidthRatio = 0.95;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(DisplayName="Column Diameter", UIMin="15", UIMax="45", ClampMin="6", Units="cm", ToolTip="Diameter of each column at its base, in cm."))
	double ColumnDiameter = 26.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bays", meta=(DisplayName="Column Taper (收分)", UIMin="0", UIMax="0.02", ClampMin="0", ClampMax="0.05", ToolTip="Taper of each column toward its top, as a fraction of its height."))
	double ColumnTaperRatio = HutongCanon::Module::ColumnTaperRatio;

	// --- The shop below ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shopfront", meta=(DisplayName="Opening Head / Story", UIMin="0.6", UIMax="0.92", ClampMin="0.3", ClampMax="0.95", ToolTip="Height of the shopfront opening's head as a fraction of the lower story."))
	double OpeningTopRatio = 0.82;

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

	// --- 腰檐 and the gallery ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story Line", meta=(DisplayName="Has Skirt Roof (腰檐)", ToolTip="Adds the tiled skirt roof (腰檐) at the story line."))
	bool bHasSkirtRoof = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story Line", meta=(DisplayName="Skirt Projection", EditCondition="bHasSkirtRoof", UIMin="30", UIMax="140", ClampMin="10", Units="cm", ToolTip="How far the skirt roof (腰檐) projects in front of the facade, in cm."))
	double SkirtProjection = 78.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story Line", meta=(DisplayName="Skirt Drop", EditCondition="bHasSkirtRoof", UIMin="10", UIMax="50", ClampMin="4", Units="cm", ToolTip="How far the skirt roof falls from the wall to its outer edge, in cm."))
	double SkirtDrop = 22.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story Line", meta=(DisplayName="Skirt Rafter Ends (椽頭)", EditCondition="bHasSkirtRoof", ToolTip="Adds a row of exposed rafter ends (椽頭) under the skirt roof."))
	bool bHasSkirtRafters = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story Line", meta=(DisplayName="Has Gallery (欄杆)", ToolTip="Adds the balcony deck and its railing (欄杆) across the upper front."))
	bool bHasGallery = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story Line", meta=(DisplayName="Gallery Depth", EditCondition="bHasGallery", UIMin="40", UIMax="140", ClampMin="20", Units="cm", ToolTip="How far the gallery deck projects in front of the upper facade, in cm."))
	double GalleryDepth = 92.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story Line", meta=(DisplayName="Deck Thickness", EditCondition="bHasGallery", UIMin="6", UIMax="30", ClampMin="3", Units="cm", ToolTip="Thickness of the gallery deck, in cm."))
	double DeckThickness = 14.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story Line", meta=(DisplayName="Rail Height", EditCondition="bHasGallery", UIMin="70", UIMax="120", ClampMin="40", Units="cm", ToolTip="Height of the railing above the gallery deck, in cm."))
	double RailHeight = 92.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story Line", meta=(DisplayName="Baluster Spacing", EditCondition="bHasGallery", UIMin="12", UIMax="60", ClampMin="6", Units="cm", ToolTip="Spacing between the railing's balusters, in cm."))
	double BalusterSpacing = 26.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Story Line", meta=(DisplayName="Baluster Section", EditCondition="bHasGallery", UIMin="3", UIMax="12", ClampMin="1", Units="cm", ToolTip="Section of each baluster and rail member, in cm."))
	double RailSection = 6.0;

	// --- The upper story's front ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Upper Front", meta=(DisplayName="Sill Height", UIMin="0", UIMax="90", ClampMin="0", Units="cm", ToolTip="Height of the upper windows' sill above the gallery deck, in cm."))
	double UpperSillHeight = 34.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Upper Front", meta=(DisplayName="Header Drop", UIMin="15", UIMax="90", ClampMin="5", Units="cm", ToolTip="How far the upper windows' head sits below the eave, in cm."))
	double UpperHeaderDrop = 42.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Upper Front", meta=(DisplayName="Has Window Lattice (窗欞)", ToolTip="Adds lattice bars across each upper window."))
	bool bHasWindowLattice = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Upper Front", meta=(DisplayName="Lattice Bar Thickness", EditCondition="bHasWindowLattice", UIMin="2", UIMax="10", ClampMin="1", Units="cm", ToolTip="Thickness of each lattice bar, in cm."))
	double WindowLatticeThickness = 4.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Upper Front", meta=(DisplayName="Lattice Bars Per Bay", EditCondition="bHasWindowLattice", UIMin="2", UIMax="10", ClampMin="0", ClampMax="16", ToolTip="Number of upright lattice bars across each upper window."))
	int32 WindowLatticeBars = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Upper Front", meta=(DisplayName="Has Window Paper (窗紙)", ToolTip="Adds the paper pane (窗紙) behind each upper window's lattice."))
	bool bHasWindowPaper = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Upper Front", meta=(DisplayName="Has Signboard (匾額)", ToolTip="Adds a name plaque (匾額) hung on the gallery rail."))
	bool bHasSignboard = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Upper Front", meta=(DisplayName="Signboard Height", EditCondition="bHasSignboard", UIMin="30", UIMax="90", ClampMin="10", Units="cm", ToolTip="Height of the signboard, in cm."))
	double SignboardHeight = 56.0;

	// --- Shell ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Base Course Height (下鹼)", UIMin="0", UIMax="150", ClampMin="0", Units="cm", ToolTip="Height of the base course (下鹼) above the floor, in cm; zero derives it."))
	double BaseCourseHeight = 0.0;

	double GetBaseCourseHeight() const
	{
		return BaseCourseHeight > 0.0 ? BaseCourseHeight : FMath::Max(HutongCanon::BaseCourse::TopCm - FMath::Max(FloorHeight, 0.0), 25.0);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Shell", meta=(DisplayName="Base Course Projection", UIMin="0", UIMax="15", ClampMin="0", Units="cm", ToolTip="How far the base course stands proud of the wall face, in cm."))
	double BaseCourseProjection = 3.0;

	// --- Roof ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Roof Tile (瓦作)", ToolTip="Type of tile laid on the roof."))
	EHutongRoofTile RoofTile = EHutongRoofTile::He;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Tile Row Spacing (壟)", UIMin="12", UIMax="60", ClampMin="6", Units="cm", ToolTip="Spacing between tile rows across the roof, in cm."))
	double TileRowSpacing = HutongGen::RoofTile::DefaultRowSpacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rear Eave (後檐)", ToolTip="What stands behind the building."))
	EHutongRearEave RearEave = EHutongRearEave::Lane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Front Roof Overhang", UIMin="20", UIMax="160", ClampMin="0", Units="cm", ToolTip="How far the front eave overhangs the facade, in cm."))
	double RoofOverhang = 90.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rear Roof Overhang", EditCondition="RearEave == EHutongRearEave::Lane", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="How far the rear eave overhangs the back wall, in cm."))
	double RearRoofOverhang = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="40", UIMax="250", ClampMin="10", Units="cm", ToolTip="Rise of the roof from eave to ridge, in cm."))
	double RoofRise = 155.0;

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
	double RidgeEndKick = 30.0;

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
	double Depth = 620.0;
	int32 BayCountOverride = 0;

	double GetColumnRadius() const { return FMath::Max(0.5 * ColumnDiameter, 4.0); }

	// The radius the columns are actually laid on, held against the wall they stand in, exactly as
	// the shopfront's is — the bay boundaries are inset by it, so the plan asks for the same number.
	double GetColumnRadiusFor(double Frontage, double PlanDepth) const
	{
		const double W = FMath::Max(Frontage, 1.0), D = FMath::Max(PlanDepth, 1.0);
		const double T = FMath::Clamp(WallThickness, 1.0, FMath::Min(W, D) * 0.2);
		return FMath::Min(GetColumnRadius(), 0.25 * T + 12.0);
	}

	double GetBayBoundary(int32 i, int32 BayCount, double Frontage, double ColR) const
	{
		return HutongGen::BayBoundary(i, BayCount, Frontage, ColR, SideBayWidthRatio, BayCount / 2);
	}

	// The story line: the top of the ground story and the level the gallery deck lies on.
	double GetStoreyLineHeight() const
	{
		return FMath::Clamp(FloorHeight, 0.0, 120.0) + FMath::Max(LowerStoreyHeight, 150.0);
	}

	// The eave above the ground, which is both storys and the platform.
	double GetEaveHeight() const
	{
		return GetStoreyLineHeight() + FMath::Max(UpperStoreyHeight, 140.0);
	}

	// The head of the shopfront's own opening, floored so the ground story is a way in and not a
	// picture of one: the mesh is its own collision.
	double GetOpeningTopHeight() const
	{
		const double Floor = FMath::Clamp(FloorHeight, 0.0, 120.0);
		const double Asked = Floor + FMath::Clamp(OpeningTopRatio, 0.3, 0.95) * FMath::Max(LowerStoreyHeight, 150.0);
		return FMath::Max(Asked, Floor + HutongGen::Passage::MinClearHeight);
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
	void BuildStorey(UE::Geometry::FDynamicMesh3& Mesh, const FHutongStoreyParams& P);
}
