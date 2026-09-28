#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

namespace HutongGen
{
	// Where the starter material assets land, one per material slot.
	extern const TCHAR* StarterMaterialPackagePath;

	// e.g. M_Hutong_Body.
	FString StarterMaterialAssetName(int32 Slot);

	// Whether the slot's material has a pattern (own normal and roughness).
	bool StarterMaterialHasRelief(int32 Slot);

	// Null if not created or deleted. Cached; cheap per spawn.
	UMaterialInterface* FindStarterMaterial(int32 Slot);

	// Creates and saves one material per slot from palette defaults; never overwrites. Returns count created.
	int32 CreateStarterMaterials(TArray<FString>& OutCreatedNames, TArray<FString>& OutSkippedNames);

	// Same, into any path (for tests).
	int32 CreateStarterMaterialsAt(const FString& PackagePath, TArray<FString>& OutCreatedNames, TArray<FString>& OutSkippedNames);
}
