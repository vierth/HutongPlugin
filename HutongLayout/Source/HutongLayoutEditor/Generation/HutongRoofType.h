#pragma once

#include "CoreMinimal.h"
#include "HutongRoofType.generated.h"

// Which of the three hipped-family roofs a building carries.
UENUM(BlueprintType)
enum class EHutongRoofType : uint8
{
	Cuanjian    UMETA(DisplayName = "Pyramidal Roof (攢尖)", ToolTip="Four hips meeting at a point, with no ridge."),

	Wudian      UMETA(DisplayName = "Hipped Roof (廡殿)", ToolTip="Four slopes meeting at a straight ridge."),

	Xieshan     UMETA(DisplayName = "Hip-and-Gable Roof (歇山)", ToolTip="A gable standing on a hipped lower skirt."),
};
