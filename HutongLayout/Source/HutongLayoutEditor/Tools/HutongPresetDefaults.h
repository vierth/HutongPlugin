#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "Generation/CompoundLayout.h"

struct FHutongSiheyuanParams;
struct FHutongFrameParams;

namespace HutongPresets
{
	// One row of HutongCanon::House as params. The single place the canon's house table is turned
	// into a building, so a compound's copy of a type and the preset a hand placement loads are
	// the same numbers.
	FHutongSiheyuanParams MakeHouse(const HutongCanon::House::FHouse& House);

	// The frame of one row of the house table, both eaves out so the rafters show front and back.
	FHutongFrameParams MakeFrame(const HutongCanon::House::FHouse& House);

	// The three court sizes, by the compound's own enum.
	const HutongCanon::Courtyard::FSize& CourtSize(EHutongCompoundSize Size);

	// The buildings of a court of that size: 正房 in whichever frame it carries, its 耳房, its 廂房.
	FHutongSiheyuanParams MakeCourtHall(const HutongCanon::Courtyard::FSize& Size, bool bRearVeranda);
	FHutongSiheyuanParams MakeCourtEarRoom(const HutongCanon::Courtyard::FSize& Size);
	FHutongSiheyuanParams MakeCourtWing(const HutongCanon::Courtyard::FSize& Size);

	// Registers the presets the plugin ships.
	void RegisterBuiltInPresets();

	// The names RegisterBuiltInPresets registered for the siheyuan, in the order it added them.
	const TArray<FString>& BuiltInSiheyuanNames();

	// The house the tool opens on when no preset has been picked yet: the side house, two to
	// every courtyard and so the most numerous roof on a 1750 hutong.
	const FString& DefaultSiheyuanName();

	// The frame the frame tool opens on: 圖5-3-1's 七檁前後廊 正房.
	const FString& DefaultFrameName();

	// The house a street row opens on: the front row, the one that lines the lane.
	const FString& DefaultStreetRowHouseName();
}
