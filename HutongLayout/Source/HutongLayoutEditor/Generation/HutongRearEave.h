#pragma once

#include "CoreMinimal.h"
#include "HutongRearEave.generated.h"

// What stands behind the building, which is the only thing deciding what its back eave looks like.
UENUM()
enum class EHutongRearEave : uint8
{
	Lane UMETA(DisplayName = "Sealed Rear Eave (封護檐) — backs onto a lane", ToolTip="The roof stops at the back wall; the brickwork rises past the eave."),

	Courtyard UMETA(DisplayName = "Courtyard — eaves symmetrical", ToolTip="The rear eave overhangs, dressed like the facade."),
};
