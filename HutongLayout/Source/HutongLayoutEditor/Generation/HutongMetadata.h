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

// What a building is in its courtyard unit, for the heights tool's suggestions. Auto reads it from
// the building's type and preset.
UENUM(BlueprintType)
enum class EHutongCourtRole : uint8
{
	Auto UMETA(DisplayName = "Auto (from type and preset)"),
	MainHall UMETA(DisplayName = "Main Hall (正房)"),
	SideHouse UMETA(DisplayName = "Side House (廂房)"),
	EarRoom UMETA(DisplayName = "Ear Room (耳房)"),
	FrontRow UMETA(DisplayName = "Front Row (倒座房)"),
	RearRow UMETA(DisplayName = "Rear Row (後罩房)"),
	Gate UMETA(DisplayName = "Gate (大門)"),
	InnerGate UMETA(DisplayName = "Inner Gate (垂花門)"),
	Corridor UMETA(DisplayName = "Covered Corridor (遊廊)"),
	LaneWall UMETA(DisplayName = "Lane Wall (院牆)"),
	CourtWall UMETA(DisplayName = "Court Wall (隔牆)"),
	Other UMETA(DisplayName = "Other"),
};
