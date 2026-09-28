#pragma once

#include "CoreMinimal.h"

namespace PlaceLabelsEditorUtils
{
	// Where the starter assets land.
	extern const TCHAR* StarterTypePackagePath;

	// Creates and saves the starter type assets with their parenting rules.
	int32 CreateStarterTypeAssets(TArray<FString>& OutCreatedNames, TArray<FString>& OutSkippedNames);
}
