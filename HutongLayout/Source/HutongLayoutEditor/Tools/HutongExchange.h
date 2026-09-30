#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Generation/BaySide.h"
#include "Generation/HutongDetail.h"
#include "Generation/HutongFootprint.h"

class AActor;
class UHutongBuildingComponent;
class UWorld;

// Placed buildings to file and back.
namespace HutongExchange
{
	// 2: the file carries its own defaults (FSceneFile::Defaults).
	inline constexpr int32 FormatVersion = 2;

	inline const TCHAR* FileExtension = TEXT("hutong.json");

	// One run's outcome.
	struct FResult
	{
		bool bSucceeded = false;
		int32 Exported = 0;
		int32 Created = 0;
		int32 Updated = 0;
		int32 Skipped = 0;
		TArray<FString> Problems;

		// What the import placed or reshaped, so the caller can select it and move it as one with the gizmo
		// (a file landing at foreign coordinates).
		TArray<TWeakObjectPtr<AActor>> Placed;

		// Imported from a layout-only file: params are current type defaults, not the export's, and the
		// report must say so or it reads as lost tuning.
		bool bFromLayoutOnly = false;

		FText Summarise() const;
	};

	// One placed building as serialised.
	struct FRecord
	{
		FGuid Id;
		// Component class name.
		FName ClassName;
		FString Label;
		FName Folder;

		FVector2D Offset = FVector2D::ZeroVector;
		double OffsetZ = 0.0;
		double RelativeYawDeg = 0.0;

		// GetFootprintSize() at export.
		FVector2D Footprint = FVector2D::ZeroVector;
		// Corner offsets at export, for the import ghost and set bounds only; the component blob and
		// Footprint-category fields are authoritative on import.
		FHutongFootprintSkew Skew;

		// Placement decisions, written with or without params; all a layout-only record has.
		bool bHasFacing = false;
		EHutongBaySide Facing = EHutongBaySide::MinusY;
		EHutongDetail Detail = EHutongDetail::Near;
		bool bPlanOnly = false;
		// Run axis of a line-like piece. Needed by layout-only records: `SetFootprintSize` takes length off
		// the run axis, so a wall drawn along Y came back a stub.
		bool bRunAlongY = false;

		// Variant within the class (the wall's 院牆/隔牆 role), where a class has several.
		FName Variant;

		// Component's Footprint-category fields (length, width, run axis, facade side, bay override, wall
		// miters), by reflection so a new field in that category travels automatically.
		TSharedPtr<FJsonObject> FootprintFields;

		// Params as differences from Defaults; null on a layout-only record.
		TSharedPtr<FJsonObject> Blob;

		// The file's defaults for this record's type and preset: what Blob is a difference from, applied
		// before it on import. Null in a file written before the defaults travelled.
		TSharedPtr<FJsonObject> Defaults;
	};

	struct FSceneFile
	{
		int32 Version = FormatVersion;
		FString LevelName;

		// An old arrangement-only file (type, footprint, position, yaw, facing), no params: rebuilt from
		// current type defaults. Read, never written — no export drops a building's parameters now.
		bool bLayoutOnly = false;

		// Set frame origin in the source level; enough to place at recorded coordinates.
		FVector SetOriginWorld = FVector::ZeroVector;
		double SetYawDeg = 0.0;

		FVector2D BoundsSize = FVector2D::ZeroVector;

		TArray<FRecord> Records;

		// Every field of a fresh component, per type and preset used by a record here (key DefaultsKey),
		// written at the top of the file. A record is read against these, not against the presets and
		// shipped defaults of whatever project reads it, so a canon change or a differing preset cannot
		// change the work in the file.
		TMap<FString, TSharedPtr<FJsonObject>> Defaults;

		// Unknown-type remap: old class name -> class to build, or NAME_None to skip. Filled by the import
		// dialog before Place, so files predating a split or rename still land.
		TMap<FName, FName> TypeRemap;
	};

	enum class EMode : uint8
	{
		// Always spawn with fresh ids: a copy of someone else's street beside yours.
		Additive,
		// Match on id: update in place, spawn what is missing.
		Sync,
	};

	// --- reflected halves, public for tests ---
	TSharedPtr<FJsonObject> WriteComponent(const UHutongBuildingComponent* Component);
	// Defaults: the file's own for this type and preset; applied first, in place of the preset lookup.
	bool ReadComponent(const TSharedRef<FJsonObject>& Blob, UHutongBuildingComponent* Component,
		FString& OutProblem, const TSharedPtr<FJsonObject>& Defaults = nullptr);
	// Every field of a fresh component of the class with the preset applied (identity left out): what a
	// record is written as a difference from.
	TSharedPtr<FJsonObject> DefaultsFor(UClass* Class, const FString& Preset);
	FString DefaultsKey(FName ClassName, const FString& Preset);

	// --- level -> set ---
	// Every building with its parameters; there is no arrangement-only export.
	bool Gather(const TArray<UHutongBuildingComponent*>& Buildings, UWorld* World,
		FSceneFile& OutFile, FResult& OutResult);

	// --- set <-> file ---
	bool Write(const FSceneFile& File, const FString& FilePath, FResult& OutResult);
	bool Read(const FString& FilePath, FSceneFile& OutFile, FResult& OutResult);

	// --- placement arithmetic.

	// Record's world transform given the set transform.
	FTransform ComposeRecordTransform(const FRecord& Record, const FTransform& SetToWorld);

	// Record's four footprint corners in the set frame, its relative yaw applied, in order.
	void FootprintCornersInSetFrame(const FRecord& Record, FVector2D OutCorners[4]);

	// Shifts offsets so the set origin is the bounds' min corner; outputs the size.
	FVector2D NormaliseToBounds(TArray<FRecord>& Records, FVector2D& OutSize);

	// --- set -> level.
	void Place(UWorld* World, const FSceneFile& File, const FTransform& SetToWorld,
		EMode Mode, FName FolderOverride, FResult& OutResult);

	// --- conveniences; these open their own FScopedTransaction.
	void ExportLoaded(UWorld* World, const FString& FilePath, FResult& OutResult);
	// Every building in the level, loaded or not (World Partition loads the rest first).
	void ExportAll(UWorld* World, const FString& FilePath, FResult& OutResult);
	void ExportSelection(UWorld* World, const FString& FilePath, FResult& OutResult);
	// Resolve runs between read and placement (unknown-type dialog hook); false abandons the import.
	using FResolveFile = TFunction<bool(FSceneFile&)>;

	void ImportAtRecordedTransforms(UWorld* World, const FString& FilePath, EMode Mode,
		FName FolderOverride, FResult& OutResult, const FResolveFile& Resolve = nullptr);

	// Concrete UHutongBuildingComponent subclasses by class name, for resolving record types.
	void GatherBuildingComponentClasses(TMap<FName, UClass*>& OutClasses);

	// Types in the file with no class in this build, with record counts; asked before placing anything.
	void FindUnknownTypes(const FSceneFile& File, TMap<FName, int32>& OutCounts);

	// The file dialogs' filter for scene files.
	inline const TCHAR* SceneFileTypes = TEXT("Hutong scene (*.hutong.json)|*.hutong.json|JSON (*.json)|*.json");

	// Every problem to the log, the summary to a toast.
	void Report(const FResult& Result, const TCHAR* What);
}
