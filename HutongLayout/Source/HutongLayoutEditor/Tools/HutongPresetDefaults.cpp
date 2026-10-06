#include "Tools/HutongPresetDefaults.h"
#include "Generation/EarPassageGenerator.h"
#include "Generation/FrameGenerator.h"
#include "Tools/HutongPresets.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/PavilionGenerator.h"
#include "Generation/HallGenerator.h"
#include "Generation/WallGenerator.h"
#include "Generation/GateHouseGenerator.h"
#include "Generation/HutongCanon.h"
#include "Generation/SmallBuildingGenerator.h"

// Courtyard building types, as presets rather than tools.
FHutongSiheyuanParams HutongPresets::MakeHouse(const HutongCanon::House::FHouse& H)
{
	FHutongSiheyuanParams P;
	// Fallback when derivation is off; normally eave = 檐柱高 = 8/10 明間面闊 off the frontage.
	P.EaveHeight = H.EaveCm;
	P.MinBayWidth = H.MinBayCm;
	P.MaxBayWidth = H.MaxBayCm;
	P.bHasFrontVeranda = H.bVeranda;
	P.RearEave = H.RearEave;
	P.SuggestedFrontage = H.FrontageCm;
	P.TypeFrontage = P.SuggestedFrontage;
	// 進深 unset: it is (檁數 - 1) x 步架 and the drag snaps to that.
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
	case EHutongCompoundSize::Standard: return Standard;
	default:                          return Large;
	}
}

// 明間/次間 are the court's; bay limits force three bays at that frontage. 檐柱高, 柱徑, 進深 derive as usual.
FHutongSiheyuanParams HutongPresets::MakeCourtHall(const HutongCanon::Courtyard::FSize& Size, bool bRearVeranda)
{
	FHutongSiheyuanParams P = MakeHouse(bRearVeranda
		? HutongCanon::House::MainHall : HutongCanon::House::MainHallSmall);
	P.SuggestedFrontage = Size.HallCentralBayCm + 2.0 * Size.HallSideBayCm;
	P.TypeFrontage = P.SuggestedFrontage;
	P.SideBayWidthRatio = Size.HallSideBayCm / Size.HallCentralBayCm;
	P.MinBayWidth = 0.9 * Size.HallSideBayCm;
	P.MaxBayWidth = 1.05 * Size.HallCentralBayCm;
	return P;
}

FHutongSiheyuanParams HutongPresets::MakeCourtEarRoom(const HutongCanon::Courtyard::FSize& Size)
{
	FHutongSiheyuanParams P = MakeHouse(HutongCanon::House::EarRoom);
	P.SuggestedFrontage = Size.EarRoomBayCm * FMath::Max(Size.EarRoomsPerFlank, 1);
	P.TypeFrontage = P.SuggestedFrontage;
	P.MinBayWidth = 0.85 * Size.EarRoomBayCm;
	P.MaxBayWidth = 1.1 * Size.EarRoomBayCm;
	return P;
}

FHutongSiheyuanParams HutongPresets::MakeCourtWing(const HutongCanon::Courtyard::FSize& Size)
{
	// 進深 = (檁數 - 1) × 步架; the court's walk decides the 廊 (HutongCompound::CourtWing).
	FHutongSiheyuanParams P = MakeHouse(HutongCanon::House::SideHouseSmall);
	P.StepRun = Size.WingStepRunCm;
	// 三間 at the hall's 次間: bay limits force exactly that frontage split.
	const int32 Bays = FMath::Max(Size.WingBays, 1);
	P.SuggestedFrontage = Bays * Size.HallSideBayCm;
	P.TypeFrontage = P.SuggestedFrontage;
	P.MinBayWidth = 0.9 * Size.HallSideBayCm;
	P.MaxBayWidth = 1.1 * Size.HallSideBayCm;
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
	TArray<FString> RegisteredEarRoomNames;

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
	// 正房: north main hall, the only type with a 前廊. 七檁前後廊 in large/medium compounds, 前廊後無廊 in small.
	Add(TEXT("Main Hall (正房)"), HutongCanon::House::MainHall);
	Add(TEXT("Main Hall, Five Bays (五間正房)"), HutongCanon::House::MainHallFiveBay);
	Add(TEXT("Main Hall, Small Court (正房 前廊後無廊)"), HutongCanon::House::MainHallSmall);

	// 廂房: east and west side houses.
	Add(TEXT("Side House (廂房)"), HutongCanon::House::SideHouse);
	Add(TEXT("Side House, Small Court (廂房 五檁無廊)"), HutongCanon::House::SideHouseSmall);

	// 倒座房: south row, front to the courtyard, back as lane wall.
	Add(TEXT("Front Row (倒座房)"), HutongCanon::House::FrontRow);

	// 後罩房: row behind the 正房, closing the plot.
	Add(TEXT("Rear Row (後罩房)"), HutongCanon::House::RearRow);
	Add(TEXT("Rear Row, Blank Back Wall (後罩房 無後窗)"), HutongCanon::House::RearRowBlankBack);

	// 構架: the 正房's frame alone, each form.
	for (const TPair<const TCHAR*, HutongCanon::House::FHouse>& Frame : {
			TPair<const TCHAR*, HutongCanon::House::FHouse>(TEXT("Main Hall (正房 七檁前後廊)"), HutongCanon::House::MainHall),
			TPair<const TCHAR*, HutongCanon::House::FHouse>(TEXT("Main Hall, Five Bays (五間正房 七檁前後廊)"), HutongCanon::House::MainHallFiveBay),
			TPair<const TCHAR*, HutongCanon::House::FHouse>(TEXT("Main Hall, Small Court (正房 前廊後無廊)"), HutongCanon::House::MainHallSmall) })
	{
		const FHutongFrameParams P = MakeFrame(Frame.Value);
		UHutongPresetLibrary::RegisterBuiltIn(TEXT("Frame"), Frame.Key, FHutongFrameParams::StaticStruct(), &P);
	}

	// 大門: one per style, so the gate's rank is a preset pick (tool, Details, heights window).
	for (const EHutongGateStyle Style : { EHutongGateStyle::Guangliang, EHutongGateStyle::Jinzhu, EHutongGateStyle::Manzi, EHutongGateStyle::Ruyi })
	{
		FHutongGateHouseParams P;
		P.Style = Style;
		UHutongPresetLibrary::RegisterBuiltIn(TEXT("GateHouse"),
			StaticEnum<EHutongGateStyle>()->GetDisplayNameTextByValue((int64)Style).ToString(), FHutongGateHouseParams::StaticStruct(), &P);
	}

	// 小房: minor freestanding buildings. PROVISIONAL — general knowledge, to be shaped on the user's figures.
	{
		FHutongSmallBuildingParams House;
		UHutongPresetLibrary::RegisterBuiltIn(TEXT("Small"), TEXT("Small House (小房)"), FHutongSmallBuildingParams::StaticStruct(), &House);

		// 堆撥房: the watch post at a lane mouth or bridgehead, door to the street.
		FHutongSmallBuildingParams Post;
		Post.EaveHeight = 280.0;
		Post.MinBayWidth = 240.0;
		UHutongPresetLibrary::RegisterBuiltIn(TEXT("Small"), TEXT("Guard Post (堆撥房)"), FHutongSmallBuildingParams::StaticStruct(), &Post);

		// 棚: a lean-to, open at the front.
		FHutongSmallBuildingParams Shed;
		Shed.Roof = EHutongSmallRoof::LeanTo;
		Shed.Front = EHutongSmallFront::Open;
		Shed.EaveHeight = 230.0;
		Shed.FloorHeight = 0.0;
		Shed.WallThickness = 24.0;
		Shed.RoofRise = 90.0;
		Shed.bHasTileRuns = false;
		UHutongPresetLibrary::RegisterBuiltIn(TEXT("Small"), TEXT("Shed (棚)"), FHutongSmallBuildingParams::StaticStruct(), &Shed);

		// 土地廟: a one-bay wayside shrine on a raised base.
		FHutongSmallBuildingParams Shrine;
		Shrine.EaveHeight = 210.0;
		Shrine.FloorHeight = 45.0;
		Shrine.WallThickness = 24.0;
		Shrine.DoorWidth = 60.0;
		Shrine.bWindows = false;
		Shrine.RoofRise = 80.0;
		Shrine.RoofOverhang = 35.0;
		Shrine.MinBayWidth = 120.0;
		Shrine.MaxBayWidth = 400.0;
		UHutongPresetLibrary::RegisterBuiltIn(TEXT("Small"), TEXT("Shrine (土地廟)"), FHutongSmallBuildingParams::StaticStruct(), &Shrine);
	}

	// 亭: the 則例's two, the round one with its 倒掛楣子 as drawn.
	{
		FHutongPavilionParams Square;
		UHutongPresetLibrary::RegisterBuiltIn(TEXT("Pavilion"), TEXT("Square Pavilion (四角方亭)"), FHutongPavilionParams::StaticStruct(), &Square);
		FHutongPavilionParams Round;
		Round.Plan = EHutongPavilionPlan::Round;
		Round.bHasFrieze = true;
		UHutongPresetLibrary::RegisterBuiltIn(TEXT("Pavilion"), TEXT("Round Pavilion (六柱圓亭)"), FHutongPavilionParams::StaticStruct(), &Round);
	}

	// 殿: the small lane temple at the struct's defaults, and the 則例 卷二 大式 hall. The small one was
	// left unregistered, so the picker offered the grand hall alone.
	{
		const FHutongHallParams Small;
		UHutongPresetLibrary::RegisterBuiltIn(TEXT("Hall"), TEXT("Small Temple (小式 歇山殿)"), FHutongHallParams::StaticStruct(), &Small);
		FHutongHallParams Grand;
		Grand.Style = EHutongHallStyle::Grand;
		Grand.RoofTile = EHutongRoofTile::Tong;
		UHutongPresetLibrary::RegisterBuiltIn(TEXT("Hall"), TEXT("Grand Hall (大式 九檁歇山殿)"), FHutongHallParams::StaticStruct(), &Grand);
	}

	// 耳房: low ear rooms against the 正房 flanks, on their own tool. Plain with a front door; with the
	// compound's 過道 (struct defaults); shut to the court, entered from the hall beside it.
	{
		FHutongEarPassageParams Plain;
		Plain.Passageway = EHutongEarPassage::None;
		FHutongEarPassageParams WithPassage;
		FHutongEarPassageParams Shut = Plain;
		Shut.Room.bHasFrontDoorCenter = false;
		for (const TPair<FString, const FHutongEarPassageParams*>& Ear : {
				TPair<FString, const FHutongEarPassageParams*>(TEXT("Ear Room (耳房)"), &Plain),
				TPair<FString, const FHutongEarPassageParams*>(TEXT("Ear Room With Passage (耳房過道)"), &WithPassage),
				TPair<FString, const FHutongEarPassageParams*>(TEXT("Ear Room, No Front Door (耳房 無門)"), &Shut) })
		{
			UHutongPresetLibrary::RegisterBuiltIn(TEXT("EarPassage"), Ear.Key, FHutongEarPassageParams::StaticStruct(), Ear.Value);
			RegisteredEarRoomNames.AddUnique(Ear.Key);
		}
	}
}

const TArray<FString>& HutongPresets::BuiltInSiheyuanNames()
{
	return RegisteredSiheyuanNames;
}

const TArray<FString>& HutongPresets::BuiltInEarRoomNames()
{
	return RegisteredEarRoomNames;
}

const FString& HutongPresets::DefaultEarRoomName()
{
	static const FString Name = TEXT("Ear Room (耳房)");
	return Name;
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

namespace
{
	double BuiltEave(const HutongCanon::House::FHouse& House)
	{
		FHutongSiheyuanParams P = HutongPresets::MakeHouse(House);
		P.Width = P.SuggestedFrontage;
		return P.GetEaveHeight();
	}
}

double HutongPresets::EaveRatio(EHutongCourtRole Role)
{
	using namespace HutongCanon::House;
	const HutongCanon::House::FHouse* House = nullptr;
	switch (Role)
	{
	case EHutongCourtRole::MainHall:  return 1.0;
	case EHutongCourtRole::SideHouse: House = &SideHouse; break;
	case EHutongCourtRole::EarRoom:   House = &EarRoom; break;
	case EHutongCourtRole::FrontRow:  House = &FrontRow; break;
	case EHutongCourtRole::RearRow:   House = &RearRow; break;
	case EHutongCourtRole::LaneWall:
	case EHutongCourtRole::CourtWall:
		break;
	default: return -1.0;
	}
	static const double Hall = BuiltEave(MainHallSmall);
	if (House) return BuiltEave(*House) / Hall;
	// A wall's body top as its role derives it (院牆 level with the wings, 隔牆 lower so a 垂花門 clears it).
	FHutongWallParams Wall;
	Wall.bDeriveFromRole = true;
	Wall.Role = Role == EHutongCourtRole::LaneWall ? EHutongWallRole::Perimeter : EHutongWallRole::Courtyard;
	return Wall.GetHeight() / Hall;
}

double HutongPresets::SuggestEave(EHutongCourtRole Role, EHutongCourtRole ReferenceRole, double ReferenceEave)
{
	const double Mine = EaveRatio(Role), Ref = EaveRatio(ReferenceRole);
	return (Mine > 0.0 && Ref > 0.0) ? ReferenceEave * Mine / Ref : -1.0;
}
