#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "HutongDoorStone.generated.h"

// 門墩: the stone at the foot of each gate jamb.
UENUM()
enum class EHutongDoorStone : uint8
{
	Block UMETA(DisplayName = "Block Door Stone (方門墩)", ToolTip = "A plain rectangular block."),

	Drum UMETA(DisplayName = "Drum Door Stone (抱鼓石)", ToolTip = "A round drum standing on a plinth."),
};

// Everything a pair of 門墩 needs, in one struct because three gates want the same object.
USTRUCT(BlueprintType)
struct FHutongDoorStoneParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door Stones", meta=(DisplayName="Has Door Stones (門墩)", ToolTip="Adds a door stone (門墩) at the foot of each jamb."))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door Stones", meta=(DisplayName="Style", EditCondition="bEnabled", ToolTip="Form of the door stones."))
	EHutongDoorStone Style = EHutongDoorStone::Block;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door Stones", meta=(DisplayName="Block Height (方門墩)", EditCondition="bEnabled && Style == EHutongDoorStone::Block", UIMin="25", UIMax="90", ClampMin="10", Units="cm", ToolTip="Height of a block door stone, in cm."))
	double BlockHeight = HutongCanon::Stone::BlockHeightCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door Stones", meta=(DisplayName="Drum Height (抱鼓石)", EditCondition="bEnabled && Style == EHutongDoorStone::Drum", UIMin="70", UIMax="130", ClampMin="30", Units="cm", ToolTip="Height of a drum door stone including its plinth, in cm."))
	double DrumHeight = HutongCanon::Stone::DrumHeightCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door Stones", meta=(DisplayName="Drum Fraction", EditCondition="bEnabled && Style == EHutongDoorStone::Drum", UIMin="0.3", UIMax="0.85", ClampMin="0.2", ClampMax="0.9", ToolTip="Drum diameter as a fraction of the stone's height."))
	double DrumFraction = HutongCanon::Stone::DrumFraction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door Stones", meta=(DisplayName="Forward Projection", EditCondition="bEnabled", UIMin="0", UIMax="70", ClampMin="0", Units="cm", ToolTip="How far each stone projects forward of the door plane, in cm."))
	double Projection = HutongCanon::Stone::ProjectionCm;

	double GetHeight() const
	{
		return (Style == EHutongDoorStone::Drum) ? DrumHeight : BlockHeight;
	}
};
