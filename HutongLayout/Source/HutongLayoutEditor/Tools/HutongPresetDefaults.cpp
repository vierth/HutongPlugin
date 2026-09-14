#include "Tools/HutongPresetDefaults.h"
#include "Generation/EarPassageGenerator.h"
#include "Tools/HutongPresets.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/HutongCanon.h"

// The building types a courtyard is made of, as presets rather than as tools.
FHutongSiheyuanParams HutongPresets::MakeHouse(const HutongCanon::House::FHouse& H)
{
	FHutongSiheyuanParams P;
	// A fallback for the derivation being turned off; the eave these build at comes from
	// 檐柱高 = 8/10 明間面闊 off the frontage.
	P.EaveHeight = H.EaveCm;
	P.MinBayWidth = H.MinBayCm;
	P.MaxBayWidth = H.MaxBayCm;
	P.bHasFrontVeranda = H.bVeranda;
	P.RearEave = H.RearEave;
	P.SuggestedFrontage = H.FrontageCm;
	// 進深 is not given. It is (檁數 - 1) x 步架, and the drag snaps to whatever that comes to.
	P.SuggestedDepth = 0.0;
	P.Purlins = H.Purlins;
	P.StepRun = H.StepRunCm;
	P.bHasRearHighWindows = H.bRearHighWindows;
	return P;
}

namespace
{
	TArray<FString> RegisteredSiheyuanNames;

	void Add(const FString& Name, const HutongCanon::House::FHouse& House)
	{
		const FHutongSiheyuanParams P = HutongPresets::MakeHouse(House);
		UHutongPresetLibrary::RegisterBuiltIn(
			TEXT("Siheyuan"), Name, FHutongSiheyuanParams::StaticStruct(), &P);
		RegisteredSiheyuanNames.AddUnique(Name);
	}
}

void HutongPresets::RegisterBuiltInPresets()
{
	// 正房: the main hall on the north side, and the only one of the five that gets a 前廊.
	Add(TEXT("Main Hall (正房)"), HutongCanon::House::MainHall);

	// 廂房: the side houses down the east and west of the courtyard.
	Add(TEXT("Side House (廂房)"), HutongCanon::House::SideHouse);

	// 倒座房: the row along the south, whose front faces the courtyard and whose back is the lane wall.
	Add(TEXT("Front Row (倒座房)"), HutongCanon::House::FrontRow);

	// 後罩房: the row behind the 正房, closing the back of the plot.
	Add(TEXT("Rear Row (後罩房)"), HutongCanon::House::RearRow);

	// 耳房: the low "ear" rooms tucked against the flanks of the 正房.
	Add(TEXT("Ear Room (耳房)"), HutongCanon::House::EarRoom);

	// The same 耳房 with the compound's 過道 beside it: one built-in, at the struct's own defaults,
	// so the picker has a name to show and a tuned copy something to be saved over.
	{
		const FHutongEarPassageParams P;
		UHutongPresetLibrary::RegisterBuiltIn(
			TEXT("EarPassage"), TEXT("Ear Room With Passage (耳房過道)"), FHutongEarPassageParams::StaticStruct(), &P);
	}
}

const TArray<FString>& HutongPresets::BuiltInSiheyuanNames()
{
	return RegisteredSiheyuanNames;
}
