#include "Generation/GateHouseGenerator.h"
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
#include "AssetRegistry/AssetData.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongMeshLibraryTest,
	"HutongLayout.Detail.MeshLibrary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongMeshLibraryTest::RunTest(const FString& Parameters)
{
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
		UStaticMeshComponent* C = HutongGen::GetBuiltMeshComponent(Actor);
		TestEqual(TEXT("each building wears its own palette over the mesh's two slots"), C->OverrideMaterials.Num(), 2);
		return C->GetStaticMesh();
	};

	// A height no other test uses, so the asset found is this run's.
	const double H = 311.0 + FMath::FRand();
	UStaticMesh* A = Bake(H);
	UStaticMesh* B = Bake(H);
	UStaticMesh* C = Bake(H + 50.0);
	TestTrue(TEXT("identical buildings share one mesh"), A && A == B);
	TestTrue(TEXT("a different building has its own"), C && C != A);
	TestTrue(FString::Printf(TEXT("the shared mesh lives in the library (%s)"), A ? *A->GetPackage()->GetName() : TEXT("none")),
		A && A->GetPackage()->GetName().StartsWith(TEXT("/Game/HutongLayout/Generated/")));

	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
		TArray<UE::Geometry::FDynamicMesh3> LODs = Build(H + 90.0);
		HutongGen::BuildAndAssignStaticMesh(Actor, LODs, FHutongPalette(), 0);
		UStaticMesh* Left = HutongGen::GetBuiltMeshComponent(Actor)->GetStaticMesh();
		HutongGen::GetBuiltMeshComponent(Actor)->SetStaticMesh(nullptr);
		TSet<FName> Unused;
		for (const FAssetData& Asset : HutongGen::FindUnusedLibraryMeshes()) Unused.Add(Asset.PackageName);
		TestTrue(TEXT("the mesh no building wears is offered for clean-up"), Left && Unused.Contains(Left->GetPackage()->GetFName()));
		TestFalse(TEXT("a mesh still worn is not"), A && Unused.Contains(A->GetPackage()->GetFName()));
	}

	IConsoleVariable* Share = IConsoleManager::Get().FindConsoleVariable(TEXT("hutong.ShareMeshes"));
	if (TestNotNull(TEXT("the switch exists"), Share))
	{
		Share->Set(false);
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
		TArray<UE::Geometry::FDynamicMesh3> LODs = Build(H);
		HutongGen::BuildAndAssignStaticMesh(Actor, LODs, FHutongPalette(), 0);
		UStaticMesh* Own = HutongGen::GetBuiltMeshComponent(Actor)->GetStaticMesh();
		TestTrue(TEXT("sharing off: the actor's own mesh, outered to it"), Own && Own != A && Own->GetOuter() == Actor);
		Share->Set(true);
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGateSeedsShareTest,
	"HutongLayout.Detail.GateSeedsShare",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGateSeedsShareTest::RunTest(const FString& Parameters)
{
	const auto GateVertices = [](int32 Seed)
	{
		FHutongGateHouseParams P;
		P.RandomSeed = Seed;
		P.Width = 600.0; // Narrower, the leaves all take the widest swing to hold the clear width.
		UE::Geometry::FDynamicMesh3 Mesh;
		HutongGen::BuildGateHouse(Mesh, P);
		FString Vertices;
		for (const int32 V : Mesh.VertexIndicesItr())
		{
			Vertices += Mesh.GetVertex(V).ToString();
		}
		return Vertices;
	};

	TSet<FString> Forms;
	for (int32 i = 0; i < 200; ++i)
	{
		Forms.Add(GateVertices(i * 7919 - 500000));
	}
	TestEqual(TEXT("two hundred seeds make four gates"), Forms.Num(), 4);
	TestEqual(TEXT("a seed rebuilds the same gate"), GateVertices(1234567), GateVertices(1234567));
	TestEqual(TEXT("a negative seed is one of the four"), Forms.Contains(GateVertices(-7)), true);
	return true;
}

#endif
