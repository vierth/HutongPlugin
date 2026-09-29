#include "Misc/AutomationTest.h"
#include "Generation/HutongBuildingComponent.h"
#include "Tools/HutongPresetDefaults.h"
#include "Tools/HeightsTool.h"
#include "Tools/HutongCourts.h"
#include "Generation/HutongGateRow.h"
#include "UObject/Package.h"
#include "Engine/World.h"
#include "Generation/HutongActorSpawn.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongHeightsRanksTest,
	"HutongLayout.Heights.Ranks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongHeightsRanksTest::RunTest(const FString& Parameters)
{
	// The ranking the suggestions use is the house table's, 正房 above 廂房 above the low rows.
	using namespace HutongPresets;
	const double Side = EaveRatio(EHutongCourtRole::SideHouse);
	const double Front = EaveRatio(EHutongCourtRole::FrontRow);
	const double Rear = EaveRatio(EHutongCourtRole::RearRow);
	const double Ear = EaveRatio(EHutongCourtRole::EarRoom);
	AddInfo(FString::Printf(TEXT("廂房 %.3f, 倒座房 %.3f, 後罩房 %.3f, 耳房 %.3f of 正房"), Side, Front, Rear, Ear));
	TestEqual(TEXT("the 正房 is its own reference"), EaveRatio(EHutongCourtRole::MainHall), 1.0);
	TestTrue(TEXT("the 廂房 ranks below the 正房"), Side < 1.0 && Side > Front);
	TestTrue(TEXT("the low rows and the 耳房 rank below the 廂房"), Front > 0.0 && Rear > 0.0 && Ear > 0.0 && Ear < Side);
	TestTrue(TEXT("a gate has no ratio"), EaveRatio(EHutongCourtRole::Gate) < 0.0);
	const double Lane = EaveRatio(EHutongCourtRole::LaneWall), Court = EaveRatio(EHutongCourtRole::CourtWall);
	AddInfo(FString::Printf(TEXT("院牆 %.3f, 隔牆 %.3f of 正房"), Lane, Court));
	TestTrue(TEXT("the lane wall is under the 正房 and over the court wall"), Lane < 1.0 && Court > 0.0 && Lane > Court);
	TestTrue(TEXT("a suggestion scales the reference"),
		FMath::IsNearlyEqual(SuggestEave(EHutongCourtRole::EarRoom, EHutongCourtRole::MainHall, 400.0), 400.0 * Ear));
	TestTrue(TEXT("a suggestion from a 廂房 reference goes through the 正房 ratio"),
		FMath::IsNearlyEqual(SuggestEave(EHutongCourtRole::EarRoom, EHutongCourtRole::SideHouse, 300.0), 300.0 * Ear / Side));

	// The tool's reference is the highest rank selected, and it suggests itself.
	TArray<FHutongEaveRow> Rows;
	auto Row = [](EHutongCourtRole Role, double Eave) { FHutongEaveRow R; R.Role = Role; R.NewEave = Eave; return R; };
	Rows.Add(Row(EHutongCourtRole::EarRoom, 280.0));
	Rows.Add(Row(EHutongCourtRole::Gate, 420.0));
	Rows.Add(Row(EHutongCourtRole::MainHall, 364.0));
	Rows.Add(Row(EHutongCourtRole::SideHouse, 300.0));
	TestEqual(TEXT("the 正房 is the reference"), UHutongHeightsTool::ReferenceRow(Rows), 2);
	const TArray<double> S = UHutongHeightsTool::Suggestions(Rows);
	TestTrue(TEXT("the 耳房 is suggested off the 正房"), FMath::IsNearlyEqual(S[0], 364.0 * Ear));
	TestTrue(TEXT("the gate gets none"), S[1] < 0.0);
	TestEqual(TEXT("the reference suggests its own eave"), S[2], 364.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongHeightsSetEaveTest,
	"HutongLayout.Heights.SetEave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongHeightsSetEaveTest::RunTest(const FString& Parameters)
{
	// Every type the tool offers takes the eave it is given, as its own accessor answers it.
	const TArray<UClass*> Classes = {
		UHutongSiheyuanBuildingComponent::StaticClass(), UHutongEarPassageBuildingComponent::StaticClass(),
		UHutongGateHouseBuildingComponent::StaticClass(), UHutongCorridorBuildingComponent::StaticClass(),
		UHutongInnerGateBuildingComponent::StaticClass(), UHutongShopfrontBuildingComponent::StaticClass(),
		UHutongHallBuildingComponent::StaticClass(), UHutongWallBuildingComponent::StaticClass(),
		UHutongStoreyBuildingComponent::StaticClass(), UHutongScreenWallBuildingComponent::StaticClass(),
		UHutongFrameBuildingComponent::StaticClass() };
	for (UClass* Class : Classes)
	{
		UHutongBuildingComponent* B = NewObject<UHutongBuildingComponent>(GetTransientPackage(), Class);
		const FString Name = Class->GetName();
		if (!TestTrue(Name + TEXT(" can set its eave"), B->CanSetEditHeight())) continue;
		const double Target = FMath::Max(B->GetEditHeight() + 37.0, 330.0);
		TestTrue(Name + TEXT(" takes the height"), B->SetEditHeight(Target));
		TestTrue(FString::Printf(TEXT("%s answers it (%.1f)"), *Name, B->GetEditHeight()), FMath::IsNearlyEqual(B->GetEditHeight(), Target, 0.01));
	}

	// A house's derived eave is overridden, not kept.
	UHutongSiheyuanBuildingComponent* House = NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
	House->SetFootprintSize(FVector2D(1040.0, 600.0));
	const double Derived = House->GetEaveHeight();
	House->SetEditHeight(Derived - 40.0);
	TestTrue(TEXT("the house eave leaves its bay-derived figure"), FMath::IsNearlyEqual(House->GetEaveHeight(), Derived - 40.0, 0.01));
	TestFalse(TEXT("an unsettable type says so"), NewObject<UHutongPaifangBuildingComponent>(GetTransientPackage())->CanSetEditHeight());
	UHutongWallBuildingComponent* Wall = NewObject<UHutongWallBuildingComponent>(GetTransientPackage());
	TestTrue(TEXT("a wall's height is its body top, not an eave"), Wall->GetEaveHeight() == 0.0 && Wall->GetEditHeight() > 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongHeightsRoleTest,
	"HutongLayout.Courts.Role",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongHeightsRoleTest::RunTest(const FString& Parameters)
{
	UHutongSiheyuanBuildingComponent* House = NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
	TestEqual(TEXT("no preset, no label: other"), House->GetCourtRole(), EHutongCourtRole::Other);
	House->Preset = TEXT("Ear Room (耳房)");
	TestEqual(TEXT("the preset names the role"), House->GetCourtRole(), EHutongCourtRole::EarRoom);
	House->Preset = TEXT("Rear Row, Blank Back Wall (後罩房 無後窗)");
	TestEqual(TEXT("a preset variant keeps its role"), House->GetCourtRole(), EHutongCourtRole::RearRow);
	House->CourtRole = EHutongCourtRole::MainHall;
	TestEqual(TEXT("a stored role wins"), House->GetCourtRole(), EHutongCourtRole::MainHall);
	UHutongWallBuildingComponent* Wall = NewObject<UHutongWallBuildingComponent>(GetTransientPackage());
	Wall->Params.Role = EHutongWallRole::Courtyard;
	TestEqual(TEXT("a court wall by its role"), Wall->GetCourtRole(), EHutongCourtRole::CourtWall);
	// A wall stands between courts and keeps both; a house moves.
	Wall->AssignCourt(TEXT("3M6_Courtyard_1"));
	Wall->AssignCourt(TEXT("3M6_Courtyard_2"));
	TestTrue(TEXT("a wall is in both courts"), Wall->IsInCourt(TEXT("3M6_Courtyard_1")) && Wall->IsInCourt(TEXT("3M6_Courtyard_2")));
	House->AssignCourt(TEXT("3M6_Courtyard_1"));
	House->AssignCourt(TEXT("3M6_Courtyard_2"));
	TestEqual(TEXT("a house is in the last court only"), House->GetCourts(), TArray<FString>{ TEXT("3M6_Courtyard_2") });
	Wall->AssignCourt(FString());
	TestEqual(TEXT("empty clears a wall's courts"), Wall->GetCourts().Num(), 0);
	TestEqual(TEXT("an ear room with passage is an ear room"),
		NewObject<UHutongEarPassageBuildingComponent>(GetTransientPackage())->GetCourtRole(), EHutongCourtRole::EarRoom);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongHeightsCourtNameTest,
	"HutongLayout.Courts.NamesAndFolders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongHeightsCourtNameTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("the first on a tile"), HutongCourts::NextCourtName(TEXT("3M6_Courtyard"), {}), FString(TEXT("3M6_Courtyard_1")));
	TestEqual(TEXT("one past the highest, gaps kept"),
		HutongCourts::NextCourtName(TEXT("3M6_Courtyard"), { TEXT("3M6_Courtyard_1"), TEXT("3M6_Courtyard_4"), TEXT("3M7_Courtyard_9"), TEXT("3M6_Courtyard_x") }),
		FString(TEXT("3M6_Courtyard_5")));

	// A map tile is found by the label of the plane the point stands on; other actors are not tiles.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!TestNotNull(TEXT("the engine plane loads"), Plane)) { World->DestroyWorld(false); return false; }
	auto Place = [&](const TCHAR* Label, const FVector& At, double Scale)
	{
		AStaticMeshActor* A = World->SpawnActor<AStaticMeshActor>(At, FRotator::ZeroRotator);
		A->GetStaticMeshComponent()->SetStaticMesh(Plane);
		A->SetActorScale3D(FVector(Scale, Scale, 1.0));
		A->SetActorLabel(Label);
		return A;
	};
	Place(TEXT("3M6"), FVector(0.0, 0.0, 0.0), 100.0);
	Place(TEXT("Ground"), FVector(0.0, 0.0, 0.0), 300.0);
	Place(TEXT("3M7"), FVector(10000.0, 0.0, 0.0), 100.0);
	TestEqual(TEXT("the tile under the point"), HutongCourts::MapTileAt(World, FVector(1000.0, 500.0, 0.0)), FString(TEXT("3M6")));
	TestEqual(TEXT("the neighbouring tile"), HutongCourts::MapTileAt(World, FVector(10500.0, 0.0, 0.0)), FString(TEXT("3M7")));
	TestEqual(TEXT("off every tile"), HutongCourts::MapTileAt(World, FVector(0.0, 20000.0, 0.0)), FString());

	// Each building is filed by the tile under it and its court; a wall between courts under Shared walls.
	auto Building = [&](UClass* Class, const FVector& At, const TCHAR* Court) -> UHutongBuildingComponent*
	{
		AStaticMeshActor* Actor = HutongGen::SpawnEmptyActor(World, FTransform(At), TEXT("Filed"));
		UHutongBuildingComponent* B = NewObject<UHutongBuildingComponent>(Actor, Class);
		Actor->AddInstanceComponent(B);
		B->RegisterComponent();
		B->Court = Court;
		return B;
	};
	UHutongBuildingComponent* House = Building(UHutongSiheyuanBuildingComponent::StaticClass(), FVector(-2000.0, -2000.0, 0.0), TEXT("3M6_Courtyard_1"));
	UHutongBuildingComponent* Shared = Building(UHutongWallBuildingComponent::StaticClass(), FVector(-2000.0, 1000.0, 0.0), TEXT("3M6_Courtyard_1; 3M6_Courtyard_2"));
	UHutongBuildingComponent* Away = Building(UHutongSiheyuanBuildingComponent::StaticClass(), FVector(0.0, 30000.0, 0.0), TEXT("Courtyard_1"));
	UHutongBuildingComponent* Loose = Building(UHutongSiheyuanBuildingComponent::StaticClass(), FVector(-3000.0, -3000.0, 0.0), TEXT(""));
	TestEqual(TEXT("a house under its tile and court"), HutongCourts::FolderFor(World, House), FName(TEXT("Courts/3M6/3M6_Courtyard_1")));
	TestEqual(TEXT("a shared wall under the tile's Shared walls"), HutongCourts::FolderFor(World, Shared), FName(TEXT("Courts/3M6/Shared walls")));
	TestEqual(TEXT("off the map under Off map"), HutongCourts::FolderFor(World, Away), FName(TEXT("Courts/Off map/Courtyard_1")));
	TestEqual(TEXT("no court, no folder"), HutongCourts::FolderFor(World, Loose), FName(NAME_None));
	const FName LooseWas = Loose->GetOwner()->GetFolderPath();
	TestEqual(TEXT("three are filed"), HutongCourts::FileInFolders(World, { House, Shared, Away, Loose }), 3);
	TestEqual(TEXT("the house moved"), House->GetOwner()->GetFolderPath(), FName(TEXT("Courts/3M6/3M6_Courtyard_1")));
	TestEqual(TEXT("the building in no court stayed"), Loose->GetOwner()->GetFolderPath(), LooseWas);
	TestEqual(TEXT("every court in the level, sorted"), HutongCourts::AllCourts(World),
		TArray<FString>{ TEXT("3M6_Courtyard_1"), TEXT("3M6_Courtyard_2"), TEXT("Courtyard_1") });

	// The window's list: each court once, its tile, its buildings (a shared wall counts in both).
	const TArray<HutongCourts::FSummary> Summaries = HutongCourts::Summaries(World);
	const HutongCourts::FSummary* First = Summaries.FindByPredicate([](const HutongCourts::FSummary& S) { return S.Name == TEXT("3M6_Courtyard_1"); });
	const HutongCourts::FSummary* Away2 = Summaries.FindByPredicate([](const HutongCourts::FSummary& S) { return S.Name == TEXT("Courtyard_1"); });
	TestTrue(TEXT("a court lists its tile and both its buildings"), First && First->Tile == TEXT("3M6") && First->Buildings == 2);
	TestTrue(TEXT("a court off the map says so"), Away2 && Away2->Tile == HutongCourts::OffMapFolder);

	// Out of one court: the shared wall keeps the other and is filed under it.
	TestEqual(TEXT("the wall leaves one court"), HutongCourts::Remove({ Shared, Loose }, TEXT("3M6_Courtyard_2")), 1);
	TestEqual(TEXT("keeping the other"), Shared->GetCourts(), TArray<FString>{ TEXT("3M6_Courtyard_1") });
	TestEqual(TEXT("and is filed under it"), Shared->GetOwner()->GetFolderPath(), FName(TEXT("Courts/3M6/3M6_Courtyard_1")));

	// Out of its only court: out of the Courts folders too; a folder of the user's own is left alone.
	TestEqual(TEXT("the house leaves its court"), HutongCourts::Remove({ House }, TEXT("3M6_Courtyard_1")), 1);
	TestEqual(TEXT("and the Courts folders"), House->GetOwner()->GetFolderPath(), FName(NAME_None));
	Loose->GetOwner()->SetFolderPath(TEXT("Study/Traced"));
	HutongCourts::FileInFolders(World, { Loose });
	TestEqual(TEXT("a folder of the user's own stays"), Loose->GetOwner()->GetFolderPath(), FName(TEXT("Study/Traced")));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongHeightsGateTest,
	"HutongLayout.Heights.Gate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongHeightsGateTest::RunTest(const FString& Parameters)
{
	// The gate's eave for a ridge is solved both ways, so a suggestion can lower a gate too.
	FHutongGateHouseParams Gate;
	const double Depth = 600.0;
	const double Ridge = HutongGen::Ridge::Gate(Gate, Depth);
	for (const double Step : { 60.0, -30.0 })
	{
		const double Eave = HutongGen::GateRow::EaveForRidge(Gate, Depth, Ridge + Step);
		FHutongGateHouseParams At = Gate;
		At.EaveHeight = Eave;
		TestTrue(FString::Printf(TEXT("the ridge lands %+.0f cm off (%.2f)"), Step, HutongGen::Ridge::Gate(At, Depth) - Ridge),
			FMath::IsNearlyEqual(HutongGen::Ridge::Gate(At, Depth), Ridge + Step, 0.1));
	}

	// Each style is a preset, so the rank is picked where the height is set.
	UHutongGateHouseBuildingComponent* Placed = NewObject<UHutongGateHouseBuildingComponent>(GetTransientPackage());
	const TArray<FString> Options = Placed->GetPresetOptions();
	TestTrue(TEXT("the four gate styles are presets"),
		Options.ContainsByPredicate([](const FString& N) { return N.Contains(TEXT("廣亮大門")); })
		&& Options.ContainsByPredicate([](const FString& N) { return N.Contains(TEXT("如意門")); }));
	const FString* Ruyi = Options.FindByPredicate([](const FString& N) { return N.Contains(TEXT("如意門")); });
	if (Ruyi)
	{
		TestTrue(TEXT("a preset loads its style"), Placed->ApplyPresetParams(*Ruyi) && Placed->Params.Style == EHutongGateStyle::Ruyi);
		const double Want = Placed->GetRidgeHeight() + 50.0;
		TestTrue(TEXT("the component solves under a named preset"), Placed->EaveForRidge(Want, *Ruyi) > Placed->GetEaveHeight());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
