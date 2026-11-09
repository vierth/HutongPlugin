#include "Tools/HutongCourts.h"
#include "Tools/HutongDetailOps.h"
#include "Generation/HutongBuildingComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/Selection.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Internationalization/Regex.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "HutongCourts"

namespace
{
	FVector FootprintCentre(const UHutongBuildingComponent* B)
	{
		FVector2D Q[4];
		B->GetFootprintCorners(Q);
		const FVector2D Mid = 0.25 * (Q[0] + Q[1] + Q[2] + Q[3]);
		return B->GetOwner()->GetActorTransform().TransformPosition(FVector(Mid.X, Mid.Y, 0.0));
	}
}

namespace HutongCourts
{
	AStaticMeshActor* MapTileActorAt(UWorld* World, const FVector& Point, FString* OutName)
	{
		if (!World) return nullptr;
		// Sheet names of 《乾隆京城全圖》 as the map tiles carry them: row, sheet letter, column (3M6, 4M10_2).
		const FRegexPattern TilePattern(TEXT("^[0-9]+[A-Z][0-9]+(_[0-9]+)?$"));
		auto IsTile = [&TilePattern](const FString& Candidate) { FRegexMatcher M(TilePattern, Candidate); return M.FindNext(); };

		FString Best;
		AStaticMeshActor* BestActor = nullptr;
		double BestArea = TNumericLimits<double>::Max();
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			AStaticMeshActor* Actor = *It;
			if (Actor->FindComponentByClass<UHutongBuildingComponent>()) continue;
			const FBox Box = Actor->GetComponentsBoundingBox();
			if (!Box.IsValid || Point.X < Box.Min.X || Point.X > Box.Max.X || Point.Y < Box.Min.Y || Point.Y > Box.Max.Y) continue;
			FString Name;
			if (IsTile(Actor->GetActorLabel())) Name = Actor->GetActorLabel();
			else if (const UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent())
			{
				for (int32 i = 0; i < Mesh->GetNumMaterials() && Name.IsEmpty(); ++i)
				{
					const UMaterialInterface* M = Mesh->GetMaterial(i);
					if (M && IsTile(M->GetName())) Name = M->GetName();
				}
			}
			if (Name.IsEmpty()) continue;
			const double Area = Box.GetSize().X * Box.GetSize().Y;
			if (Area < BestArea) { BestArea = Area; Best = Name; BestActor = Actor; }
		}
		if (OutName) *OutName = Best;
		return BestActor;
	}

	FString MapTileAt(UWorld* World, const FVector& Point)
	{
		FString Name;
		MapTileActorAt(World, Point, &Name);
		return Name;
	}

	FParented ParentToTiles(UWorld* World, const TArray<UHutongBuildingComponent*>& Buildings)
	{
		FParented Out;
		if (!GEditor) return Out;
		const FScopedTransaction Transaction(LOCTEXT("ParentToTiles", "Parent Buildings To Map Tiles"));
		for (UHutongBuildingComponent* B : Buildings)
		{
			AActor* Actor = B ? B->GetOwner() : nullptr;
			if (!Actor) continue;
			AStaticMeshActor* Tile = MapTileActorAt(World ? World : Actor->GetWorld(), FootprintCentre(B));
			if (!Tile) { ++Out.OffMap; continue; }
			if (Actor->GetAttachParentActor() == Tile) { ++Out.Already; continue; }
			if (!GEditor->CanParentActors(Tile, Actor)) { ++Out.Refused; continue; }
			// Takes the tile's Outliner folder too.
			GEditor->ParentActors(Tile, Actor, NAME_None);
			++Out.Parented;
		}
		return Out;
	}

	int32 DetachFromTiles(UWorld* World, const TArray<UHutongBuildingComponent*>& Buildings)
	{
		const FScopedTransaction Transaction(LOCTEXT("DetachFromTiles", "Detach Buildings From Map Tiles"));
		int32 Detached = 0;
		for (UHutongBuildingComponent* B : Buildings)
		{
			AActor* Actor = B ? B->GetOwner() : nullptr;
			AActor* Parent = Actor ? Actor->GetAttachParentActor() : nullptr;
			if (!Parent || Parent->FindComponentByClass<UHutongBuildingComponent>() || !Cast<AStaticMeshActor>(Parent)) continue;
			Actor->Modify();
			Parent->Modify(/*bAlwaysMarkDirty*/ false);
			Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			Actor->SetFolderPath(FolderFor(World ? World : Actor->GetWorld(), B));
			++Detached;
		}
		return Detached;
	}

	FString NextCourtName(const FString& Prefix, const TArray<FString>& Existing)
	{
		int32 Highest = 0;
		const FString Head = Prefix + TEXT("_");
		for (const FString& Name : Existing)
		{
			if (!Name.StartsWith(Head)) continue;
			const FString Tail = Name.RightChop(Head.Len());
			if (Tail.IsNumeric()) Highest = FMath::Max(Highest, FCString::Atoi(*Tail));
		}
		return FString::Printf(TEXT("%s_%d"), *Prefix, Highest + 1);
	}

	FString SuggestName(UWorld* World, const TArray<UHutongBuildingComponent*>& Buildings)
	{
		FVector Centre = FVector::ZeroVector;
		int32 Count = 0;
		for (const UHutongBuildingComponent* B : Buildings)
		{
			if (!B || !B->GetOwner()) continue;
			Centre += FootprintCentre(B);
			++Count;
		}
		const FString Tile = Count > 0 ? MapTileAt(World, Centre / Count) : FString();
		return NextCourtName(Tile.IsEmpty() ? TEXT("Courtyard") : Tile + TEXT("_Courtyard"), AllCourts(World));
	}

	TArray<FString> AllCourts(UWorld* World)
	{
		TArray<FString> Out;
		for (const UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(World))
		{
			for (const FString& Name : B->GetCourts()) Out.AddUnique(Name);
		}
		Out.Sort();
		return Out;
	}

	TArray<FSummary> Summaries(UWorld* World)
	{
		TMap<FString, FSummary> ByName;
		TMap<FString, FVector> Where;
		for (const UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(World))
		{
			for (const FString& Name : B->GetCourts())
			{
				FSummary& S = ByName.FindOrAdd(Name);
				S.Name = Name;
				++S.Buildings;
				if (!Where.Contains(Name) || !B->CanShareCourts()) Where.Add(Name, FootprintCentre(B));
			}
		}
		TArray<FSummary> Out;
		for (TPair<FString, FSummary>& It : ByName)
		{
			const FString Tile = MapTileAt(World, Where[It.Key]);
			It.Value.Tile = Tile.IsEmpty() ? FString(OffMapFolder) : Tile;
			Out.Add(It.Value);
		}
		Out.Sort([](const FSummary& A, const FSummary& B) { return A.Tile != B.Tile ? A.Tile < B.Tile : A.Name < B.Name; });
		return Out;
	}

	int32 Remove(const TArray<UHutongBuildingComponent*>& Buildings, const FString& Name)
	{
		TArray<UHutongBuildingComponent*> In = Buildings.FilterByPredicate([&Name](const UHutongBuildingComponent* B) { return B->IsInCourt(Name); });
		if (In.Num() == 0) return 0;
		const FScopedTransaction Transaction(LOCTEXT("RemoveFromCourt", "Remove From Courtyard Unit"));
		for (UHutongBuildingComponent* B : In)
		{
			B->Modify();
			B->RemoveCourt(Name);
		}
		FileInFolders(In[0]->GetOwner() ? In[0]->GetOwner()->GetWorld() : nullptr, In);
		return In.Num();
	}

	int32 Assign(const TArray<UHutongBuildingComponent*>& Buildings, const FString& Name)
	{
		if (Buildings.Num() == 0) return 0;
		const FString Trimmed = Name.TrimStartAndEnd();
		const FScopedTransaction Transaction(LOCTEXT("AssignCourt", "Assign Courtyard Unit"));
		UWorld* World = nullptr;
		for (UHutongBuildingComponent* B : Buildings)
		{
			B->Modify();
			B->AssignCourt(Trimmed);
			if (!World && B->GetOwner()) World = B->GetOwner()->GetWorld();
		}
		FileInFolders(World, Buildings);
		return Buildings.Num();
	}

	TSet<FString> CourtsOf(const TArray<UHutongBuildingComponent*>& Buildings)
	{
		const bool bOnlyWalls = !Buildings.ContainsByPredicate([](const UHutongBuildingComponent* B) { return !B->CanShareCourts(); });
		TSet<FString> Courts;
		for (const UHutongBuildingComponent* B : Buildings)
		{
			if (bOnlyWalls || !B->CanShareCourts()) Courts.Append(B->GetCourts());
		}
		return Courts;
	}

	int32 SelectCourts(UWorld* World, const TSet<FString>& Courts, bool bAdd)
	{
		if (!GEditor || Courts.Num() == 0) return 0;
		const FScopedTransaction Transaction(LOCTEXT("SelectCourt", "Select Courtyard Unit"));
		GEditor->GetSelectedActors()->BeginBatchSelectOperation();
		if (!bAdd) GEditor->SelectNone(false, true);
		int32 Count = 0;
		for (UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(World))
		{
			if (B->GetOwner() && B->GetCourts().ContainsByPredicate([&Courts](const FString& Court) { return Courts.Contains(Court); }))
			{
				GEditor->SelectActor(B->GetOwner(), true, false, true);
				++Count;
			}
		}
		GEditor->GetSelectedActors()->EndBatchSelectOperation();
		GEditor->NoteSelectionChange();
		return Count;
	}

	FName FolderFor(UWorld* World, const UHutongBuildingComponent* Building)
	{
		const TArray<FString> Courts = Building ? Building->GetCourts() : TArray<FString>();
		if (Courts.Num() == 0 || !Building->GetOwner()) return NAME_None;
		FString Tile = MapTileAt(World, FootprintCentre(Building));
		if (Tile.IsEmpty()) Tile = OffMapFolder;
		const FString Leaf = Courts.Num() > 1 ? FString(SharedWallsFolder) : Courts[0];
		return FName(*FString::Printf(TEXT("%s/%s/%s"), RootFolder, *Tile, *Leaf));
	}

	int32 FileInFolders(UWorld* World, const TArray<UHutongBuildingComponent*>& Buildings)
	{
		int32 Moved = 0;
		for (UHutongBuildingComponent* B : Buildings)
		{
			AActor* Actor = B ? B->GetOwner() : nullptr;
			// An attached actor sits under its parent in the Outliner whatever its folder.
			if (!Actor || Actor->GetAttachParentActor()) continue;
			FName Folder = FolderFor(World ? World : Actor->GetWorld(), B);
			// Folders of the user's own stay.
			if (Folder.IsNone() && !Actor->GetFolderPath().ToString().StartsWith(FString(RootFolder) + TEXT("/"))) continue;
			if (Actor->GetFolderPath() == Folder) continue;
			Actor->Modify();
			Actor->SetFolderPath(Folder);
			++Moved;
		}
		return Moved;
	}
}

#undef LOCTEXT_NAMESPACE
