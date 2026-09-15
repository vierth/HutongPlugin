#pragma once

#include "CoreMinimal.h"
#include "HutongMetadata.generated.h"

// How far a placement is evidence and how far it is a guess. A plan traced off the 乾隆京城全圖
// is a mixture: some polygons are drawn on the sheet, some are the pattern of the block carried
// across a gap in it, and some are there because a street with a hole in it reads worse than a
// street with a guess in it. Which is which is a fact about the placement that nothing in the
// geometry can answer for afterwards, so the polygon carries it.
//
// The numbers are the scale, and they are what a reader outside the editor sees: 5 is attested,
// 1 is nothing but the need to fill the block.
// The values *are* the scale, so `Confidence >= Probable` reads as it looks; hence a zero entry
// that is not one of the five. It is hidden from the picker and exists because a reflected enum
// must have one — anything that zero-initialises the field lands on "unknown" rather than on
// "not present", which would be a confident wrong answer.
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
