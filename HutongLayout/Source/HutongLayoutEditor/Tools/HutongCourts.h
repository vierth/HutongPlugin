#pragma once

#include "CoreMinimal.h"

class UWorld;
class UHutongBuildingComponent;
class AStaticMeshActor;

namespace HutongCourts
{
	FString MapTileAt(UWorld* World, const FVector& Point);

	AStaticMeshActor* MapTileActorAt(UWorld* World, const FVector& Point, FString* OutName = nullptr);

	struct FParented
	{
		int32 Parented = 0;
		int32 Already = 0;
		int32 OffMap = 0;
		int32 Refused = 0;
	};
	// Each building onto the tile under its footprint centre, so moving the tile carries it.
	FParented ParentToTiles(UWorld* World, const TArray<UHutongBuildingComponent*>& Buildings);

	// Back to the court folder, or the top level.
	int32 DetachFromTiles(UWorld* World, const TArray<UHutongBuildingComponent*>& Buildings);

	FString NextCourtName(const FString& Prefix, const TArray<FString>& Existing);

	FString SuggestName(UWorld* World, const TArray<UHutongBuildingComponent*>& Buildings);

	TArray<FString> AllCourts(UWorld* World);

	struct FSummary
	{
		FString Name;
		FString Tile;
		int32 Buildings = 0;
	};
	TArray<FSummary> Summaries(UWorld* World);

	int32 Remove(const TArray<UHutongBuildingComponent*>& Buildings, const FString& Name);

	int32 Assign(const TArray<UHutongBuildingComponent*>& Buildings, const FString& Name);

	int32 SelectCourts(UWorld* World, const TSet<FString>& Courts, bool bAdd);

	// A shared wall's courts count only when walls alone are selected: a house and its wall would pull in the court next door.
	TSet<FString> CourtsOf(const TArray<UHutongBuildingComponent*>& Buildings);

	FName FolderFor(UWorld* World, const UHutongBuildingComponent* Building);

	int32 FileInFolders(UWorld* World, const TArray<UHutongBuildingComponent*>& Buildings);

	inline const TCHAR* RootFolder = TEXT("Courts");
	inline const TCHAR* SharedWallsFolder = TEXT("Shared walls");
	inline const TCHAR* OffMapFolder = TEXT("Off map");
}
