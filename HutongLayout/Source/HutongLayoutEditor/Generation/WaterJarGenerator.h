#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "WaterJarGenerator.generated.h"

// 魚缸: the water jar on its 缸座, standing on the axis before the 正房 — the middle term of 天棚魚缸石榴樹.
USTRUCT(BlueprintType)
struct FHutongWaterJarParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jar", meta=(DisplayName="Jar Height", UIMin="35", UIMax="120", ClampMin="15", Units="cm", ToolTip="Height of the jar excluding its base, in cm."))
	double Height = 62.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jar", meta=(DisplayName="Belly Diameter", UIMin="40", UIMax="140", ClampMin="20", Units="cm", ToolTip="Diameter of the jar at its widest point, in cm."))
	double BellyDiameter = 78.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jar", meta=(DisplayName="Foot / Belly", UIMin="0.4", UIMax="0.9", ClampMin="0.2", ClampMax="1", ToolTip="Diameter of the jar's foot as a fraction of the belly diameter."))
	double FootFraction = 0.58;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jar", meta=(DisplayName="Mouth / Belly", UIMin="0.5", UIMax="0.95", ClampMin="0.3", ClampMax="1", ToolTip="Diameter of the jar's mouth as a fraction of the belly diameter."))
	double MouthFraction = 0.80;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jar", meta=(DisplayName="Belly Height", UIMin="0.25", UIMax="0.7", ClampMin="0.1", ClampMax="0.9", ToolTip="Height of the widest point as a fraction of the jar's height."))
	double BellyFraction = 0.42;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Jar", meta=(DisplayName="Sides", UIMin="8", UIMax="48", ClampMin="6", ClampMax="96", ToolTip="Number of sides round the jar's circumference."))
	int32 Sides = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Base", meta=(DisplayName="Has Stone Base (缸座)", ToolTip="Adds a square stone base (缸座) under the jar."))
	bool bHasBase = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Base", meta=(DisplayName="Base Height", EditCondition="bHasBase", UIMin="4", UIMax="40", ClampMin="1", Units="cm", ToolTip="Height of the stone base, in cm."))
	double BaseHeight = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Base", meta=(DisplayName="Base Margin", EditCondition="bHasBase", UIMin="0", UIMax="30", ClampMin="0", Units="cm", ToolTip="How far the base extends past the jar's foot on each side, in cm."))
	double BaseMargin = 7.0;

	// The footprint this jar occupies, which is what the drag is held square at.
	double GetFootprint() const
	{
		const double Belly = FMath::Max(BellyDiameter, 1.0);
		if (!bHasBase) return Belly;
		const double Foot = Belly * FMath::Clamp(FootFraction, 0.1, 1.0);
		return FMath::Max(Belly, Foot + 2.0 * FMath::Max(BaseMargin, 0.0));
	}
};

namespace HutongGen
{
	void BuildWaterJar(UE::Geometry::FDynamicMesh3& Mesh, const FHutongWaterJarParams& P);
}
