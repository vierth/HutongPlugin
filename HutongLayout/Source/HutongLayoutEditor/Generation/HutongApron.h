#pragma once

#include "CoreMinimal.h"
#include "HutongApron.generated.h"

// 散水: the paved band round the foot of a building, catching what the roof sheds and carrying it clear of the footing.
USTRUCT(BlueprintType)
struct FHutongApronParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Apron", meta=(DisplayName="Apron Paving (散水)", ToolTip="Builds a paved apron (散水) round the foot of the building."))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Apron", meta=(DisplayName="Derive Width From Eave", EditCondition="bEnabled", ToolTip="Derives the apron's width from the roof overhang."))
	bool bDeriveWidth = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Apron", meta=(DisplayName="Reach Past Drip Line", EditCondition="bEnabled && bDeriveWidth", UIMin="0", UIMax="0.4", ClampMin="0", ClampMax="1", ToolTip="Reach of the apron past the drip line, as a fraction of the overhang."))
	double DripMargin = 0.12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Apron", meta=(DisplayName="Apron Width", EditCondition="bEnabled && !bDeriveWidth", UIMin="30", UIMax="140", ClampMin="10", Units="cm", ToolTip="Width of the apron, in cm."))
	double Width = 62.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Apron", meta=(DisplayName="Height At The Wall", EditCondition="bEnabled", UIMin="3", UIMax="15", ClampMin="1", Units="cm", ToolTip="Height of the apron at the wall, in cm."))
	double Thickness = 6.0;

	// The band this roof overhang asks for. Pass the building's own eave projection.
	double GetWidth(double RoofOverhang) const
	{
		if (!bDeriveWidth) return FMath::Max(Width, 0.0);
		return FMath::Max(RoofOverhang, 0.0) * (1.0 + FMath::Max(DripMargin, 0.0));
	}
};
