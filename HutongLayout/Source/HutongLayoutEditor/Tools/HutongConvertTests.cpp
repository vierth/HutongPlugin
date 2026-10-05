#include "Misc/AutomationTest.h"
#include "Tools/HutongOverlaps.h"
#include "Tools/HutongDetailOps.h"
#include "Tools/HutongPresets.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongPlanOutlineComponent.h"
#include "Generation/HutongActorSpawn.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"

#if WITH_DEV_AUTOMATION_TESTS

using UE::Geometry::FDynamicMesh3;

namespace
{
	// A placement of this type, as a drag would leave it.
	UHutongBuildingComponent* PlaceNamed(UWorld* World, UClass* Class, const FVector2D& Footprint,
		const FTransform& Xform, const TCHAR* NameBase)
	{
		UHutongBuildingComponent* Template = NewObject<UHutongBuildingComponent>(GetTransientPackage(), Class);
		Template->DetailLevel = EHutongDetail::Massing;
		Template->bBuildLODChain = false;
		Template->SetFootprintSize(Footprint);

		TArray<FDynamicMesh3> LODs;
		Template->BuildLODs(LODs);
		AStaticMeshActor* Actor = HutongGen::SpawnStaticMeshActor(World, LODs, Xform, NameBase, FHutongPalette());
		if (!Actor) return nullptr;

		UHutongBuildingComponent* B = NewObject<UHutongBuildingComponent>(
			Actor, Class, NAME_None, RF_Transactional);
		B->DetailLevel = EHutongDetail::Massing;
		B->bBuildLODChain = false;
		B->SetFootprintSize(Footprint);
		Actor->AddInstanceComponent(B);
		B->RegisterComponent();
		return B;
	}
}

// Conversion keeps placement decisions (position, footprint, facing, id); everything else comes from the new type.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongConvertAcrossTypesTest, "HutongLayout.Convert.AcrossTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongConvertAcrossTypesTest::RunTest(const FString& Parameters)
{
	// Every type offered, and both kinds of a two-kind class (the wall): a class-only list would miss one.
	const TArray<HutongDetailOps::FConvertTarget>& Targets = HutongDetailOps::ConvertTargets();
	TestTrue(TEXT("every building type is offered"), Targets.Num() >= 15);

	const HutongDetailOps::FConvertTarget House =
		HutongDetailOps::FindConvertTarget(TEXT("House (房)"));
	const HutongDetailOps::FConvertTarget LaneWall =
		HutongDetailOps::FindConvertTarget(TEXT("Lane Wall (院牆)"));
	const HutongDetailOps::FConvertTarget CourtWall =
		HutongDetailOps::FindConvertTarget(TEXT("Court Wall (隔牆)"));
	if (!TestTrue(TEXT("the house is a target"), House.IsValid())) return false;
	if (!TestTrue(TEXT("both walls are targets"), LaneWall.IsValid() && CourtWall.IsValid())) return false;
	TestEqual(TEXT("the two walls are one class"), LaneWall.Class, CourtWall.Class);

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	// 鋪面房 → 房: different generator and params struct.
	{
		const FVector2D Footprint(1120.0, 640.0);
		const FTransform Xform(FRotator(0.0, 23.0, 0.0), FVector(800.0, -200.0, 0.0));
		UHutongBuildingComponent* Shop = PlaceNamed(World, UHutongShopfrontBuildingComponent::StaticClass(),
			Footprint, Xform, TEXT("Convert"));
		if (!TestNotNull(TEXT("the shopfront places"), Shop)) { World->DestroyWorld(false); return false; }

		Shop->SetFacade(EHutongBaySide::PlusX);
		const FGuid Id = Shop->BuildingId;
		AActor* Actor = Shop->GetOwner();

		UHutongBuildingComponent* Built = HutongDetailOps::ConvertBuilding(Shop, House, FString());
		if (!TestNotNull(TEXT("the shopfront converts"), Built)) { World->DestroyWorld(false); return false; }

		TestTrue(TEXT("it is a house now"),
			Built->IsA(UHutongSiheyuanBuildingComponent::StaticClass()));
		TestNull(TEXT("and carries no second building component"),
			Actor->FindComponentByClass<UHutongShopfrontBuildingComponent>());

		TestEqual(TEXT("the footprint is the placement's"), Built->GetFootprintSize().X, Footprint.X, 0.01);
		TestEqual(TEXT("and so is its depth"), Built->GetFootprintSize().Y, Footprint.Y, 0.01);

		EHutongBaySide Facade = EHutongBaySide::MinusY;
		TestTrue(TEXT("the facade survives"), Built->GetFacade(Facade));
		TestEqual(TEXT("on the side it was placed facing"), (int32)Facade, (int32)EHutongBaySide::PlusX);

		TestEqual(TEXT("the same building keeps its id"), Built->BuildingId, Id);
		TestEqual(TEXT("and stands where it stood"),
			Actor->GetActorLocation().X, Xform.GetLocation().X, 0.01);
		TestTrue(TEXT("the label names what it is now"),
			Actor->GetActorLabel().Contains(TEXT("Siheyuan")));

		// Mesh is the new type's, not the old one left standing.
		const AStaticMeshActor* SMA = Cast<AStaticMeshActor>(Actor);
		TestTrue(TEXT("it is built again"),
			SMA && SMA->GetStaticMeshComponent() && SMA->GetStaticMeshComponent()->GetStaticMesh() != nullptr);
	}

	// 院牆 → 隔牆: one class, two kinds; conversion must pick.
	{
		UHutongBuildingComponent* Wall = PlaceNamed(World, LaneWall.Class, FVector2D(1400.0, 37.0),
			FTransform::Identity, TEXT("ConvertWall"));
		if (!TestNotNull(TEXT("the wall places"), Wall)) { World->DestroyWorld(false); return false; }
		Wall->SetTypeVariant(LaneWall.Variant);

		UHutongBuildingComponent* Court = HutongDetailOps::ConvertBuilding(Wall, CourtWall, FString());
		if (!TestNotNull(TEXT("the wall converts"), Court)) { World->DestroyWorld(false); return false; }
		TestEqual(TEXT("it is a 隔牆 now"), Court->GetTypeVariant(), CourtWall.Variant);
		TestEqual(TEXT("and says so"), Court->GetTypeLabel().ToString(), FString(TEXT("Court Wall (隔牆)")));
		TestEqual(TEXT("along the run it was drawn on"), Court->GetFootprintSize().X, 1400.0, 0.01);
	}

	World->DestroyWorld(false);
	return true;
}

// Undoing a conversion baked a default house onto a plan-only actor: the undone component reverts to
// defaults (bPlanOnly false) and rebuilt on its way out. One Ctrl+Z must restore the original building.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongConvertUndoTest, "HutongLayout.Convert.Undo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongConvertUndoTest::RunTest(const FString& Parameters)
{
	if (!GEditor || !GEditor->Trans) { AddError(TEXT("no transaction buffer to undo through")); return false; }

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	const HutongDetailOps::FConvertTarget House =
		HutongDetailOps::FindConvertTarget(TEXT("House (房)"));
	if (!TestTrue(TEXT("the house is a target"), House.IsValid())) { World->DestroyWorld(false); return false; }

	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const bool bPlanOnly = (Pass == 1);
		const TCHAR* What = bPlanOnly ? TEXT("a laid-out shopfront") : TEXT("a built shopfront");

		UHutongBuildingComponent* Shop = PlaceNamed(World, UHutongShopfrontBuildingComponent::StaticClass(),
			FVector2D(1120.0, 640.0), FTransform::Identity, TEXT("Undo"));
		if (!TestNotNull(TEXT("the shopfront places"), Shop)) { World->DestroyWorld(false); return false; }
		Shop->bPlanOnly = bPlanOnly;
		Shop->Rebuild();

		AActor* Actor = Shop->GetOwner();
		AStaticMeshActor* SMA = Cast<AStaticMeshActor>(Actor);

		GEditor->BeginTransaction(FText::FromString(TEXT("Convert")));
		HutongDetailOps::ConvertBuilding(Shop, House, FString());
		GEditor->EndTransaction();

		GEditor->UndoTransaction();

		TArray<UHutongBuildingComponent*> Buildings;
		Actor->GetComponents<UHutongBuildingComponent>(Buildings);
		TestEqual(FString::Printf(TEXT("%s carries one building component after the undo"), What),
			Buildings.Num(), 1);
		if (Buildings.Num() == 1)
		{
			TestTrue(FString::Printf(TEXT("%s is a shopfront again"), What),
				Buildings[0]->IsA(UHutongShopfrontBuildingComponent::StaticClass()));
		}

		const bool bHasMesh = SMA && SMA->GetStaticMeshComponent()
			&& SMA->GetStaticMeshComponent()->GetStaticMesh() != nullptr;
		if (bPlanOnly)
		{
			TestFalse(TEXT("a laid-out plan comes back with no geometry"), bHasMesh);
		}
		else
		{
			TestTrue(TEXT("a built one comes back built"), bHasMesh);
		}
	}

	World->DestroyWorld(false);
	return true;
}

// A plan-only building has one outline and resize moves it. An undo can leave a second; only the first
// was updated, the stray kept drawing the old footprint. Every outline must follow the resize.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongPlanOutlineUniqueTest, "HutongLayout.Detail.PlanOutlineUnique",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongPlanOutlineUniqueTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	const FVector2D Footprint(900.0, 600.0);
	UHutongBuildingComponent* B = PlaceNamed(World, UHutongSiheyuanBuildingComponent::StaticClass(),
		Footprint, FTransform::Identity, TEXT("PlanOutline"));
	if (!TestNotNull(TEXT("the house places"), B)) { World->DestroyWorld(false); return false; }

	AActor* Actor = B->GetOwner();
	B->bPlanOnly = true;
	B->ApplyPlanOutline();

	auto OutlinesOn = [](AActor* A)
	{
		TArray<UHutongPlanOutlineComponent*> Found;
		A->GetComponents(Found);
		return Found;
	};

	TArray<UHutongPlanOutlineComponent*> Outlines = OutlinesOn(Actor);
	if (!TestEqual(TEXT("one outline after the first apply"), Outlines.Num(), 1))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("it carries the footprint"), Outlines[0]->Footprint.X, Footprint.X);

	// Stray second outline, as an undo leaves it, at its original size.
	UHutongPlanOutlineComponent* Stray = NewObject<UHutongPlanOutlineComponent>(
		Actor, NAME_None, RF_Transactional);
	Stray->SetupAttachment(Actor->GetRootComponent());
	Actor->AddInstanceComponent(Stray);
	Stray->RegisterComponent();
	FHutongPlanBays NoBays;
	Stray->SetPlan(Footprint, FHutongFootprintSkew(), false, EHutongBaySide::MinusY, {}, false, NoBays, true,
		FLinearColor::White);
	TestEqual(TEXT("two outlines to start from"), OutlinesOn(Actor).Num(), 2);

	// Resize through the drag's accessor.
	const FVector2D Resized(400.0, 300.0);
	B->SetFootprintSize(Resized);
	B->ApplyPlanOutline();

	Outlines = OutlinesOn(Actor);
	if (!TestEqual(TEXT("the stray outline is gone"), Outlines.Num(), 1))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("the survivor followed the resize"), Outlines[0]->Footprint.X,
		B->GetFootprintSize().X);
	TestTrue(TEXT("and it is not the stray"), Outlines[0] != Stray);

	// Built again, outline removed: geometry shows the footprint.
	B->bPlanOnly = false;
	B->ApplyPlanOutline();
	TestEqual(TEXT("no outline on a built building"), OutlinesOn(Actor).Num(), 0);

	World->DestroyWorld(false);
	return true;
}

// Revert to plan: mesh off, outline on, params kept so regenerating gives the same building. Already
// plan-only buildings are skipped and not counted.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongRevertToPlanTest, "HutongLayout.Detail.RevertToPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongRevertToPlanTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	const FVector2D Footprint(1120.0, 640.0);
	UHutongBuildingComponent* Built = PlaceNamed(World, UHutongShopfrontBuildingComponent::StaticClass(),
		Footprint, FTransform::Identity, TEXT("Revert"));
	UHutongBuildingComponent* Plan = PlaceNamed(World, UHutongShopfrontBuildingComponent::StaticClass(),
		Footprint, FTransform::Identity, TEXT("RevertPlan"));
	if (!TestNotNull(TEXT("the built shopfront places"), Built)
		|| !TestNotNull(TEXT("the laid-out shopfront places"), Plan))
	{
		World->DestroyWorld(false);
		return false;
	}
	Plan->bPlanOnly = true;
	Plan->Rebuild();

	auto HasMesh = [](const UHutongBuildingComponent* B)
	{
		const AStaticMeshActor* SMA = Cast<AStaticMeshActor>(B->GetOwner());
		return SMA && SMA->GetStaticMeshComponent() && SMA->GetStaticMeshComponent()->GetStaticMesh() != nullptr;
	};
	auto OutlineCount = [](const UHutongBuildingComponent* B)
	{
		TArray<UHutongPlanOutlineComponent*> Found;
		B->GetOwner()->GetComponents(Found);
		return Found.Num();
	};

	TestTrue(TEXT("the built one starts with a mesh"), HasMesh(Built));
	TestEqual(TEXT("and no outline"), OutlineCount(Built), 0);

	const int32 Reverted = HutongDetailOps::RevertToPlan({ Built, Plan });
	TestEqual(TEXT("only the built one is reverted"), Reverted, 1);
	TestTrue(TEXT("it is laid out now"), Built->bPlanOnly);
	TestFalse(TEXT("its mesh is gone"), HasMesh(Built));
	TestEqual(TEXT("it draws one outline"), OutlineCount(Built), 1);
	TestEqual(TEXT("the footprint is kept"), Built->GetFootprintSize().X, Footprint.X);

	const int32 Generated = HutongDetailOps::GeneratePlanned({ Built });
	TestEqual(TEXT("generating brings it back"), Generated, 1);
	TestTrue(TEXT("with a mesh"), HasMesh(Built));
	TestEqual(TEXT("and no outline"), OutlineCount(Built), 0);

	World->DestroyWorld(false);
	return true;
}

// What P / T would throw away: nothing on a building as its preset laid it, the field a student
// moved once they move it, and every type found again by its own label.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongConvertCustomizedTest,
	"HutongLayout.Convert.Customized",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongConvertCustomizedTest::RunTest(const FString& Parameters)
{
	const TArray<HutongDetailOps::FConvertTarget>& Targets = HutongDetailOps::ConvertTargets();
	// Kin side by side for T: house, ear room, shop in a row at the head.
	auto At = [&](UClass* C) { return Targets.IndexOfByPredicate([C](const HutongDetailOps::FConvertTarget& T) { return T.Class == C; }); };
	TestEqual(TEXT("the house heads the types"), At(UHutongSiheyuanBuildingComponent::StaticClass()), 0);
	TestEqual(TEXT("the ear room next"), At(UHutongEarPassageBuildingComponent::StaticClass()), 1);
	TestEqual(TEXT("then the shop"), At(UHutongShopfrontBuildingComponent::StaticClass()), 2);
	auto CanSwap = [&](UClass* A, UClass* B) { return HutongDetailOps::CanExchange(Targets[At(A)], Targets[At(B)]); };
	TestTrue(TEXT("a house may become a gate"), CanSwap(UHutongSiheyuanBuildingComponent::StaticClass(), UHutongGateHouseBuildingComponent::StaticClass()));
	TestTrue(TEXT("a shop may become a pavilion"), CanSwap(UHutongShopfrontBuildingComponent::StaticClass(), UHutongPavilionBuildingComponent::StaticClass()));
	TestTrue(TEXT("a temple may become a house"), CanSwap(UHutongHallBuildingComponent::StaticClass(), UHutongSiheyuanBuildingComponent::StaticClass()));
	TestTrue(TEXT("a wall may become a screen wall"), CanSwap(UHutongWallBuildingComponent::StaticClass(), UHutongScreenWallBuildingComponent::StaticClass()));
	TestTrue(TEXT("a corridor may become a path"), CanSwap(UHutongCorridorBuildingComponent::StaticClass(), UHutongPathBuildingComponent::StaticClass()));
	TestTrue(TEXT("a flower bed may become a jar"), CanSwap(UHutongFlowerBedBuildingComponent::StaticClass(), UHutongWaterJarBuildingComponent::StaticClass()));
	TestFalse(TEXT("a house may not become a wall"), CanSwap(UHutongSiheyuanBuildingComponent::StaticClass(), UHutongWallBuildingComponent::StaticClass()));
	TestFalse(TEXT("a wall may not become a path"), CanSwap(UHutongWallBuildingComponent::StaticClass(), UHutongPathBuildingComponent::StaticClass()));
	TestFalse(TEXT("a path may not become a jar"), CanSwap(UHutongPathBuildingComponent::StaticClass(), UHutongWaterJarBuildingComponent::StaticClass()));
	TestFalse(TEXT("every type has a family"), Targets.ContainsByPredicate([](const HutongDetailOps::FConvertTarget& T) { return T.Family == INDEX_NONE; }));
	TestFalse(TEXT("every type has a group"), Targets.ContainsByPredicate([](const HutongDetailOps::FConvertTarget& T) { return T.Group == INDEX_NONE; }));
	for (int32 i = 0; i < Targets.Num(); ++i)
	{
		const HutongDetailOps::FConvertTarget& T = Targets[i];
		UHutongBuildingComponent* Fresh = NewObject<UHutongBuildingComponent>(GetTransientPackage(), T.Class, NAME_None, RF_Transient);
		if (!T.Variant.IsNone()) Fresh->SetTypeVariant(T.Variant);
		TestEqual(FString::Printf(TEXT("%s found by its own label"), *T.Label), HutongDetailOps::FindConvertTargetIndex(Fresh), i);
		if (!T.Variant.IsNone()) continue;
		TestEqual(FString::Printf(TEXT("%s: type defaults are not customized"), *T.Label), HutongDetailOps::CustomizedFields(Fresh).Num(), 0);
		for (const FString& Name : UHutongPresetLibrary::Get()->GetPresetNames(HutongDetailOps::PresetKeyOf(T)))
		{
			UHutongBuildingComponent* B = NewObject<UHutongBuildingComponent>(GetTransientPackage(), T.Class, NAME_None, RF_Transient);
			if (!B->ApplyPresetParams(Name)) continue;
			B->Preset = Name;
			const TArray<FString> Fields = HutongDetailOps::CustomizedFields(B);
			TestEqual(FString::Printf(TEXT("%s · %s as loaded is not customized (%s)"), *T.Label, *Name, *FString::Join(Fields, TEXT(", "))), Fields.Num(), 0);
		}
	}

	// A moved field is named, inside nested params too.
	{
		UHutongSiheyuanBuildingComponent* House = NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
		House->Preset = TEXT("Main Hall (正房)");
		House->ApplyPresetParams(House->Preset);
		House->Params.bHasFrontDoorCenter = !House->Params.bHasFrontDoorCenter;
		TestEqual(TEXT("one field moved, one named"), HutongDetailOps::CustomizedFields(House),
			TArray<FString>{ TEXT("Has Front Door") });
		// A preset that no longer exists measures against the type's defaults.
		House->Preset = TEXT("No Such Preset");
		TestTrue(TEXT("a vanished preset leaves the values standing as customized"), HutongDetailOps::CustomizedFields(House).Num() > 1);
	}
	{
		UHutongEarPassageBuildingComponent* Ear = NewObject<UHutongEarPassageBuildingComponent>(GetTransientPackage());
		Ear->Params.Room.EaveHeight += 20.0;
		const TArray<FString> Fields = HutongDetailOps::CustomizedFields(Ear);
		TestTrue(TEXT("a nested field is named under its group"), Fields.Num() == 1 && Fields[0].Contains(TEXT(" › ")));
	}

	// What the drag wrote is not a customization: a corridor's walk, a jar's belly, a snapped 下鹼.
	{
		UHutongCorridorBuildingComponent* Corridor = NewObject<UHutongCorridorBuildingComponent>(GetTransientPackage());
		Corridor->Params.Width = Corridor->Params.WalkWidthFromFootprint(190.0);
		Corridor->Width = Corridor->Params.Width;
		Corridor->Length = 900.0;
		TestEqual(TEXT("a dragged corridor walk is not customized"), HutongDetailOps::CustomizedFields(Corridor).Num(), 0);
		UHutongWaterJarBuildingComponent* Jar = NewObject<UHutongWaterJarBuildingComponent>(GetTransientPackage());
		Jar->Params.BellyDiameter = 97.0;
		TestEqual(TEXT("a dragged jar belly is not customized"), HutongDetailOps::CustomizedFields(Jar).Num(), 0);
		UHutongSiheyuanBuildingComponent* House = NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
		House->Preset = TEXT("Main Hall (正房)");
		House->ApplyPresetParams(House->Preset);
		House->SetBaseCourseTop(House->GetBaseCourseTop() + 12.0);
		TestEqual(TEXT("a neighbour's base course line is not customized"), HutongDetailOps::CustomizedFields(House).Num(), 0);

		// Another preset lands as if drawn: the line kept, nothing reads as customized after.
		const double Top = House->GetBaseCourseTop();
		TestTrue(TEXT("the side house preset applies"), HutongDetailOps::ApplyPresetAsDrawn(House, TEXT("Side House (廂房)")));
		TestEqual(TEXT("the building now names it"), House->Preset, FString(TEXT("Side House (廂房)")));
		TestNearlyEqual(TEXT("the base course line stays"), House->GetBaseCourseTop(), Top, 0.5);
		TestEqual(TEXT("as drawn, nothing customized"), HutongDetailOps::CustomizedFields(House).Num(), 0);

		// A house turned corridor takes the walk its footprint leaves, as the tool would.
		House->FootprintX = 900.0;
		House->FootprintY = 230.0;
		const UHutongBuildingComponent* AsCorridor = HutongDetailOps::MakeAsDrawn(*House, UHutongCorridorBuildingComponent::StaticClass(), NAME_None, FString());
		const UHutongCorridorBuildingComponent* C = Cast<UHutongCorridorBuildingComponent>(AsCorridor);
		TestTrue(TEXT("walk from the footprint's depth"), C && FMath::IsNearlyEqual(C->Params.Width, C->Params.WalkWidthFromFootprint(230.0)));
	}
	return true;
}

// The bays are what the map attests: P and T keep the count drawn, forced or not.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongConvertBayCountTest, "HutongLayout.Convert.BayCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongConvertBayCountTest::RunTest(const FString& Parameters)
{
	auto Count = [](const UHutongBuildingComponent* B)
	{
		FHutongPlanBays Bays;
		B->GetPlanBays(Bays);
		return Bays.Boundaries.Num() - 1;
	};

	// Another preset: a forced count stays forced; a derived one is held where the new limits differ.
	for (const int32 Forced : { 0, 5 })
	{
		UHutongSiheyuanBuildingComponent* House = NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
		House->Preset = TEXT("Main Hall (正房)");
		House->ApplyPresetParams(House->Preset);
		House->SetFootprintSize(FVector2D(1060.0, 702.0));
		House->BayCountOverride = Forced;
		const int32 Before = Count(House);
		for (const FString& Name : UHutongPresetLibrary::Get()->GetPresetNames(House->GetPresetKey()))
		{
			TestTrue(*FString::Printf(TEXT("%s applies"), *Name), HutongDetailOps::ApplyPresetAsDrawn(House, Name));
			TestEqual(*FString::Printf(TEXT("%s keeps %d bays (forced %d)"), *Name, Before, Forced), Count(House), Before);
			if (Forced > 0) TestEqual(TEXT("forced stays forced"), House->BayCountOverride, Forced);
		}
	}

	// Another type: the count carries across generators.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;
	{
		UHutongBuildingComponent* House = PlaceNamed(World, UHutongSiheyuanBuildingComponent::StaticClass(),
			FVector2D(1400.0, 600.0), FTransform::Identity, TEXT("Bays"));
		if (House)
		{
			HutongDetailOps::SetBayCountOverride(House, 4);
			const HutongDetailOps::FConvertTarget Shop = HutongDetailOps::FindConvertTarget(
				Cast<UHutongBuildingComponent>(UHutongShopfrontBuildingComponent::StaticClass()->GetDefaultObject())->GetTypeLabel().ToString());
			const UHutongBuildingComponent* New = HutongDetailOps::ConvertBuilding(House, Shop, FString());
			TestTrue(TEXT("a four-bay house is a four-bay shop"), New && Count(New) == 4);
			const HutongDetailOps::FConvertTarget Back = HutongDetailOps::FindConvertTarget(TEXT("House (房)"));
			const UHutongBuildingComponent* Again = New ? HutongDetailOps::ConvertBuilding(const_cast<UHutongBuildingComponent*>(New), Back, TEXT("Side House (廂房)")) : nullptr;
			TestTrue(TEXT("and a four-bay side house again"), Again && Count(Again) == 4);
		}
	}
	World->DestroyWorld(false);
	return true;
}

#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongPlanSharedEdgeTest, "HutongLayout.Detail.PlanSharedEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongPlanSharedEdgeTest::RunTest(const FString& Parameters)
{
	// A gate flush with its house: one edge, drawn by the gate alone.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	auto Plan = [&](const FVector2D& Size, const FVector& At, const TCHAR* Name) -> UHutongPlanOutlineComponent*
	{
		UHutongBuildingComponent* B = PlaceNamed(World, UHutongSiheyuanBuildingComponent::StaticClass(),
			Size, FTransform(At), Name);
		if (!B) return nullptr;
		B->bPlanOnly = true;
		B->ApplyPlanOutline();
		return B->GetOwner()->FindComponentByClass<UHutongPlanOutlineComponent>();
	};
	UHutongPlanOutlineComponent* House = Plan(FVector2D(900.0, 600.0), FVector::ZeroVector, TEXT("SharedHouse"));
	UHutongPlanOutlineComponent* Gate = Plan(FVector2D(300.0, 600.0), FVector(900.0, 0.0, 0.0), TEXT("SharedGate"));
	UHutongPlanOutlineComponent* Far = Plan(FVector2D(300.0, 600.0), FVector(5000.0, 0.0, 0.0), TEXT("FarGate"));
	if (!TestTrue(TEXT("three plans place"), House && Gate && Far)) { World->DestroyWorld(false); return false; }

	const TArray<TPair<FVector, FVector>> HouseYields = House->CollectWinningNeighbourEdges();
	const bool bSharedEdge = HouseYields.ContainsByPredicate([](const TPair<FVector, FVector>& E)
	{
		return FMath::IsNearlyEqual(E.Key.X, 900.0, 0.01) && FMath::IsNearlyEqual(E.Value.X, 900.0, 0.01);
	});
	TestTrue(TEXT("the house yields the shared edge to the smaller gate"), bSharedEdge);
	TestEqual(TEXT("only the touching gate's edges"), HouseYields.Num(), 4);
	TestEqual(TEXT("the gate yields nothing"), Gate->CollectWinningNeighbourEdges().Num(), 0);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongOverlapsTest, "HutongLayout.Detail.Overlaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongOverlapsTest::RunTest(const FString& Parameters)
{
	// Share of the smaller footprint: a square half over another is 0.5, one inside a bigger one 1.
	const TArray<FVector2D> Square = { {0, 0}, {100, 0}, {100, 100}, {0, 100} };
	const TArray<FVector2D> Half = { {50, 0}, {150, 0}, {150, 100}, {50, 100} };
	const TArray<FVector2D> Big = { {-50, -50}, {200, -50}, {200, 200}, {-50, 200} };
	const TArray<FVector2D> Clockwise = { {0, 0}, {0, 100}, {100, 100}, {100, 0} };
	TestTrue(TEXT("half over is 0.5"), FMath::IsNearlyEqual(HutongOverlaps::OverlapShare(Square, Half), 0.5, 1e-6));
	TestTrue(TEXT("inside a bigger one is 1"), FMath::IsNearlyEqual(HutongOverlaps::OverlapShare(Square, Big), 1.0, 1e-6));
	TestTrue(TEXT("winding does not matter"), FMath::IsNearlyEqual(HutongOverlaps::OverlapShare(Clockwise, Square), 1.0, 1e-6));

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;
	auto Place = [&](const FVector& At, const TCHAR* Name)
	{
		return PlaceNamed(World, UHutongSiheyuanBuildingComponent::StaticClass(), FVector2D(900.0, 600.0), FTransform(At), Name);
	};
	UHutongBuildingComponent* Old = Place(FVector::ZeroVector, TEXT("OverlapOld"));
	UHutongBuildingComponent* Twin = Place(FVector(100.0, 50.0, 0.0), TEXT("OverlapTwin"));
	UHutongBuildingComponent* Neighbour = Place(FVector(900.0, 0.0, 0.0), TEXT("OverlapNeighbour"));
	if (!TestTrue(TEXT("three buildings place"), Old && Twin && Neighbour)) { World->DestroyWorld(false); return false; }

	const TArray<HutongOverlaps::FPair> Pairs = HutongOverlaps::Find({ Twin }, { Old, Twin, Neighbour });
	if (TestEqual(TEXT("one pair: the twin over the old one, not the flush neighbour"), Pairs.Num(), 1))
	{
		TestTrue(TEXT("the building already there comes first"), Pairs[0].First.Get() == Old && Pairs[0].Second.Get() == Twin);
	}
	TestEqual(TEXT("a whole-level scan meets the pair once"), HutongOverlaps::Find({ Old, Twin, Neighbour }, { Old, Twin, Neighbour }).Num(), 1);

	Old->Notes = TEXT("");
	Old->Confidence = EHutongConfidence::Inferred;
	Twin->Notes = TEXT("Traced off sheet 3M6");
	Twin->Confidence = EHutongConfidence::Attested;
	Twin->Court = TEXT("East court");
	HutongOverlaps::TransferMetadata(Twin, Old);
	TestTrue(TEXT("notes carried"), Old->Notes.Contains(TEXT("Traced off sheet 3M6")));
	TestEqual(TEXT("the higher confidence kept"), Old->Confidence, EHutongConfidence::Attested);
	TestEqual(TEXT("court filled where empty"), Old->Court, FString(TEXT("East court")));

	World->DestroyWorld(false);
	return true;
}
