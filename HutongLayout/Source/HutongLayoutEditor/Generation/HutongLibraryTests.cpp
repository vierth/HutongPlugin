#include "Generation/HutongActorSpawn.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongMeshLibraryTest,
	"HutongLayout.Detail.MeshLibrary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongMeshLibraryTest::RunTest(const FString& Parameters)
{
	// Identical geometry shares one asset in the library; different geometry does not; with sharing off
	// the mesh is the actor's own, as before.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to spawn into"), World)) return false;

	auto Build = [](double Height)
	{
		TArray<UE::Geometry::FDynamicMesh3> LODs;
		UE::Geometry::FDynamicMesh3& M = LODs.AddDefaulted_GetRef();
		{
			HutongMeshUtils::FSlotScope Body(M, HutongGen::MatSlot_Body);
			HutongMeshUtils::AppendBox(M, FVector3d(0, 0, 0), FVector3d(300, 200, Height));
		}
		{
			HutongMeshUtils::FSlotScope Roof(M, HutongGen::MatSlot_Roof);
			HutongMeshUtils::AppendBox(M, FVector3d(-20, -20, Height), FVector3d(320, 220, Height + 40));
		}
		return LODs;
	};
	auto Bake = [&](double Height) -> UStaticMesh*
	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
		TArray<UE::Geometry::FDynamicMesh3> LODs = Build(Height);
		HutongGen::BuildAndAssignStaticMesh(Actor, LODs, FHutongPalette(), 0);
		UStaticMeshComponent* C = Actor->GetStaticMeshComponent();
		TestEqual(TEXT("each building wears its own palette over the mesh's two slots"), C->OverrideMaterials.Num(), 2);
		return C->GetStaticMesh();
	};

	// A height no other test uses, so an earlier run's asset is not what is found.
	const double H = 311.0 + FMath::FRand();
	UStaticMesh* A = Bake(H);
	UStaticMesh* B = Bake(H);
	UStaticMesh* C = Bake(H + 50.0);
	TestTrue(TEXT("identical buildings share one mesh"), A && A == B);
	TestTrue(TEXT("a different building has its own"), C && C != A);
	TestTrue(FString::Printf(TEXT("the shared mesh lives in the library (%s)"), A ? *A->GetPackage()->GetName() : TEXT("none")),
		A && A->GetPackage()->GetName().StartsWith(TEXT("/Game/HutongLayout/Generated/")));

	// A building rebuilt to other geometry leaves its old mesh to the clean-up; one still worn stays.
	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
		TArray<UE::Geometry::FDynamicMesh3> LODs = Build(H + 90.0);
		HutongGen::BuildAndAssignStaticMesh(Actor, LODs, FHutongPalette(), 0);
		UStaticMesh* Left = Actor->GetStaticMeshComponent()->GetStaticMesh();
		Actor->GetStaticMeshComponent()->SetStaticMesh(nullptr);
		const TArray<UStaticMesh*> Unused = HutongGen::FindUnusedLibraryMeshes();
		TestTrue(TEXT("the mesh no building wears is offered for clean-up"), Left && Unused.Contains(Left));
		TestFalse(TEXT("a mesh still worn is not"), Unused.Contains(A));
	}

	IConsoleVariable* Share = IConsoleManager::Get().FindConsoleVariable(TEXT("hutong.ShareMeshes"));
	if (TestNotNull(TEXT("the switch exists"), Share))
	{
		Share->Set(false);
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
		TArray<UE::Geometry::FDynamicMesh3> LODs = Build(H);
		HutongGen::BuildAndAssignStaticMesh(Actor, LODs, FHutongPalette(), 0);
		UStaticMesh* Own = Actor->GetStaticMeshComponent()->GetStaticMesh();
		TestTrue(TEXT("sharing off: the actor's own mesh, outered to it"), Own && Own != A && Own->GetOuter() == Actor);
		Share->Set(true);
	}

	World->DestroyWorld(false);
	return true;
}

#endif
