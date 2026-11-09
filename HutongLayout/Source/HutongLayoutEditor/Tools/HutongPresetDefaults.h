#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "Generation/CompoundLayout.h"
#include "Generation/HutongMetadata.h"

struct FHutongSiheyuanParams;
struct FHutongFrameParams;

namespace HutongPresets
{
	// The one place the house table becomes a building, so compound pieces and presets share numbers.
	FHutongSiheyuanParams MakeHouse(const HutongCanon::House::FHouse& House);

	FHutongFrameParams MakeFrame(const HutongCanon::House::FHouse& House);

	const HutongCanon::Courtyard::FSize& CourtSize(EHutongCompoundSize Size);

	FHutongSiheyuanParams MakeCourtHall(const HutongCanon::Courtyard::FSize& Size, bool bRearVeranda);
	FHutongSiheyuanParams MakeCourtEarRoom(const HutongCanon::Courtyard::FSize& Size);
	FHutongSiheyuanParams MakeCourtWing(const HutongCanon::Courtyard::FSize& Size);

	void RegisterBuiltInPresets();

	const TArray<FString>& BuiltInSiheyuanNames();

	const TArray<FString>& BuiltInEarRoomNames();

	const FString& DefaultEarRoomName();

	// Default house: the side house, two per courtyard, the most numerous roof on a 1750 hutong.
	const FString& DefaultSiheyuanName();

	// Frame tool default: 圖5-3-1's 七檁前後廊 正房.
	const FString& DefaultFrameName();

	const FString& DefaultStreetRowHouseName();

	// Derived from the house table, so its ranking and the heights tool agree; negative for roles without one.
	double EaveRatio(EHutongCourtRole Role);

	// A role's suggested eave, from a reference building's role and eave; negative if either has no ratio.
	double SuggestEave(EHutongCourtRole Role, EHutongCourtRole ReferenceRole, double ReferenceEave);
}
