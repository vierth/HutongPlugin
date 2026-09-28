#pragma once

#include "CoreMinimal.h"
#include "HutongMetadata.generated.h"

// How far a placement is evidence rather than guess: 5 is attested, 1 only fills the block.
// The values are the scale, so `Confidence >= Probable` reads as written. Zero is required by a
// reflected enum; zero-initialised fields read "unknown", not a confident "not present".
UENUM(BlueprintType)
enum class EHutongConfidence : uint8
{
	// No answer recorded; not offered in the picker.
	Unknown = 0 UMETA(Hidden),

	// Not in any source.
	Absent = 1 UMETA(DisplayName = "1 · Not present"),

	// A plausible guess, not in any source.
	Conjectural = 2 UMETA(DisplayName = "2 · Conjectural"),

	// Inferred from the surroundings.
	Inferred = 3 UMETA(DisplayName = "3 · Inferred"),

	// On the map; extent or type read into it.
	Probable = 4 UMETA(DisplayName = "4 · Probable"),

	// Drawn on the map as placed.
	Attested = 5 UMETA(DisplayName = "5 · Attested on the map"),
};
