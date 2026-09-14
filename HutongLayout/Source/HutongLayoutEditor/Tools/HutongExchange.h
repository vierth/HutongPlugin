#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Generation/BaySide.h"
#include "Generation/HutongDetail.h"
#include "Generation/HutongFootprint.h"

class AActor;
class UHutongBuildingComponent;
class UWorld;

// Placed buildings to a file and back.
namespace HutongExchange
{
	inline constexpr int32 FormatVersion = 1;

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

		// What the import actually put down or reshaped, so a caller can select the set and let the
		// ordinary gizmo move and turn it as one — which is what a file landing in a level whose
		// coordinates are not the ones it was written in needs.
		TArray<TWeakObjectPtr<AActor>> Placed;

		// Set on an import from a layout-only file: the parameters in the level are the types'
		// current defaults, not the ones the export was made from, and a report that does not say
		// so reads as tuning lost.
		bool bFromLayoutOnly = false;

		FText Summarise() const;
	};

	// One placed building as it survives the trip.
	struct FRecord
	{
		FGuid Id;
		// The component class's own name, e.g.
		FName ClassName;
		FString Label;
		FName Folder;

		FVector2D Offset = FVector2D::ZeroVector;
		double OffsetZ = 0.0;
		double RelativeYawDeg = 0.0;

		// GetFootprintSize() as it stood at export.
		FVector2D Footprint = FVector2D::ZeroVector;
		// The corner offsets as they stood, for the import ghost and the set's bounds only: the
		// component blob and the Footprint-category fields are the authority on the way back in.
		FHutongFootprintSkew Skew;

		// The placement's own decisions, written whether or not the parameters are. On a layout-only
		// record these are all there is: the type, where it stands, how big it is, which way it
		// faces, and what it was placed at.
		bool bHasFacing = false;
		EHutongBaySide Facing = EHutongBaySide::MinusY;
		EHutongDetail Detail = EHutongDetail::Near;
		bool bPlanOnly = false;
		// Which way a line-like piece runs. Without it a layout-only record cannot say what its
		// footprint means: `SetFootprintSize` takes the length off the run's own axis, so a wall
		// drawn down Y came back with its thickness for a length — a stub.
		bool bRunAlongY = false;

		// The kind within the class — the wall's 院牆/隔牆 role — where a class carries more than one.
		FName Variant;

		// The component's own Footprint-category fields: length, width, run axis, facade side,
		// bay override, the wall's miters. Everything a placement decided that is not a parameter,
		// taken by reflection so a field added to that category travels the day it is added.
		TSharedPtr<FJsonObject> FootprintFields;

		// The parameters. Null on a layout-only record, which is the whole point of one.
		TSharedPtr<FJsonObject> Blob;
	};

	struct FSceneFile
	{
		int32 Version = FormatVersion;
		FString LevelName;

		// The arrangement without the parameters: type, footprint, position, yaw and facing, and
		// nothing about how any of them is built. Re-importing one rebuilds the street from the
		// types' current defaults, which is what makes it the file to iterate against — change a
		// generator or a canon figure and the same plan comes back built the new way.
		bool bLayoutOnly = false;

		// Where the set frame stood in the level it came from, which is all a placement at the recorded coordinates needs.
		FVector SetOriginWorld = FVector::ZeroVector;
		double SetYawDeg = 0.0;

		FVector2D BoundsSize = FVector2D::ZeroVector;

		TArray<FRecord> Records;

		// What to place for a type this build does not have: old class name -> the class to build
		// instead, or NAME_None to skip it. Filled in by the import dialog before Place runs, so
		// a file written before a type was split or renamed still lands.
		TMap<FName, FName> TypeRemap;
	};

	enum class EMode : uint8
	{
		// Always spawn, always a fresh id — dropping a copy of somebody else's street beside your own.
		Additive,
		// Match on the id: update that building in place, spawn what is missing.
		Sync,
	};

	// --- the reflected halves, public because the tests drive them directly ---
	TSharedPtr<FJsonObject> WriteComponent(const UHutongBuildingComponent* Component);
	bool ReadComponent(const TSharedRef<FJsonObject>& Blob, UHutongBuildingComponent* Component,
		FString& OutProblem);

	// --- level -> set ---
	bool Gather(const TArray<UHutongBuildingComponent*>& Buildings, UWorld* World,
		FSceneFile& OutFile, FResult& OutResult, bool bLayoutOnly = false);

	// --- set <-> file ---
	bool Write(const FSceneFile& File, const FString& FilePath, FResult& OutResult);
	bool Read(const FString& FilePath, FSceneFile& OutFile, FResult& OutResult);

	// --- placement arithmetic.

	// Where one record lands given the set's own transform.
	FTransform ComposeRecordTransform(const FRecord& Record, const FTransform& SetToWorld);

	// The record's four footprint corners in the set frame, carrying its own relative yaw, in order.
	void FootprintCornersInSetFrame(const FRecord& Record, FVector2D OutCorners[4]);

	// Slides every offset so the set frame's origin is the bounding rectangle's min corner, and reports the size.
	FVector2D NormaliseToBounds(TArray<FRecord>& Records, FVector2D& OutSize);

	// --- set -> level.
	void Place(UWorld* World, const FSceneFile& File, const FTransform& SetToWorld,
		EMode Mode, FName FolderOverride, FResult& OutResult);

	// --- the conveniences, which do open their own FScopedTransaction.
	void ExportLoaded(UWorld* World, const FString& FilePath, FResult& OutResult,
		bool bLayoutOnly = false);
	void ExportSelection(UWorld* World, const FString& FilePath, FResult& OutResult,
		bool bLayoutOnly = false);
	// Resolve is given the file between the read and the placement — the hook the unknown-type
	// dialog hangs on. Returning false abandons the import.
	using FResolveFile = TFunction<bool(FSceneFile&)>;

	void ImportAtRecordedTransforms(UWorld* World, const FString& FilePath, EMode Mode,
		FName FolderOverride, FResult& OutResult, const FResolveFile& Resolve = nullptr);

	// Every concrete UHutongBuildingComponent subclass, keyed by class name, for resolving a record's type on import.
	void GatherBuildingComponentClasses(TMap<FName, UClass*>& OutClasses);

	// The types this file names that this build has no class for, with how many records each has.
	// Answered before anything is placed, so the user can say what to build instead.
	void FindUnknownTypes(const FSceneFile& File, TMap<FName, int32>& OutCounts);
}
