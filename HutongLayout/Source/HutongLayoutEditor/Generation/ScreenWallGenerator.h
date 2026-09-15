#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongJiajia.h"
#include "ScreenWallGenerator.generated.h"

// 影壁: the screen facing a gate, freestanding a few paces inside (獨立影壁) or built against the wall across from it (座山影壁).
USTRUCT(BlueprintType)
struct FHutongScreenWallParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Screen", meta=(UIMin="150", UIMax="400", ClampMin="60", Units="cm", ToolTip="Height of the screen to its eave, in cm."))
	double Height = 260.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Screen", meta=(UIMin="15", UIMax="90", ClampMin="8", Units="cm", ToolTip="Thickness of the screen wall, in cm."))
	double Thickness = 38.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Screen", meta=(DisplayName="Plinth Height (須彌座)", UIMin="0", UIMax="90", ClampMin="0", Units="cm", ToolTip="Height of the moulded plinth (須彌座) under the screen, in cm."))
	double PlinthHeight = 46.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Screen", meta=(DisplayName="Plinth Projection", EditCondition="PlinthHeight > 0", UIMin="2", UIMax="30", ClampMin="0", Units="cm", ToolTip="How far the plinth stands out from the wall face on each side, in cm."))
	double PlinthProjection = HutongCanon::Screen::PlinthProjectionCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Screen", meta=(DisplayName="Base Course Height (下鹼)", UIMin="0", UIMax="150", ClampMin="0", Units="cm", ToolTip="Height of the base course (下鹼) above the plinth, in cm; zero derives it."))
	double BaseCourseHeight = 0.0;

	double GetBaseCourseHeight() const
	{
		return BaseCourseHeight > 0.0 ? BaseCourseHeight : FMath::Max(HutongCanon::BaseCourse::TopCm - FMath::Max(PlinthHeight, 0.0), 25.0);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Screen", meta=(DisplayName="Base Course Projection", UIMin="0", UIMax="15", ClampMin="0", Units="cm", ToolTip="How far the base course stands proud of the wall face, in cm."))
	double BaseCourseProjection = 3.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Panel", meta=(DisplayName="Has Centre Panel (影壁心)", ToolTip="Adds a bordered screen panel (影壁心) to both faces of the screen."))
	bool bHasPanel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Panel", meta=(DisplayName="Border Width", EditCondition="bHasPanel", UIMin="0.05", UIMax="0.3", ClampMin="0.02", ClampMax="0.45", ToolTip="Width of the panel's border as a fraction of the face's shorter side."))
	double BorderFraction = 0.14;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Panel", meta=(DisplayName="Border Projection", EditCondition="bHasPanel", UIMin="1", UIMax="12", ClampMin="0", Units="cm", ToolTip="How far the panel's border stands proud of the wall face, in cm."))
	double BorderProjection = 5.0;

	// --- Roof ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="10", UIMax="90", ClampMin="0", Units="cm", ToolTip="How far the eaves overhang the wall faces, in cm."))
	double RoofOverhang = 34.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Gable Overhang (懸山)", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="How far the roof extends past each end of the wall, in cm."))
	double GableOverhang = 20.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(UIMin="15", UIMax="120", ClampMin="5", Units="cm", ToolTip="Rise of the roof from eave to ridge, in cm."))
	double RoofRise = 42.0;

	// The roof's eave is the body's top; the floor used to be the generator's alone.
	double GetEaveHeight() const { return FMath::Max(Height, 40.0); }
	double GetRoofRise() const { return FMath::Max(RoofRise, 5.0); }


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Apex Roll (捲棚)", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="Rounding of the ridge into a rolled ridge (捲棚); zero keeps it sharp."))
	double RoofApexRoll = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Has Ridge Course (清水脊)", ToolTip="Adds a plain tile ridge (清水脊) course along the top of the roof."))
	bool bHasRidgeCourse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge Course Height", EditCondition="bHasRidgeCourse", UIMin="0", UIMax="40", Units="cm", ToolTip="Height of the ridge course, in cm."))
	double RidgeCourseHeight = 14.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge Course Width", EditCondition="bHasRidgeCourse", UIMin="8", UIMax="60", Units="cm", ToolTip="Width of the ridge course, in cm."))
	double RidgeCourseWidth = 22.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Ridge End Kick (蠍子尾)", EditCondition="bHasRidgeCourse", UIMin="0", UIMax="40", Units="cm", ToolTip="Rise of each ridge-end tail (蠍子尾), in cm."))
	double RidgeEndKick = 17.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Depth", UIMin="0", UIMax="20", ClampMin="0", Units="cm", ToolTip="Vertical depth of the fascia board along the eave, in cm."))
	double EaveFasciaDepth = 7.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Eave Fascia Width", UIMin="4", UIMax="30", ClampMin="1", Units="cm", ToolTip="Horizontal thickness of the fascia board along the eave, in cm."))
	double EaveFasciaWidth = 11.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Section (椽頭)", UIMin="0", UIMax="14", ClampMin="0", Units="cm", ToolTip="Section size of each exposed rafter end (椽頭), in cm; zero omits them."))
	double RafterEndSection = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Roof", meta=(DisplayName="Rafter End Spacing", EditCondition="RafterEndSection > 0", UIMin="8", UIMax="40", ClampMin="4", Units="cm", ToolTip="Spacing between rafter ends along the eave, in cm."))
	double RafterEndSpacing = 16.0;

	// Set by the tool from the drag rect; not user-editable.
	double Length = 400.0;

	// Total depth on the ground, which the drag's collapsed axis has to contain: the plinth is the widest thing on it.
	double GetFootprintDepth() const
	{
		return FMath::Max(Thickness, 1.0) + 2.0 * FMath::Max(PlinthProjection, 0.0);
	}
};

namespace HutongGen
{
	void BuildScreenWall(UE::Geometry::FDynamicMesh3& Mesh, const FHutongScreenWallParams& P);
}
