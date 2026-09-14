#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"

struct FHutongSiheyuanParams;

namespace HutongPresets
{
	// One row of HutongCanon::House as params. The single place the canon's house table is turned
	// into a building, so a compound's copy of a type and the preset a hand placement loads are
	// the same numbers.
	FHutongSiheyuanParams MakeHouse(const HutongCanon::House::FHouse& House);

	// Registers the presets the plugin ships.
	void RegisterBuiltInPresets();

	// The names RegisterBuiltInPresets registered for the siheyuan, in the order it added them.
	const TArray<FString>& BuiltInSiheyuanNames();
}
