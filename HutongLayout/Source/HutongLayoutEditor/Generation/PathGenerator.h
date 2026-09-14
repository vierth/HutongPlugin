#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "PathGenerator.generated.h"

// 甬路: the raised brick walk from the gate to the steps of the 正房.
USTRUCT(BlueprintType)
struct FHutongPathParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Path", meta=(DisplayName="Min Width", UIMin="60", UIMax="150", ClampMin="40", Units="cm", ToolTip="Smallest clear paving width the drag can set, in cm."))
	double WidthMin = 90.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Path", meta=(DisplayName="Max Width", UIMin="120", UIMax="400", ClampMin="60", Units="cm", ToolTip="Largest clear paving width the drag can set, in cm."))
	double WidthMax = 220.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Path", meta=(DisplayName="Rise", UIMin="2", UIMax="30", ClampMin="0", Units="cm", ToolTip="Height of the paving above the surrounding ground, in cm."))
	double Rise = 9.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Path", meta=(DisplayName="Has Kerb (牙子石)", ToolTip="Builds kerb stones (牙子石) along each edge of the path."))
	bool bHasKerb = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Path", meta=(DisplayName="Kerb Width", EditCondition="bHasKerb", UIMin="5", UIMax="30", ClampMin="2", Units="cm", ToolTip="Width of the kerb along each edge, in cm."))
	double KerbWidth = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Path", meta=(DisplayName="Kerb Lip", EditCondition="bHasKerb", UIMin="1", UIMax="12", ClampMin="0", Units="cm", ToolTip="How far the kerb's top stands above the paving, in cm."))
	double KerbLip = 3.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Path", meta=(DisplayName="Course Spacing", UIMin="0", UIMax="200", ClampMin="0", Units="cm", ToolTip="Spacing between cross joints along the path, in cm; 0 builds none."))
	double CourseSpacing = 60.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Path", meta=(DisplayName="Joint Width", EditCondition="CourseSpacing > 0", UIMin="1", UIMax="10", ClampMin="0", Units="cm", ToolTip="Width of each cross joint cut into the paving, in cm."))
	double JointWidth = 3.0;

	// Set by the tool from the drag rect; not user-editable.
	double Length = 600.0;
	double Width = 130.0;

	double GetKerbWidth() const { return bHasKerb ? FMath::Max(KerbWidth, 0.0) : 0.0; }

	double GetFootprintDepth() const { return FMath::Max(Width, 1.0) + 2.0 * GetKerbWidth(); }

	// Clear paving width for a dragged footprint depth, held inside the band.
	double WidthFromFootprint(double FootprintDepth) const
	{
		const double Lo = FMath::Max(WidthMin, 10.0);
		const double Hi = FMath::Max(WidthMax, Lo + 1.0);
		return FMath::Clamp(FootprintDepth - 2.0 * GetKerbWidth(), Lo, Hi);
	}
};

namespace HutongGen
{
	void BuildPath(UE::Geometry::FDynamicMesh3& Mesh, const FHutongPathParams& P);
}
