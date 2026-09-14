#include "Misc/AutomationTest.h"
#include "Tools/HutongDetailOps.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongPlanOutlineComponent.h"
#include "Generation/HutongActorSpawn.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

using UE::Geometry::FDynamicMesh3;

namespace
{
	UHutongBuildingComponent* Place(UWorld* World, UClass* Class, const FVector2D& Footprint,
		const FTransform& Xform, int32 Bays, bool bPlanOnly = false)
	{
		AStaticMeshActor* Actor = HutongGen::SpawnEmptyActor(World, Xform, TEXT("DivideFuse"));
		if (!Actor) return nullptr;
		UHutongBuildingComponent* B = NewObject<UHutongBuildingComponent>(Actor, Class, NAME_None, RF_Transactional);
		B->DetailLevel = EHutongDetail::Massing;
		B->bBuildLODChain = false;
		B->bPlanOnly = bPlanOnly;
		B->SetFootprintSize(Footprint);
		HutongDetailOps::SetBayCountOverride(B, Bays);
		Actor->AddInstanceComponent(B);
		B->RegisterComponent();
		B->Rebuild();
		B->ApplyPlacementAttachments();
		return B;
	}

	int32 BayCount(const UHutongBuildingComponent* B)
	{
		FHutongPlanBays Bays;
		B->GetPlanBays(Bays);
		return Bays.Boundaries.Num() - 1;
	}

	bool HasMesh(const UHutongBuildingComponent* B)
	{
		const AStaticMeshActor* SMA = Cast<AStaticMeshActor>(B->GetOwner());
		return SMA && SMA->GetStaticMeshComponent() && SMA->GetStaticMeshComponent()->GetStaticMesh() != nullptr;
	}
}

// A five-bay house drawn across two compounds is two houses. Divided at its second bay line it
// becomes a two and a three standing end to end on the same line, each its own building with its
// own gable and door; fused again it is the five it was.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongDivideFuseHouseTest, "HutongLayout.DivideFuse.HouseAtBayLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongDivideFuseHouseTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };

	const FTransform Xform(FRotator(0.0, 30.0, 0.0), FVector(400.0, -900.0, 0.0));
	UHutongBuildingComponent* House = Place(World, UHutongSiheyuanBuildingComponent::StaticClass(),
		FVector2D(1500.0, 500.0), Xform, 5);
	if (!TestNotNull(TEXT("the house places"), House)) return false;
	TestTrue(TEXT("a five-bay house can be divided"), HutongDetailOps::CanDivide(House));
	TestEqual(TEXT("and has five bays"), BayCount(House), 5);

	FHutongPlanBays Bays;
	House->GetPlanBays(Bays);
	Bays.Boundaries.Sort();
	const double Cut = Bays.Boundaries[2];
	const FGuid Id = House->BuildingId;
	const EHutongPurlins Purlins = Cast<UHutongSiheyuanBuildingComponent>(House)->Params.Purlins;

	FText Why;
	UHutongBuildingComponent* Second = HutongDetailOps::DivideBuilding(House, 2, Why);
	if (!TestNotNull(*FString::Printf(TEXT("the house divides: %s"), *Why.ToString()), Second)) return false;

	TestEqual(TEXT("the house keeps two bays"), BayCount(House), 2);
	TestEqual(TEXT("the new building takes three"), BayCount(Second), 3);
	TestEqual(TEXT("the house's frontage ends at the cut"), House->GetFootprintSize().X, Cut, 0.01);
	TestEqual(TEXT("the new one's frontage is the rest"), Second->GetFootprintSize().X, 1500.0 - Cut, 0.01);
	TestEqual(TEXT("both keep the depth"), Second->GetFootprintSize().Y, 500.0, 0.01);
	TestTrue(TEXT("the new one is the same type"), Second->GetClass() == House->GetClass());
	TestEqual(TEXT("with the same parameters"), (int32)Cast<UHutongSiheyuanBuildingComponent>(Second)->Params.Purlins, (int32)Purlins);
	TestTrue(TEXT("and a fresh id"), Second->BuildingId.IsValid() && Second->BuildingId != Id);
	TestEqual(TEXT("the house keeps its id"), House->BuildingId, Id);

	// The second stands at the cut, on the house's own line, turned the same way.
	const FVector Expected = Xform.TransformPosition(FVector(Cut, 0.0, 0.0));
	const AActor* SecondActor = Second->GetOwner();
	TestTrue(TEXT("the new building stands at the cut"), SecondActor && SecondActor->GetActorLocation().Equals(Expected, 0.1));
	TestEqual(TEXT("on the same bearing"), SecondActor ? SecondActor->GetActorRotation().Yaw : 0.0, 30.0, 0.01);
	TestTrue(TEXT("the house has not moved"), House->GetOwner()->GetActorLocation().Equals(Xform.GetLocation(), 0.01));
	EHutongBaySide SideA, SideB;
	TestTrue(TEXT("both face the same way"), House->GetFacade(SideA) && Second->GetFacade(SideB) && SideA == SideB);
	TestTrue(TEXT("both are built"), HasMesh(House) && HasMesh(Second));

	// The pair now offers to fuse, at the house's far end and the new one's origin end.
	bool bAtEnd = false;
	TestTrue(TEXT("the new one fuses onto the house's end"), HutongDetailOps::CanFuse(House, Second, bAtEnd, &Why) && bAtEnd);
	TestTrue(TEXT("and the house onto the new one's start"), HutongDetailOps::CanFuse(Second, House, bAtEnd, &Why) && !bAtEnd);

	TestTrue(*FString::Printf(TEXT("they fuse: %s"), *Why.ToString()), HutongDetailOps::FuseBuildings(House, Second, Why));
	TestEqual(TEXT("five bays again"), BayCount(House), 5);
	TestEqual(TEXT("the full frontage again"), House->GetFootprintSize().X, 1500.0, 0.01);
	TestTrue(TEXT("standing where it stood"), House->GetOwner()->GetActorLocation().Equals(Xform.GetLocation(), 0.01));
	TestTrue(TEXT("the absorbed actor is gone"), !IsValid(SecondActor) || SecondActor->IsActorBeingDestroyed());
	TestTrue(TEXT("and the house is built"), HasMesh(House));

	// Fused the other way round the fused building starts where the earlier one did.
	UHutongBuildingComponent* Third = HutongDetailOps::DivideBuilding(House, 3, Why);
	if (!TestNotNull(TEXT("divides again"), Third)) return false;
	TestTrue(TEXT("the later piece absorbs the earlier"), HutongDetailOps::FuseBuildings(Third, House, Why));
	TestTrue(TEXT("and takes its origin"), Third->GetOwner()->GetActorLocation().Equals(Xform.GetLocation(), 0.1));
	TestEqual(TEXT("with all five bays"), BayCount(Third), 5);
	return true;
}

// What refuses to fuse: another type, a gap, a different depth, an angle, a corner cut on the bias.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongFuseRefusalsTest, "HutongLayout.DivideFuse.FuseRefusals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongFuseRefusalsTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); };

	UClass* HouseClass = UHutongSiheyuanBuildingComponent::StaticClass();
	UHutongBuildingComponent* House = Place(World, HouseClass, FVector2D(900.0, 500.0), FTransform::Identity, 3, true);
	if (!TestNotNull(TEXT("the house places"), House)) return false;
	bool bAtEnd = false;
	FText Why;

	UHutongBuildingComponent* Shop = Place(World, UHutongShopfrontBuildingComponent::StaticClass(),
		FVector2D(600.0, 500.0), FTransform(FVector(900.0, 0.0, 0.0)), 2, true);
	TestFalse(TEXT("a shop does not fuse with a house"), HutongDetailOps::CanFuse(House, Shop, bAtEnd, &Why));

	UHutongBuildingComponent* Apart = Place(World, HouseClass, FVector2D(600.0, 500.0), FTransform(FVector(1200.0, 0.0, 0.0)), 2, true);
	TestFalse(TEXT("a house three metres on does not"), HutongDetailOps::CanFuse(House, Apart, bAtEnd, &Why));

	UHutongBuildingComponent* Deeper = Place(World, HouseClass, FVector2D(600.0, 620.0), FTransform(FVector(900.0, 0.0, 0.0)), 2, true);
	TestFalse(TEXT("a deeper house does not"), HutongDetailOps::CanFuse(House, Deeper, bAtEnd, &Why));

	UHutongBuildingComponent* Turned = Place(World, HouseClass, FVector2D(600.0, 500.0), FTransform(FRotator(0.0, 10.0, 0.0), FVector(900.0, 0.0, 0.0)), 2, true);
	TestFalse(TEXT("a house at an angle does not, yet"), HutongDetailOps::CanFuse(House, Turned, bAtEnd, &Why));

	UHutongBuildingComponent* Beside = Place(World, HouseClass, FVector2D(600.0, 500.0), FTransform(FVector(900.0, 40.0, 0.0)), 2, true);
	TestFalse(TEXT("a house off the line does not"), HutongDetailOps::CanFuse(House, Beside, bAtEnd, &Why));

	// Three centimetres short is a hand-placed join and fuses; a corner cut on the bias at the join does not.
	UHutongBuildingComponent* Near = Place(World, HouseClass, FVector2D(600.0, 500.0), FTransform(FVector(903.0, 0.0, 0.0)), 2, true);
	TestTrue(TEXT("a join three centimetres open fuses"), HutongDetailOps::CanFuse(House, Near, bAtEnd, &Why) && bAtEnd);
	Near->FootprintSkew.Corner00 = FVector2D(30.0, 0.0);
	TestFalse(TEXT("but not with its join corner angled"), HutongDetailOps::CanFuse(House, Near, bAtEnd, &Why));

	// The far end's angle rides along: fused, the outer corner keeps its offset and the join is square.
	Near->FootprintSkew.Corner00 = FVector2D::ZeroVector;
	Near->FootprintSkew.Corner10 = FVector2D(-25.0, 0.0);
	TestTrue(TEXT("fuses with the far end angled"), HutongDetailOps::FuseBuildings(House, Near, Why));
	TestEqual(TEXT("the fused far corner keeps the angle"), House->FootprintSkew.Corner10.X, -25.0, 0.01);
	TestTrue(TEXT("and the origin corners are square"), House->FootprintSkew.Corner00.IsNearlyZero() && House->FootprintSkew.Corner01.IsNearlyZero());
	TestEqual(TEXT("frontage is the sum"), House->GetFootprintSize().X, 1500.0, 0.01);

	// Divided again, the angle goes with the far piece and the cut end is square on both.
	UHutongBuildingComponent* Far = HutongDetailOps::DivideBuilding(House, 2, Why);
	if (!TestNotNull(TEXT("the fused house divides"), Far)) return false;
	TestEqual(TEXT("the far piece carries the angle"), Far->FootprintSkew.Corner10.X, -25.0, 0.01);
	TestTrue(TEXT("its cut end is square"), Far->FootprintSkew.Corner00.IsNearlyZero());
	TestTrue(TEXT("and so is the kept piece's"), House->FootprintSkew.IsZero());
	TestTrue(TEXT("laid-out pieces stay laid out"), House->bPlanOnly && Far->bPlanOnly && !HasMesh(House) && !HasMesh(Far));
	TestNotNull(TEXT("with an outline each"), Far->GetOwner()->FindComponentByClass<UHutongPlanOutlineComponent>());

	// A cut at an end is no cut.
	TestNull(TEXT("dividing at the origin line is refused"), HutongDetailOps::DivideBuilding(House, 0, Why));
	TestNull(TEXT("and at the far line"), HutongDetailOps::DivideBuilding(House, BayCount(House), Why));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
