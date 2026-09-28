#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "Generation/CompoundLayout.h"

struct FHutongSiheyuanParams;
struct FHutongFrameParams;

namespace HutongPresets
{
	// One row of HutongCanon::House as params: the one place the house table becomes a building,
	// so compound pieces and hand-placed presets share numbers.
	FHutongSiheyuanParams MakeHouse(const HutongCanon::House::FHouse& House);

	// Frame of one house-table row, both eaves out so rafters show front and back.
	FHutongFrameParams MakeFrame(const HutongCanon::House::FHouse& House);

	// The three court sizes, by the compound's own enum.
	const HutongCanon::Courtyard::FSize& CourtSize(EHutongCompoundSize Size);

	// The buildings of a court of that size: 正房 in whichever frame it carries, its 耳房, its 廂房.
	FHutongSiheyuanParams MakeCourtHall(const HutongCanon::Courtyard::FSize& Size, bool bRearVeranda);
	FHutongSiheyuanParams MakeCourtEarRoom(const HutongCanon::Courtyard::FSize& Size);
	FHutongSiheyuanParams MakeCourtWing(const HutongCanon::Courtyard::FSize& Size);

	// Registers the presets the plugin ships.
	void RegisterBuiltInPresets();

	// Siheyuan preset names RegisterBuiltInPresets registered, in order.
	const TArray<FString>& BuiltInSiheyuanNames();

	// Default house: the side house, two per courtyard, the most numerous roof on a 1750 hutong.
	const FString& DefaultSiheyuanName();

	// Frame tool default: 圖5-3-1's 七檁前後廊 正房.
	const FString& DefaultFrameName();

	// Street row default: the front row, lining the lane.
	const FString& DefaultStreetRowHouseName();
}
