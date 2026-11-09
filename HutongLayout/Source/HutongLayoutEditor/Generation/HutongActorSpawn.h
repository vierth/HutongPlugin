#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongMeshTurn.h"

class UWorld;
class AStaticMeshActor;
class UMaterialInterface;
class UStaticMeshComponent;
class AActor;

namespace HutongGen
{
	AStaticMeshActor* SpawnStaticMeshActor(
		UWorld* World,
		UE::Geometry::FDynamicMesh3& Mesh,
		const FTransform& Transform,
		const FString& NameBase,
		const FHutongPalette& Palette = FHutongPalette());

	AStaticMeshActor* SpawnEmptyActor(
		UWorld* World,
		const FTransform& Transform,
		const FString& NameBase);

	AStaticMeshActor* SpawnStaticMeshActor(
		UWorld* World,
		TArray<UE::Geometry::FDynamicMesh3>& LODs,
		const FTransform& Transform,
		const FString& NameBase,
		const FHutongPalette& Palette = FHutongPalette(),
		int32 CollisionLOD = 0,
		const FMeshTurn& Turn = FMeshTurn());

	UStaticMeshComponent* GetBuiltMeshComponent(const AActor* Actor);

	bool MakeMeshUnique(AStaticMeshActor* Actor);

	void StartLibrarySaver();
	void StopLibrarySaver();

	void AssignPaletteMaterials(AStaticMeshActor* Actor, const FHutongPalette& Palette);

	struct FMeshInstances
	{
		TArray<UE::Geometry::FDynamicMesh3> LODs;
		int32 CollisionLOD = 0;
		TArray<FTransform> Placed;
	};

	void AssignMeshInstances(AStaticMeshActor* Actor, TArray<FMeshInstances>& Groups, const FHutongPalette& Palette);
	void ClearMeshInstances(AStaticMeshActor* Actor);
	// -1 while a mesh it wears is still building in the background, unless asked to wait for it.
	int64 CountBuiltTriangles(const AActor* Actor, bool bWaitForBuild = false);
	void CountMeshInstances(const AActor* Actor, int32& OutPlaced, int32& OutMeshes);

	FString LibraryKeyFor(const TArray<UE::Geometry::FDynamicMesh3>& LODs, int32 CollisionLOD);

	void BuildAndAssignStaticMesh(
		AStaticMeshActor* Actor,
		UE::Geometry::FDynamicMesh3& Mesh,
		const FHutongPalette& Palette = FHutongPalette());

	void BuildAndAssignStaticMesh(
		AStaticMeshActor* Actor,
		TArray<UE::Geometry::FDynamicMesh3>& LODs,
		const FHutongPalette& Palette = FHutongPalette(),
		int32 CollisionLOD = 0,
		bool bBespoke = false,
		const FMeshTurn& Turn = FMeshTurn());

	// Reads the asset registry only: the library runs to thousands.
	TArray<struct FAssetData> FindUnusedLibraryMeshes();

	TArray<int32> CompactMaterialSlots(UE::Geometry::FDynamicMesh3& Mesh);
}
