#pragma once

#include "CoreMinimal.h"

namespace PlaceLabelsEditorUtils
{
	extern const TCHAR* StarterTypePackagePath;

	int32 CreateStarterTypeAssets(TArray<FString>& OutCreatedNames, TArray<FString>& OutSkippedNames);
}
