#pragma once

#include "CoreMinimal.h"

class UWorld;
class UHutongBuildingComponent;

// Courtyard units (院落), shared by the Courts and Heights tools: naming from the map tile, assigning,
// selecting a whole court, and filing each building in the Outliner under its court.
namespace HutongCourts
{
	// A map tile's name (3M6, 4M10_2), or empty: the actor label or a material of a static mesh actor
	// (not a building) whose bounds contain the point; the smallest such where tiles overlap.
	FString MapTileAt(UWorld* World, const FVector& Point);

	// `<Prefix>_<n>` with n one past the highest already used.
	FString NextCourtName(const FString& Prefix, const TArray<FString>& Existing);

	// The next free `<tile>_Courtyard_<n>` for these buildings (Courtyard_<n> off the map).
	FString SuggestName(UWorld* World, const TArray<UHutongBuildingComponent*>& Buildings);

	// Every court named by a loaded building, sorted.
	TArray<FString> AllCourts(UWorld* World);

	// A court as the Courts window lists it: its tile (Off map without one) and how many buildings.
	struct FSummary
	{
		FString Name;
		FString Tile;
		int32 Buildings = 0;
	};
	TArray<FSummary> Summaries(UWorld* World);

	// Takes the buildings out of that court (a wall keeps any other) and files them again. One undo
	// step. Returns how many were in it.
	int32 Remove(const TArray<UHutongBuildingComponent*>& Buildings, const FString& Name);

	// Puts the buildings in the court (a wall adds it, anything else moves; empty clears) and files each
	// in the Outliner. One undo step. Returns how many.
	int32 Assign(const TArray<UHutongBuildingComponent*>& Buildings, const FString& Name);

	// Selects every loaded building in any of these courts; bAdd keeps the current selection.
	int32 SelectCourts(UWorld* World, const TSet<FString>& Courts, bool bAdd);

	// The courts a selection means: its non-wall buildings' courts, a shared wall's only when walls
	// alone are selected (a house and the wall beside it would pull in the court next door).
	TSet<FString> CourtsOf(const TArray<UHutongBuildingComponent*>& Buildings);

	// Courts/<tile>/<court>, a wall between courts under Courts/<tile>/Shared walls, NAME_None for a
	// building in no court. The tile is the one under the building (Off map without one).
	FName FolderFor(UWorld* World, const UHutongBuildingComponent* Building);

	// Moves the buildings to FolderFor; one in no court leaves the Courts folders for the top level (a
	// folder of the user's own is left alone). Returns how many moved.
	int32 FileInFolders(UWorld* World, const TArray<UHutongBuildingComponent*>& Buildings);

	inline const TCHAR* RootFolder = TEXT("Courts");
	inline const TCHAR* SharedWallsFolder = TEXT("Shared walls");
	inline const TCHAR* OffMapFolder = TEXT("Off map");
}
