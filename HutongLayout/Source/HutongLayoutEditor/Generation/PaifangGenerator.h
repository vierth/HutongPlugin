#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongBays.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRoofTile.h"
#include "PaifangGenerator.generated.h"

// A type rather than a consequence of the drag: a paifang is 一間二柱 or 三間四柱, never four and a half.
UENUM()
enum class EHutongPaifangBays : uint8
{
	One UMETA(DisplayName = "One Bay, Two Posts (一間二柱)", ToolTip="A single bay between two columns."),
	Three UMETA(DisplayName = "Three Bays, Four Posts (三間四柱)", ToolTip="Three bays between four columns."),
};

USTRUCT(BlueprintType)
struct FHutongPaifangParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Paifang", meta=(ToolTip="Number of bays and columns the paifang has."))
	EHutongPaifangBays BayCount = EHutongPaifangBays::Three;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Paifang", meta=(DisplayName="Has Roofs (牌樓)", ToolTip="Adds a tiled roof over each bay (樓)."))
	bool bHasRoofs = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Paifang", meta=(DisplayName="Columns Through Roof (衝天式)", ToolTip="Runs the columns up past the roofs rather than stopping them beneath."))
	bool bColumnsThroughRoof = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Paifang", meta=(UIMin="300", UIMax="1200", ClampMin="100", Units="cm", ToolTip="Height of the central bay's lintel assembly above the ground, in cm."))
	double Height = 620.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Paifang", meta=(UIMin="20", UIMax="120", ClampMin="8", Units="cm", ToolTip="Diameter of each column at its base, in cm."))
	double ColumnDiameter = 46.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Paifang", meta=(DisplayName="Side Bay / Central Bay", UIMin="0.4", UIMax="1.0", ClampMin="0.3", ClampMax="1", ToolTip="Width of each side bay as a fraction of the central bay's width."))
	double SideBayWidthRatio = 0.65;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Paifang", meta=(DisplayName="Side Bay Drop", UIMin="0", UIMax="0.4", ClampMin="0", ClampMax="0.6", ToolTip="Drop of the side bays' lintels below the central one, as a fraction of the height."))
	double SideBayDrop = 0.17;

	// --- Lintels ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lintels", meta=(DisplayName="Architrave Depth", UIMin="20", UIMax="150", ClampMin="8", Units="cm", ToolTip="Depth of the architrave (額枋) beam, in cm."))
	double ArchitraveDepth = 62.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lintels", meta=(DisplayName="Architrave Thickness", UIMin="10", UIMax="80", ClampMin="5", Units="cm", ToolTip="Thickness of the architrave (額枋) beam from front to back, in cm."))
	double ArchitraveThickness = 32.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lintels", meta=(DisplayName="Has Lower Architrave", ToolTip="Adds a lower architrave (小額枋) below the main one."))
	bool bHasLowerArchitrave = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lintels", meta=(DisplayName="Panel Gap", EditCondition="bHasLowerArchitrave", UIMin="20", UIMax="150", ClampMin="5", Units="cm", ToolTip="Clear gap between the upper and lower architraves, in cm."))
	double PanelGap = 55.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lintels", meta=(DisplayName="Has Plaque (匾額)", EditCondition="bHasLowerArchitrave", ToolTip="Adds a name plaque (匾額) in the central bay."))
	bool bHasPlaque = true;

	// --- Base ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Base", meta=(DisplayName="Plinth Height (夾杆石)", UIMin="0", UIMax="250", ClampMin="0", Units="cm", ToolTip="Height of the post clamp stones (夾杆石), in cm."))
	double PlinthHeight = 120.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Base", meta=(DisplayName="Plinth Spread", UIMin="0", UIMax="80", ClampMin="0", Units="cm", ToolTip="How far each plinth extends beyond its column on every side, in cm."))
	double PlinthSpread = 22.0;

	// --- Roofs ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roofs", meta=(EditCondition="bHasRoofs", UIMin="20", UIMax="200", ClampMin="10", Units="cm", ToolTip="How far each roof's eave overhangs the frame, in cm."))
	double RoofOverhang = 62.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roofs", meta=(EditCondition="bHasRoofs", UIMin="20", UIMax="200", ClampMin="10", Units="cm", ToolTip="Rise of each roof from eave to ridge, in cm."))
	double RoofRise = 62.0;

	// The frame's height, the central 樓's eave on the architrave above it, and the 樓's rise:
	// read by generator, massing block, preview and ridge estimate alike.
	double GetHeight() const { return FMath::Max(Height, 50.0); }
	double GetRoofEaveZ() const { return GetHeight() + FMath::Max(ArchitraveDepth, 8.0); }
	double GetRoofRise() const { return FMath::Max(RoofRise, 5.0); }


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roofs", meta=(DisplayName="Eave Fascia Depth", EditCondition="bHasRoofs", UIMin="0", UIMax="30", Units="cm", ToolTip="Vertical depth of the fascia board along each eave, in cm."))
	double EaveFasciaDepth = 9.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roofs", meta=(DisplayName="Corner Flare Rise (翼角)", EditCondition="bHasRoofs", UIMin="0", UIMax="90", ClampMin="0", Units="cm", ToolTip="Lift of each roof corner into its upturned corner (翼角), in cm."))
	double RoofFlareRise = 32.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roofs", meta=(DisplayName="Corner Flare Run", EditCondition="bHasRoofs", UIMin="0", UIMax="70", ClampMin="0", Units="cm", ToolTip="How far each roof corner sweeps outward along its diagonal, in cm."))
	double RoofFlareRun = 24.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roofs", meta=(DisplayName="Roof Eave Segments", EditCondition="bHasRoofs", UIMin="2", UIMax="16", ClampMin="2", ClampMax="32", ToolTip="Number of segments each roof is divided into along its eave."))
	int32 RoofEaveSegments = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roofs", meta=(DisplayName="Roof Slope Segments", EditCondition="bHasRoofs", UIMin="1", UIMax="8", ClampMin="1", ClampMax="16", ToolTip="Number of segments each roof is divided into up its slope."))
	int32 RoofSlopeSegments = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roofs", meta=(DisplayName="Tile Row Spacing (壟)", UIMin="12", UIMax="60", ClampMin="6", Units="cm", ToolTip="Spacing between tile rows across the roof, in cm."))
	double TileRowSpacing = HutongGen::RoofTile::DefaultRowSpacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Paifang", meta=(DisplayName="Column Taper (收分)", UIMin="0", UIMax="0.02", ClampMin="0", ClampMax="0.05", ToolTip="Taper of each column toward its top, as a fraction of its height."))
	double ColumnTaperRatio = HutongCanon::Module::ColumnTaperRatio;


	// Set by the detail level, not by anybody's hand — 筒瓦 here is a *rank* statement and must not become a checkbox.
	bool bPlainTileForDetail = false;
	// Set by the tool from the drag rect; not user-editable.
	double Length = 900.0;
	double Depth = 200.0;

	int32 GetBayCount() const { return BayCount == EHutongPaifangBays::One ? 1 : 3; }

	double GetColumnRadius() const { return FMath::Max(0.5 * ColumnDiameter, 4.0); }

	// The radius the columns are laid on, held down on a shallow plinth depth.
	double GetColumnRadiusFor(double PlanDepth) const
	{
		return FMath::Min(GetColumnRadius(), 0.4 * FMath::Max(PlanDepth, 1.0));
	}

	// Bay spacing through the shared helper: 明間 is wider here for the same reason it is on a
	// facade, and the plan reads its divisions out of this rather than spelling them out again.
	double GetBayBoundary(int32 Index, int32 Bays, double Span, double ColumnRadius) const
	{
		return HutongGen::BayBoundary(Index, Bays, Span, ColumnRadius, SideBayWidthRatio, Bays / 2);
	}
};

namespace HutongGen
{
	void BuildPaifang(UE::Geometry::FDynamicMesh3& Mesh, const FHutongPaifangParams& P);
}
