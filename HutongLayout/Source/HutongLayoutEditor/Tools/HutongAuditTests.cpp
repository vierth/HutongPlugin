#include "Tools/HutongAudit.h"
#include "Tools/HutongPresetDefaults.h"
#include "Generation/HutongBuildingComponent.h"

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongAuditRolesTest,
	"HutongLayout.Audit.Roles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongAuditRolesTest::RunTest(const FString& Parameters)
{
	auto House = [](const FString& Preset, double Frontage, double Depth)
	{
		UHutongSiheyuanBuildingComponent* B = NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
		B->ApplyPresetParams(Preset);
		B->Preset = Preset;
		B->FootprintX = Frontage > 0.0 ? Frontage : B->Params.SuggestedFrontage;
		B->FootprintY = Depth > 0.0 ? Depth : B->Params.GetCanonicalDepth();
		return B;
	};
	auto Failing = [](const HutongAudit::FResult& R)
	{
		TArray<FString> Off;
		for (const HutongAudit::FCheck& C : R.Checks) if (!C.Passes()) Off.Add(C.Describe());
		return FString::Join(Off, TEXT("; "));
	};

	for (const FString& Name : HutongPresets::BuiltInSiheyuanNames())
	{
		const UHutongSiheyuanBuildingComponent* B = House(Name, 0.0, 0.0);
		const HutongAudit::FResult R = HutongAudit::Audit(B, EHutongTracedOutline::Walls);
		if (!TestTrue(FString::Printf(TEXT("%s is judged (role %s)"), *Name, *R.Role), R.IsJudged())) continue;
		TestEqual(FString::Printf(TEXT("%s at its own size is within the figures: %s"), *Name, *Failing(R)), R.Failing(), 0);
	}

	// A 廂房 traced 1.2 m deeper than the page's 3.5–4 m.
	const FString Side = TEXT("Side House, Small Court (廂房 五檁無廊)");
	UHutongSiheyuanBuildingComponent* Deep = House(Side, 0.0, 520.0);
	const HutongAudit::FResult OnWalls = HutongAudit::Audit(Deep, EHutongTracedOutline::Walls);
	const HutongAudit::FCheck* DepthCheck = OnWalls.Checks.FindByPredicate([](const HutongAudit::FCheck& C) { return C.Name.StartsWith(TEXT("depth")); });
	if (TestNotNull(TEXT("depth is checked"), DepthCheck))
	{
		TestEqual(TEXT("and found 1.2 m over"), DepthCheck->Off(), 120.0, 0.5);
	}
	TestTrue(TEXT("the readout heads with the role and says what is off"),
		HutongAudit::ReadoutLines(OnWalls).Num() >= 2 && HutongAudit::ReadoutLines(OnWalls)[0].Contains(TEXT("off")));
	const HutongAudit::FResult OnEaves = HutongAudit::Audit(Deep, EHutongTracedOutline::Eaves);
	const HutongAudit::FCheck* EavesDepth = OnEaves.Checks.FindByPredicate([](const HutongAudit::FCheck& C) { return C.Name.StartsWith(TEXT("depth")); });
	if (TestNotNull(TEXT("depth is checked on the eaves too"), EavesDepth))
	{
		TestTrue(FString::Printf(TEXT("traced on the eaves, the overhang comes off (%.0f cm)"), EavesDepth->Value), EavesDepth->Value < 520.0 - 20.0);
	}

	UHutongUnknownBuildingComponent* Traced = NewObject<UHutongUnknownBuildingComponent>(GetTransientPackage());
	Traced->CourtRole = EHutongCourtRole::SideHouse;
	Traced->SetFootprintSize(FVector2D(900.0, 550.0));
	const HutongAudit::FResult Unknown = HutongAudit::Audit(Traced, EHutongTracedOutline::Walls);
	TestTrue(TEXT("an Unknown with a role is judged"), Unknown.IsJudged());
	TestEqual(FString::Printf(TEXT("as the 廂房 with its 外廊, which 5.5 m is: %s"), *Failing(Unknown)), Unknown.Failing(), 0);
	TestTrue(TEXT("and told which purlin counts fit"), Unknown.Notes.ContainsByPredicate([](const FString& N) { return N.Contains(TEXT("purlins")); }));

	{
		const UHutongSiheyuanBuildingComponent* Fits = House(Side, 0.0, 0.0);
		const TArray<HutongAudit::FAudited> Set = { { Fits, HutongAudit::Audit(Fits, EHutongTracedOutline::Walls) },
			{ Deep, OnWalls } };
		const FString Table = HutongAudit::SummaryTable(Set);
		TestTrue(FString::Printf(TEXT("the summary counts the deep one outside on depth:\n%s"), *Table),
			Table.Contains(TEXT("depth (進深)")) && Table.Contains(TEXT("1 of 2 outside, by a median +1.20 m")));
		const FString Csv = FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("HutongAudit.csv"));
		FString Text;
		TestTrue(TEXT("the CSV writes"), HutongAudit::WriteCsv(Set, Csv) && FFileHelper::LoadFileToString(Text, *Csv));
		TestTrue(TEXT("a header and a row per check"), Text.StartsWith(TEXT("building,id,role,check")) && Text.Contains(TEXT("\"depth (進深)\",520.0")));
		IFileManager::Get().Delete(*Csv);
	}

	Traced->CourtRole = EHutongCourtRole::Other;
	TestFalse(TEXT("no role, nothing to judge"), HutongAudit::Audit(Traced, EHutongTracedOutline::Walls).IsJudged());
	return true;
}

#endif
