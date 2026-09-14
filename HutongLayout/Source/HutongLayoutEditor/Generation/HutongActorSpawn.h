#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongPalette.h"

class UWorld;
class AStaticMeshActor;
class UMaterialInterface;

namespace HutongGen
{
	// Bakes a dynamic mesh into a UStaticMesh, spawns an AStaticMeshActor at Transform, and gives it one material instance per EMaterialSlot, tinted from Palette.
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
		const FHutongPalette& Palette = FHutongPalette());

	// Bakes Mesh onto an actor that already exists.
	void BuildAndAssignStaticMesh(
		AStaticMeshActor* Actor,
		UE::Geometry::FDynamicMesh3& Mesh,
		const FHutongPalette& Palette = FHutongPalette());

	void BuildAndAssignStaticMesh(
		AStaticMeshActor* Actor,
		TArray<UE::Geometry::FDynamicMesh3>& LODs,
		const FHutongPalette& Palette = FHutongPalette());

	// The slots this mesh actually wears, in slot order, with its material IDs rewritten onto 0..N-1 to match.
	TArray<int32> CompactMaterialSlots(UE::Geometry::FDynamicMesh3& Mesh);
}
