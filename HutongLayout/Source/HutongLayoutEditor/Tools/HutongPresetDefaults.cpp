#include "Tools/HutongPresetDefaults.h"
#include "Generation/EarPassageGenerator.h"
#include "Generation/FrameGenerator.h"
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
	P.bHasRearVeranda = H.bRearVeranda;
	P.SideBayWidthRatio = H.SideBayRatio;
	P.ColumnHeightPerBay = H.ColumnPerCentralBay;
	return P;
}

const HutongCanon::Courtyard::FSize& HutongPresets::CourtSize(EHutongCompoundSize Size)
{
	using namespace HutongCanon::Courtyard;
	switch (Size)
	{
	case EHutongCompoundSize::Small:  return Small;
	case EHutongCompoundSize::Medium: return Medium;
	default:                          return Large;
	}
}

// 明間 and 次間 are the court's, so the bay limits are set to give three bays at that frontage
// whatever the widths are; everything above them — 檐柱高, 柱徑, 進深 — follows as it always does.
FHutongSiheyuanParams HutongPresets::MakeCourtHall(const HutongCanon::Courtyard::FSize& Size, bool bRearVeranda)
{
	FHutongSiheyuanParams P = MakeHouse(bRearVeranda
		? HutongCanon::House::MainHall : HutongCanon::House::MainHallSmall);
	P.SuggestedFrontage = Size.HallCentralBayCm + 2.0 * Size.HallSideBayCm;
	P.SideBayWidthRatio = Size.HallSideBayCm / Size.HallCentralBayCm;
	P.MinBayWidth = 0.9 * Size.HallSideBayCm;
	P.MaxBayWidth = 1.05 * Size.HallCentralBayCm;
	return P;
}

FHutongSiheyuanParams HutongPresets::MakeCourtEarRoom(const HutongCanon::Courtyard::FSize& Size)
{
	FHutongSiheyuanParams P = MakeHouse(HutongCanon::House::EarRoom);
	P.SuggestedFrontage = Size.EarRoomBayCm * FMath::Max(Size.EarRoomsPerFlank, 1);
	P.MinBayWidth = 0.85 * Size.EarRoomBayCm;
	P.MaxBayWidth = 1.1 * Size.EarRoomBayCm;
	return P;
}

FHutongSiheyuanParams HutongPresets::MakeCourtWing(const HutongCanon::Courtyard::FSize& Size)
{
	// 進深 is (檁數 - 1) × 步架; a 廊 is added by the court's walk, and takes one more 步架.
	FHutongSiheyuanParams P = MakeHouse(HutongCanon::House::SideHouse);
	P.StepRun = Size.WingStepRunCm;
	return P;
}

FHutongFrameParams HutongPresets::MakeFrame(const HutongCanon::House::FHouse& House)
{
	FHutongFrameParams P;
	P.House = MakeHouse(House);
	P.House.RearEave = EHutongRearEave::Courtyard;
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
	// 正房: the main hall on the north side, and the only type that gets a 前廊. 七檁前後廊 in a
	// large or medium compound, 前廊後無廊 in a small one.
	Add(TEXT("Main Hall (正房)"), HutongCanon::House::MainHall);
	Add(TEXT("Main Hall, Five Bays (五間正房)"), HutongCanon::House::MainHallFiveBay);
	Add(TEXT("Main Hall, Small Court (正房 前廊後無廊)"), HutongCanon::House::MainHallSmall);

	// 廂房: the side houses down the east and west of the courtyard.
	Add(TEXT("Side House (廂房)"), HutongCanon::House::SideHouse);

	// 倒座房: the row along the south, whose front faces the courtyard and whose back is the lane wall.
	Add(TEXT("Front Row (倒座房)"), HutongCanon::House::FrontRow);

	// 後罩房: the row behind the 正房, closing the back of the plot.
	Add(TEXT("Rear Row (後罩房)"), HutongCanon::House::RearRow);

	// 耳房: the low "ear" rooms tucked against the flanks of the 正房.
	Add(TEXT("Ear Room (耳房)"), HutongCanon::House::EarRoom);

	// 構架: the 正房's frame alone, in each of its forms.
	for (const TPair<const TCHAR*, HutongCanon::House::FHouse>& Frame : {
			TPair<const TCHAR*, HutongCanon::House::FHouse>(TEXT("Main Hall (正房 七檁前後廊)"), HutongCanon::House::MainHall),
			TPair<const TCHAR*, HutongCanon::House::FHouse>(TEXT("Main Hall, Five Bays (五間正房 七檁前後廊)"), HutongCanon::House::MainHallFiveBay),
			TPair<const TCHAR*, HutongCanon::House::FHouse>(TEXT("Main Hall, Small Court (正房 前廊後無廊)"), HutongCanon::House::MainHallSmall) })
	{
		const FHutongFrameParams P = MakeFrame(Frame.Value);
		UHutongPresetLibrary::RegisterBuiltIn(TEXT("Frame"), Frame.Key, FHutongFrameParams::StaticStruct(), &P);
	}

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

const FString& HutongPresets::DefaultSiheyuanName()
{
	static const FString Name = TEXT("Side House (廂房)");
	return Name;
}

const FString& HutongPresets::DefaultFrameName()
{
	static const FString Name = TEXT("Main Hall (正房 七檁前後廊)");
	return Name;
}

const FString& HutongPresets::DefaultStreetRowHouseName()
{
	static const FString Name = TEXT("Front Row (倒座房)");
	return Name;
}
