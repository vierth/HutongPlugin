#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongActorSpawn.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "StaticMeshCompiler.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Where a placement's time goes, stage by stage, for a few types: generation, bake (logged per stage with
// hutong.LogBakeTiming), dressing, a second identical placement (the library hit), and the library save the
// editor does after a new bake. Asserts nothing; read the log.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongBakeTimingTest,
	"HutongDev.BakeTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongBakeTimingTest::RunTest(const FString& Parameters)
{
	IConsoleVariable* Log = IConsoleManager::Get().FindConsoleVariable(TEXT("hutong.LogBakeTiming"));
	if (Log) Log->Set(true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	struct FCase { UClass* Class; FVector2D Footprint; const TCHAR* Name; };
	const FCase Cases[] = {
		{ UHutongSiheyuanBuildingComponent::StaticClass(), FVector2D(1060.0, 700.0), TEXT("House") },
		{ UHutongShopfrontBuildingComponent::StaticClass(), FVector2D(1000.0, 520.0), TEXT("Shopfront") },
		{ UHutongWallBuildingComponent::StaticClass(), FVector2D(1000.0, 37.0), TEXT("LaneWall") },
		{ UHutongCityWallBuildingComponent::StaticClass(), FVector2D(6000.0, 2000.0), TEXT("CityWall") },
	};
	auto Ms = [](double Start) { return 1000.0 * (FPlatformTime::Seconds() - Start); };
	for (const FCase& C : Cases)
	{
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			UHutongBuildingComponent* Template = NewObject<UHutongBuildingComponent>(GetTransientPackage(), C.Class);
			Template->SetFootprintSize(C.Footprint);
			double T0 = FPlatformTime::Seconds();
			TArray<UE::Geometry::FDynamicMesh3> LODs;
			const int32 CollisionLOD = Template->BuildLODs(LODs);
			int32 Tris = 0;
			for (const auto& M : LODs) Tris += M.TriangleCount();
			const double Gen = Ms(T0);
			T0 = FPlatformTime::Seconds();
			AStaticMeshActor* Actor = HutongGen::SpawnStaticMeshActor(World, LODs, FTransform::Identity, C.Name, Template->GetBuiltPalette(), CollisionLOD);
			const double Bake = Ms(T0);
			// Dressed straight after the bake, as a shop's placement does: must not wait for the build.
			T0 = FPlatformTime::Seconds();
			if (Actor) HutongGen::AssignPaletteMaterials(Actor, Template->GetBuiltPalette());
			const double Dress = Ms(T0);
			// The build itself runs in the background: whatever touches the mesh next waits for it.
			T0 = FPlatformTime::Seconds();
			if (Actor && Actor->GetStaticMeshComponent()->GetStaticMesh())
			{
				FStaticMeshCompilingManager::Get().FinishCompilation({ Actor->GetStaticMeshComponent()->GetStaticMesh() });
			}
			const double Async = Ms(T0);
			double Save = 0.0;
			UStaticMesh* Mesh = Actor ? Actor->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
			if (Mesh && Pass == 0)
			{
				const FString File = FPaths::ProjectSavedDir() / TEXT("HutongBakeTiming") / (FString(C.Name) + TEXT(".uasset"));
				FSavePackageArgs Args;
				Args.TopLevelFlags = RF_Public | RF_Standalone;
				T0 = FPlatformTime::Seconds();
				UPackage::SavePackage(Mesh->GetPackage(), Mesh, *File, Args);
				Save = Ms(T0);
				IFileManager::Get().Delete(*File);
			}
			UE_LOG(LogTemp, Display, TEXT("HutongTiming %s pass %d: %d tris over %d LODs; generate %.1f ms, bake %.1f ms, async build %.1f ms, dress %.1f ms, save %.1f ms"),
				C.Name, Pass, Tris, LODs.Num(), Gen, Bake, Async, Dress, Save);
		}
	}
	// Eight distinct shops baked one after another with nothing waiting, then all finished: the builds run
	// side by side, as a bulk Generate's now do.
	{
		const double T0 = FPlatformTime::Seconds();
		TArray<UStaticMesh*> Meshes;
		for (int32 i = 0; i < 8; ++i)
		{
			UHutongBuildingComponent* Template = NewObject<UHutongBuildingComponent>(GetTransientPackage(), UHutongShopfrontBuildingComponent::StaticClass());
			Template->SetFootprintSize(FVector2D(900.0 + 16.0 * i, 520.0));
			TArray<UE::Geometry::FDynamicMesh3> LODs;
			const int32 CollisionLOD = Template->BuildLODs(LODs);
			if (AStaticMeshActor* Actor = HutongGen::SpawnStaticMeshActor(World, LODs, FTransform::Identity, TEXT("Batch"), Template->GetBuiltPalette(), CollisionLOD))
			{
				Meshes.Add(Actor->GetStaticMeshComponent()->GetStaticMesh());
			}
		}
		const double Queued = Ms(T0);
		FStaticMeshCompilingManager::Get().FinishCompilation(Meshes);
		UE_LOG(LogTemp, Display, TEXT("HutongTiming batch of 8 distinct shops: queued in %.1f ms, all built after %.1f ms"), Queued, Ms(T0));
	}
	if (Log) Log->Set(false);
	World->DestroyWorld(false);
	return true;
}

#endif
