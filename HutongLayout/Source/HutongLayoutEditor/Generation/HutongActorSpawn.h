#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongPalette.h"

class UWorld;
class AStaticMeshActor;
class UMaterialInterface;

namespace HutongGen
{
	// Bakes Mesh into a UStaticMesh on a new AStaticMeshActor at Transform, one tinted material per slot.
	AStaticMeshActor* SpawnStaticMeshActor(
		UWorld* World,
		UE::Geometry::FDynamicMesh3& Mesh,
		const FTransform& Transform,
		const FString& NameBase,
		const FHutongPalette& Palette = FHutongPalette());

	// The same, with the whole LOD chain.
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
		int32 CollisionLOD = 0);

	// Bakes Mesh onto an actor that already exists.
	void BuildAndAssignStaticMesh(
		AStaticMeshActor* Actor,
		UE::Geometry::FDynamicMesh3& Mesh,
		const FHutongPalette& Palette = FHutongPalette());

	void BuildAndAssignStaticMesh(
		AStaticMeshActor* Actor,
		TArray<UE::Geometry::FDynamicMesh3>& LODs,
		const FHutongPalette& Palette = FHutongPalette(),
		int32 CollisionLOD = 0);

	// Library meshes (/Game/HutongLayout/Generated) no saved package and no loaded component references:
	// what a rebuild or a deleted building left behind.
	TArray<class UStaticMesh*> FindUnusedLibraryMeshes();

	// Slots the mesh wears, in order; material IDs remapped to 0..N-1.
	TArray<int32> CompactMaterialSlots(UE::Geometry::FDynamicMesh3& Mesh);
}
