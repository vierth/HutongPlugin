#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Generation/BaySide.h"
#include "Generation/HutongDetail.h"
#include "Generation/HutongFootprint.h"

class AActor;
class UHutongBuildingComponent;
class UWorld;

namespace HutongExchange
{
	// 2: the file carries its own defaults (FSceneFile::Defaults).
	inline constexpr int32 FormatVersion = 2;

	inline const TCHAR* FileExtension = TEXT("hutong.json");

	struct FResult
	{
		bool bSucceeded = false;
		int32 Exported = 0;
		int32 Created = 0;
		int32 Updated = 0;
		int32 Skipped = 0;
		TArray<FString> Problems;

		TArray<TWeakObjectPtr<AActor>> Placed;

		bool bFromLayoutOnly = false;

		FText Summarise() const;
	};

	struct FRecord
	{
		FGuid Id;
		FName ClassName;
		FString Label;
		FName Folder;

		FVector2D Offset = FVector2D::ZeroVector;
		double OffsetZ = 0.0;
		double RelativeYawDeg = 0.0;

		FVector2D Footprint = FVector2D::ZeroVector;
		// For the import ghost and set bounds; the component blob is authoritative on import.
		FHutongFootprintSkew Skew;

		bool bHasFacing = false;
		EHutongBaySide Facing = EHutongBaySide::MinusY;
		EHutongDetail Detail = EHutongDetail::Near;
		bool bPlanOnly = false;
		// Run axis of a line-like piece; `SetFootprintSize` takes length off the run axis.
		bool bRunAlongY = false;

		FName Variant;

		TSharedPtr<FJsonObject> FootprintFields;

		TSharedPtr<FJsonObject> Blob;

		// Applied before Blob on import; null in a version-1 file.
		TSharedPtr<FJsonObject> Defaults;
	};

	struct FSceneFile
	{
		int32 Version = FormatVersion;
		FString LevelName;

		bool bLayoutOnly = false;

		FVector SetOriginWorld = FVector::ZeroVector;
		double SetYawDeg = 0.0;

		FVector2D BoundsSize = FVector2D::ZeroVector;

		TArray<FRecord> Records;

		// Records are read against these, so a file reads back the same in any project.
		TMap<FString, TSharedPtr<FJsonObject>> Defaults;

		// NAME_None skips.
		TMap<FName, FName> TypeRemap;
	};

	enum class EMode : uint8
	{
		Additive,
		Sync,
	};

	TSharedPtr<FJsonObject> WriteComponent(const UHutongBuildingComponent* Component);
	bool ReadComponent(const TSharedRef<FJsonObject>& Blob, UHutongBuildingComponent* Component,
		FString& OutProblem, const TSharedPtr<FJsonObject>& Defaults = nullptr);
	TSharedPtr<FJsonObject> DefaultsFor(UClass* Class, const FString& Preset);
	FString DefaultsKey(FName ClassName, const FString& Preset);

	bool Gather(const TArray<UHutongBuildingComponent*>& Buildings, UWorld* World,
		FSceneFile& OutFile, FResult& OutResult);

	bool Write(const FSceneFile& File, const FString& FilePath, FResult& OutResult);
	bool Read(const FString& FilePath, FSceneFile& OutFile, FResult& OutResult);

	FTransform ComposeRecordTransform(const FRecord& Record, const FTransform& SetToWorld);

	void FootprintCornersInSetFrame(const FRecord& Record, FVector2D OutCorners[4]);

	FVector2D NormaliseToBounds(TArray<FRecord>& Records, FVector2D& OutSize);

	// bBuildGeometry off: every building lands laid out; a Sync match already built is left as it is.
	void Place(UWorld* World, const FSceneFile& File, const FTransform& SetToWorld,
		EMode Mode, FName FolderOverride, FResult& OutResult, bool bBuildGeometry = false);

	// These open their own FScopedTransaction.
	void ExportLoaded(UWorld* World, const FString& FilePath, FResult& OutResult);
	// Every building in the level, loaded or not (World Partition loads the rest first).
	void ExportAll(UWorld* World, const FString& FilePath, FResult& OutResult);
	void ExportSelection(UWorld* World, const FString& FilePath, FResult& OutResult);
	using FResolveFile = TFunction<bool(FSceneFile&)>;

	void ImportAtRecordedTransforms(UWorld* World, const FString& FilePath, EMode Mode,
		FName FolderOverride, FResult& OutResult, const FResolveFile& Resolve = nullptr, bool bBuildGeometry = false);

	void GatherBuildingComponentClasses(TMap<FName, UClass*>& OutClasses);

	void FindUnknownTypes(const FSceneFile& File, TMap<FName, int32>& OutCounts);

	inline const TCHAR* SceneFileTypes = TEXT("Hutong scene (*.hutong.json)|*.hutong.json|JSON (*.json)|*.json");

	void Report(const FResult& Result, const TCHAR* What);
}
