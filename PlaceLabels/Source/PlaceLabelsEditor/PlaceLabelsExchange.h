#pragma once

#include "CoreMinimal.h"

class UPlaceLabelTypeAsset;
class UWorld;

// Getting label data in and out of the level as files.
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

		// Rows or features that could not be used, with the reason.
		TArray<FString> Problems;

		FText Summarise() const;
	};

	// Every region in the level, outlines and labels, as a GeoJSON FeatureCollection.
	void ExportGeoJson(UWorld* World, const FString& FilePath, FResult& OutResult);

	// Names and types only, one row per region, for bulk editing in a spreadsheet.
	void ExportCsv(UWorld* World, const FString& FilePath, FResult& OutResult);

	// Reads a GeoJSON file back.
	void ImportGeoJson(UWorld* World, const FString& FilePath, FResult& OutResult);

	// Reads names and types back from a spreadsheet.
	void ImportCsv(UWorld* World, const FString& FilePath, FResult& OutResult);

	// Every UPlaceLabelTypeAsset in the project, keyed by TypeId, for resolving imported types.
	void GatherTypeAssets(TMap<FName, UPlaceLabelTypeAsset*>& OutTypes);
}
