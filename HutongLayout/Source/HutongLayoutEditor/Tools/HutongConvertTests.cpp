#include "Misc/AutomationTest.h"
#include "Tools/HutongDetailOps.h"
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
	// A placement of this type, standing where a drag would have left it.
	UHutongBuildingComponent* Place(UWorld* World, UClass* Class, const FVector2D& Footprint,
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

// A building is not stuck as whatever tool drew it. What the placement decided survives the change
// of type — where it stands, its footprint, its facing, its id — and everything else comes from the
// type converted to, which is the whole difference between converting and placing again.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongConvertAcrossTypesTest, "HutongLayout.Convert.AcrossTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongConvertAcrossTypesTest::RunTest(const FString& Parameters)
{
	// Every type is offered, and a class carrying two kinds offers both: the wall is where a
	// conversion list built from classes alone would lose half the answer.
	const TArray<HutongDetailOps::FConvertTarget>& Targets = HutongDetailOps::ConvertTargets();
	TestTrue(TEXT("every building type is offered"), Targets.Num() >= 15);

	const HutongDetailOps::FConvertTarget House =
		HutongDetailOps::FindConvertTarget(TEXT("house (房)"));
	const HutongDetailOps::FConvertTarget LaneWall =
		HutongDetailOps::FindConvertTarget(TEXT("lane wall (院牆)"));
	const HutongDetailOps::FConvertTarget CourtWall =
		HutongDetailOps::FindConvertTarget(TEXT("court wall (隔牆)"));
	if (!TestTrue(TEXT("the house is a target"), House.IsValid())) return false;
	if (!TestTrue(TEXT("both walls are targets"), LaneWall.IsValid() && CourtWall.IsValid())) return false;
	TestEqual(TEXT("the two walls are one class"), LaneWall.Class, CourtWall.Class);

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	// 鋪面房 → 房: a different generator, a different params struct.
	{
		const FVector2D Footprint(1120.0, 640.0);
		const FTransform Xform(FRotator(0.0, 23.0, 0.0), FVector(800.0, -200.0, 0.0));
		UHutongBuildingComponent* Shop = Place(World, UHutongShopfrontBuildingComponent::StaticClass(),
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

		// The mesh is the new type's, not the old one's left standing.
		const AStaticMeshActor* SMA = Cast<AStaticMeshActor>(Actor);
		TestTrue(TEXT("it is built again"),
			SMA && SMA->GetStaticMeshComponent() && SMA->GetStaticMeshComponent()->GetStaticMesh() != nullptr);
	}

	// 院牆 → 隔牆: one class, two kinds, and the conversion has to say which.
	{
		UHutongBuildingComponent* Wall = Place(World, LaneWall.Class, FVector2D(1400.0, 37.0),
			FTransform::Identity, TEXT("ConvertWall"));
		if (!TestNotNull(TEXT("the wall places"), Wall)) { World->DestroyWorld(false); return false; }
		Wall->SetTypeVariant(LaneWall.Variant);

		UHutongBuildingComponent* Court = HutongDetailOps::ConvertBuilding(Wall, CourtWall, FString());
		if (!TestNotNull(TEXT("the wall converts"), Court)) { World->DestroyWorld(false); return false; }
		TestEqual(TEXT("it is a 隔牆 now"), Court->GetTypeVariant(), CourtWall.Variant);
		TestEqual(TEXT("and says so"), Court->GetTypeLabel().ToString(), FString(TEXT("court wall (隔牆)")));
		TestEqual(TEXT("along the run it was drawn on"), Court->GetFootprintSize().X, 1400.0, 0.01);
	}

	World->DestroyWorld(false);
	return true;
}

// Undoing a conversion put geometry on a laid-out plan. The component the conversion added is
// *un-created* by the undo, and that object is restored to its defaults before it goes — with
// bPlanOnly false — so the rebuild it ran on the way out baked a default house onto an actor whose
// whole point was that it has no geometry. One Ctrl+Z, and the building that comes back is the one
// that was there.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongConvertUndoTest, "HutongLayout.Convert.Undo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongConvertUndoTest::RunTest(const FString& Parameters)
{
	if (!GEditor || !GEditor->Trans) { AddError(TEXT("no transaction buffer to undo through")); return false; }

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	const HutongDetailOps::FConvertTarget House =
		HutongDetailOps::FindConvertTarget(TEXT("house (房)"));
	if (!TestTrue(TEXT("the house is a target"), House.IsValid())) { World->DestroyWorld(false); return false; }

	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const bool bPlanOnly = (Pass == 1);
		const TCHAR* What = bPlanOnly ? TEXT("a laid-out shopfront") : TEXT("a built shopfront");

		UHutongBuildingComponent* Shop = Place(World, UHutongShopfrontBuildingComponent::StaticClass(),
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

// A laid-out building draws one outline, and resizing it moves that one. An actor that has somehow
// acquired two — an undo restoring a destroyed outline onto an actor that has since made itself a
// new one is the way it happens — used to have only its first one found and updated ever again,
// while the second stood there drawing the footprint as it was, following the actor about. Which
// is what a resize looks like when the original rectangle will not go away.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongPlanOutlineUniqueTest, "HutongLayout.Detail.PlanOutlineUnique",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongPlanOutlineUniqueTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	const FVector2D Footprint(900.0, 600.0);
	UHutongBuildingComponent* B = Place(World, UHutongSiheyuanBuildingComponent::StaticClass(),
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

	// A second one, as an undo would leave it, still drawing the size it was made at.
	UHutongPlanOutlineComponent* Stray = NewObject<UHutongPlanOutlineComponent>(
		Actor, NAME_None, RF_Transactional);
	Stray->SetupAttachment(Actor->GetRootComponent());
	Actor->AddInstanceComponent(Stray);
	Stray->RegisterComponent();
	FHutongPlanBays NoBays;
	Stray->SetPlan(Footprint, FHutongFootprintSkew(), false, EHutongBaySide::MinusY, {}, false, NoBays, true,
		FLinearColor::White);
	TestEqual(TEXT("two outlines to start from"), OutlinesOn(Actor).Num(), 2);

	// The resize the drag does, through the same accessor.
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

	// Built again, the outline goes: geometry says the footprint from then on.
	B->bPlanOnly = false;
	B->ApplyPlanOutline();
	TestEqual(TEXT("no outline on a built building"), OutlinesOn(Actor).Num(), 0);

	World->DestroyWorld(false);
	return true;
}

#endif
