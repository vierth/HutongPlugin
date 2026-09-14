#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "FlowerBedGenerator.generated.h"

// 花池: a low kerb of brick or stone retaining earth above the swept ground.
USTRUCT(BlueprintType)
struct FHutongFlowerBedParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bed", meta=(DisplayName="Kerb Height", UIMin="8", UIMax="60", ClampMin="3", Units="cm", ToolTip="Height of the kerb above the ground, in cm."))
	double KerbHeight = 26.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bed", meta=(DisplayName="Kerb Width", UIMin="8", UIMax="40", ClampMin="4", Units="cm", ToolTip="Thickness of the kerb, in cm."))
	double KerbWidth = 14.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bed", meta=(DisplayName="Soil Below Kerb", UIMin="2", UIMax="20", ClampMin="1", Units="cm", ToolTip="How far the soil surface sits below the top of the kerb, in cm."))
	double SoilDrop = 6.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bed", meta=(DisplayName="Stone Kerb", ToolTip="Builds the kerb in dressed stone instead of brick."))
	bool bStoneKerb = false;

	// Set by the tool from the drag rect; not user-editable.
	double SizeX = 200.0;
	double SizeY = 140.0;
};

namespace HutongGen
{
	void BuildFlowerBed(UE::Geometry::FDynamicMesh3& Mesh, const FHutongFlowerBedParams& P);
}
