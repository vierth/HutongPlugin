#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongDoorStone.h"
#include "Generation/HutongDoor.h"
#include "InnerGateGenerator.generated.h"

// 垂花門: the inner gate, where the family's private courtyard begins.
USTRUCT(BlueprintType)
struct FHutongInnerGateParams
{
	GENERATED_BODY()

	FHutongInnerGateParams()
	{
		// Small and close in: an inner gate's stones are not the household's public face, and the shared struct's defaults are a street gate's.
		DoorStones.BlockHeight = HutongCanon::Stone::InnerGateHeightCm;
		DoorStones.Projection = HutongCanon::Stone::InnerGateProjectionCm;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Constrain To Historical Size", ToolTip="Holds the footprint and eave height within the size band of an inner gate (垂花門)."))
	bool bConstrainToHistoricalSize = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(UIMin="240", UIMax="420", ClampMin="120", Units="cm", ToolTip="Height of the eave above the ground, in cm."))
	double EaveHeight = 310.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Floor Height (臺基)", UIMin="0", UIMax="70", ClampMin="0", Units="cm", ToolTip="Height of the platform (臺基) above the ground, in cm."))
	double FloorHeight = 32.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Platform Overhang", UIMin="0", UIMax="80", ClampMin="0", Units="cm", ToolTip="How far the platform projects past the column line on the front and back, in cm."))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hanging Posts", meta=(DisplayName="Has Hanging Posts (垂蓮柱)", ToolTip="Builds the hanging lotus posts (垂蓮柱) off the ends of the beam, front and back."))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Front", meta=(DisplayName="Bracket Reach", EditCondition="bHasBrackets", UIMin="15", UIMax="80", ClampMin="5", Units="cm", ToolTip="How far each sparrow brace (雀替) reaches along the beam from its post, in cm."))
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(DisplayName="Door Leaves Open", ToolTip="Folds both door leaves open flat; off shuts them across the doorway."))
	bool bDoorLeavesOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(DisplayName="Door Pegs (門簪)", UIMin="0", UIMax="4", ClampMin="0", ClampMax="6", ToolTip="Number of door pegs (門簪) across the door head."))
	int32 DoorPegCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(DisplayName="Door Stones (門枕石)", ToolTip="Settings for the door pivot stones (門枕石) at the foot of each jamb."))
	FHutongDoorStoneParams DoorStones;

	// --- Roof ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="20", UIMax="140", ClampMin="0", Units="cm", ToolTip="How far the eave projects past the column line, in cm."))
	double RoofOverhang = 56.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Gable Overhang (懸山)", UIMin="0", UIMax="90", ClampMin="0", Units="cm", ToolTip="How far the roof extends past each gable end, in cm."))
	double GableOverhang = 30.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="30", UIMax="200", ClampMin="10", Units="cm", ToolTip="Height of the ridge above the eave, in cm."))
	double RoofRise = 82.0;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Apex Roll (捲棚)", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="Rounds the roof apex into a rolled ridge (捲棚) crown; 0 keeps a sharp fold."))
	double RoofApexRoll = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Has Ridge Course (正脊)", ToolTip="Builds a main ridge (正脊) course along the roof apex."))
	bool bHasRidgeCourse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(EditCondition="bHasRidgeCourse", UIMin="5", UIMax="50", ClampMin="1", Units="cm", ToolTip="Height of the ridge course above the roof surface, in cm."))
	double RidgeCourseHeight = 13.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(EditCondition="bHasRidgeCourse", UIMin="5", UIMax="50", ClampMin="1", Units="cm", ToolTip="Width of the ridge course, in cm."))
	double RidgeCourseWidth = 13.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge End Kick (蠍子尾)", EditCondition="bHasRidgeCourse", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="How far the ridge-end tail (蠍子尾) rises at each end of the ridge, in cm."))
	double RidgeEndKick = 16.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Depth", UIMin="0", UIMax="25", ClampMin="0", Units="cm", ToolTip="Depth of the fascia board along each eave, in cm."))
	double EaveFasciaDepth = 9.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Width", UIMin="4", UIMax="30", ClampMin="1", Units="cm", ToolTip="Width of the fascia board along each eave, in cm."))
	double EaveFasciaWidth = 13.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Section (椽頭)", UIMin="0", UIMax="18", ClampMin="0", Units="cm", ToolTip="Size of each square rafter end (椽頭) under the eave, in cm; 0 builds none."))
	double RafterEndSection = 7.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Spacing", EditCondition="RafterEndSection > 0", UIMin="10", UIMax="50", ClampMin="4", Units="cm", ToolTip="Spacing between rafter ends along the eave, in cm."))
	double RafterEndSpacing = 22.0;

	// Set by the tool from the drag rect; not user-editable.
	double Width = 330.0;
	double Depth = 140.0;

	double GetColumnRadius() const { return FMath::Max(0.5 * ColumnDiameter, 4.0); }
	double GetColumnHeight() const { return FMath::Max(GetEaveHeight() - FloorHeight, 1.0); }

	// Same rule and same floor as every other doorway here.
	double GetDoorHeadHeight() const
	{
		return FMath::Max(DoorHeadHeight, HutongGen::Passage::MinHeadZ(FloorHeight, ThresholdHeight));
	}

	// The 擔梁 rides above the head, so the eave has to clear the whole stack.
	double GetMinEaveHeight() const
	{
		const double PT = FMath::Max(2.0 * GetColumnRadius() * 0.8, 8.0);
		// Never under the 45 cm the tool holds back from the eave, or that clamp pulls the head back down through the clearance this was raised to give it.
		return GetDoorHeadHeight()
			+ FMath::Max(FMath::Max(DoorFrameThickness, 2.0) + 1.2 * PT, 45.0);
	}

	// The 80 was the generator's own floor, which nothing outside it could see.
	double GetEaveHeight() const { return FMath::Max3(EaveHeight, GetMinEaveHeight(), 80.0); }
	double GetRoofRise() const { return FMath::Max(RoofRise, 10.0); }

	// The band the type occurs at. All zero when the constraint is off.
	// The same band the gate house uses, from the canon.
	using FSizeRange = HutongCanon::Gate::FSizeBand;

	// UNVERIFIED in the same way as the gate house's bands, so widen them rather than trusting the centimetres.
	FSizeRange GetSizeRange() const
	{
		FSizeRange R;
		if (!bConstrainToHistoricalSize)
		{
			return R;
		}
		// The depth is the 擔梁's full run with the columns halfway.
		R = HutongCanon::Gate::InnerGateSize;
		return R;
	}
};

namespace HutongGen
{
	void BuildInnerGate(UE::Geometry::FDynamicMesh3& Mesh, const FHutongInnerGateParams& P);
}
