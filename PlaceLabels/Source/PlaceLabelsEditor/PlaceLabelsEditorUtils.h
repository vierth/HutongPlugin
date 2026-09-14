#pragma once

#include "CoreMinimal.h"

namespace PlaceLabelsEditorUtils
{
	// Where the starter assets land.
	extern const TCHAR* StarterTypePackagePath;

	// Creates the district / area / avenue / hutong / compound / temple / building type assets with their parenting rules wired up, and saves them.
	int32 CreateStarterTypeAssets(TArray<FString>& OutCreatedNames, TArray<FString>& OutSkippedNames);
}
