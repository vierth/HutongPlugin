#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

namespace HutongGen
{
	// Where the starter material assets land, one per material slot.
	extern const TCHAR* StarterMaterialPackagePath;

	// The asset a slot's starter material is called, e.g. M_Hutong_Body.
	FString StarterMaterialAssetName(int32 Slot);

	// The starter material for a slot if the project holds one; null when it was never created or was deleted. Cheap enough to ask on every spawn.
	UMaterialInterface* FindStarterMaterial(int32 Slot);

	// Writes one editable material per slot into StarterMaterialPackagePath, seeded from the palette's default colours, and saves them. Existing assets are never overwritten. Returns how many were created.
	int32 CreateStarterMaterials(TArray<FString>& OutCreatedNames, TArray<FString>& OutSkippedNames);

	// The same into any package path — for a test to write somewhere it may throw away.
	int32 CreateStarterMaterialsAt(const FString& PackagePath, TArray<FString>& OutCreatedNames, TArray<FString>& OutSkippedNames);
}
