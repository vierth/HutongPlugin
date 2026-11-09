#pragma once

#include "CoreMinimal.h"

class UPlaceLabelTypeAsset;
class UWorld;

namespace PlaceLabelsExchange
{
	inline constexpr int32 FormatVersion = 1;

	struct FResult
	{
		bool bSucceeded = false;

		int32 Created = 0;
		int32 Updated = 0;
		int32 Skipped = 0;
		int32 Exported = 0;

		TArray<FString> Problems;

		FText Summarise() const;
	};

	void ExportGeoJson(UWorld* World, const FString& FilePath, FResult& OutResult);

	void ExportCsv(UWorld* World, const FString& FilePath, FResult& OutResult);

	void ImportGeoJson(UWorld* World, const FString& FilePath, FResult& OutResult);

	void ImportCsv(UWorld* World, const FString& FilePath, FResult& OutResult);

	void GatherTypeAssets(TMap<FName, UPlaceLabelTypeAsset*>& OutTypes);
}
