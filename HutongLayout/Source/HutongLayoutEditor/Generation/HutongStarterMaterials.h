#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

namespace HutongGen
{
	extern const TCHAR* StarterMaterialPackagePath;

	FString StarterMaterialAssetName(int32 Slot);

	bool StarterMaterialHasRelief(int32 Slot);

	UMaterialInterface* FindStarterMaterial(int32 Slot);

	int32 CreateStarterMaterials(TArray<FString>& OutCreatedNames, TArray<FString>& OutSkippedNames);

	int32 CreateStarterMaterialsAt(const FString& PackagePath, TArray<FString>& OutCreatedNames, TArray<FString>& OutSkippedNames);
}
