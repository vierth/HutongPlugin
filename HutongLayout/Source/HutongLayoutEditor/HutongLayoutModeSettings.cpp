#include "HutongLayoutModeSettings.h"
#include "ObjectTools.h"
#include "Engine/StaticMesh.h"
#include "Generation/HutongActorSpawn.h"
#include "HutongLayoutEdMode.h"

#include "Tools/HutongDetailOps.h"
#include "Tools/HutongOverlaps.h"
#include "Tools/HutongExchange.h"
#include "Tools/HutongImportTypes.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongPlanOutlineComponent.h"
#include "Generation/HutongStarterMaterials.h"
#include "Tools/HutongPresets.h"

#include "Editor.h"
#include "ScopedTransaction.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/Paths.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace
{
	UWorld* EditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	// Open the dialog on the file this button last used, so a second export offers the first's
	// name to bump. The folder only if it still exists; the name regardless, since the name is
	// what is remembered.
	void DialogDefaults(const FString& Remembered, const TCHAR* FallbackName,
		FString& OutDir, FString& OutName)
	{
		OutDir = FPaths::ProjectSavedDir();
		OutName = FallbackName;
		if (Remembered.IsEmpty()) return;

		const FString Dir = FPaths::GetPath(Remembered);
		if (!Dir.IsEmpty() && FPaths::DirectoryExists(Dir)) OutDir = Dir;
		const FString Name = FPaths::GetCleanFilename(Remembered);
		if (!Name.IsEmpty()) OutName = Name;
	}

	bool PickSaveFile(const FString& DefaultDir, const FString& DefaultName, FString& OutPath)
	{
		IDesktopPlatform* Platform = FDesktopPlatformModule::Get();
		if (!Platform) return false;

		const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		TArray<FString> Files;
		if (!Platform->SaveFileDialog(Parent, TEXT("Export Hutong buildings"),
			DefaultDir, DefaultName, HutongExchange::SceneFileTypes, EFileDialogFlags::None, Files))
		{
			return false;
		}
		if (Files.Num() == 0) return false;
		OutPath = Files[0];
		return true;
	}

	bool PickOpenFile(const FString& DefaultDir, const FString& DefaultName, FString& OutPath)
	{
		IDesktopPlatform* Platform = FDesktopPlatformModule::Get();
		if (!Platform) return false;

		const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		TArray<FString> Files;
		if (!Platform->OpenFileDialog(Parent, TEXT("Import a Hutong scene"),
			DefaultDir, DefaultName, HutongExchange::SceneFileTypes, EFileDialogFlags::None, Files))
		{
			return false;
		}
		if (Files.Num() == 0) return false;
		OutPath = Files[0];
		return true;
	}

}

void UHutongLayoutModeSettings::CreateStarterMaterials()
{
	TArray<FString> Created;
	TArray<FString> Skipped;
	HutongGen::CreateStarterMaterials(Created, Skipped);

	const FText Summary = Created.Num() > 0
		? FText::Format(NSLOCTEXT("HutongLayout", "StarterMaterialsCreated",
			"Created {0} starter materials in {1}. Rebuild Loaded Buildings puts them on what is already placed."),
			FText::AsNumber(Created.Num()), FText::FromString(HutongGen::StarterMaterialPackagePath))
		: NSLOCTEXT("HutongLayout", "StarterMaterialsAllExist",
			"The starter materials already exist; nothing was overwritten.");

	FNotificationInfo Info(Summary);
	Info.ExpireDuration = 6.0f;
	TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
	if (Item.IsValid())
	{
		Item->SetCompletionState(SNotificationItem::CS_Success);
	}
}


void UHutongLayoutModeSettings::FindOverlappingBuildings()
{
	const TArray<UHutongBuildingComponent*> Loaded = HutongDetailOps::CollectLoaded(EditorWorld());
	const TArray<HutongOverlaps::FPair> Pairs = HutongOverlaps::Find(Loaded, Loaded);
	if (Pairs.Num() > 0)
	{
		HutongOverlaps::OpenWindow(Pairs);
		return;
	}
	FNotificationInfo Info(NSLOCTEXT("HutongLayout", "NoOverlaps", "No loaded buildings overlap another by more than 60%."));
	Info.ExpireDuration = 4.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void UHutongLayoutModeSettings::DeleteUnusedGeneratedMeshes()
{
	const TArray<UStaticMesh*> Unused = HutongGen::FindUnusedLibraryMeshes();
	UE_LOG(LogTemp, Display, TEXT("Hutong: %d unused generated mesh(es)."), Unused.Num());
	if (Unused.Num() == 0) return;
	TArray<UObject*> Objects(Unused);
	ObjectTools::DeleteObjects(Objects, /*bShowConfirmation*/ true);
}

TArray<FString> UHutongLayoutModeSettings::GetConvertOptions() const
{
	TArray<FString> Names;
	for (const HutongDetailOps::FConvertTarget& Target : HutongDetailOps::ConvertTargets())
	{
		Names.Add(Target.Label);
	}
	return Names;
}

TArray<FString> UHutongLayoutModeSettings::GetConvertPresetOptions() const
{
	TArray<FString> Names;
	const HutongDetailOps::FConvertTarget Target = HutongDetailOps::FindConvertTarget(ConvertTo);
	if (Target.IsValid())
	{
		if (const UHutongBuildingComponent* CDO = Target.Class->GetDefaultObject<UHutongBuildingComponent>())
		{
			if (const UHutongPresetLibrary* Library = UHutongPresetLibrary::Get())
			{
				Names = Library->GetPresetNames(CDO->GetPresetKey());
			}
		}
	}
	// The type's own defaults are a real answer, and the only one for a type with no presets.
	Names.Insert(FString(), 0);
	return Names;
}






bool UHutongLayoutModeSettings::CanApplyDetailLevel() const
{
	return HutongDetailOps::CollectSelected().ContainsByPredicate(
		[this](const UHutongBuildingComponent* B) { return B->DetailLevel != TargetLevel; });
}

void UHutongLayoutModeSettings::ApplyDetailLevel()
{
	const int32 Changed = HutongDetailOps::SetLevel(HutongDetailOps::CollectSelected(), TargetLevel);
	UE_LOG(LogTemp, Display, TEXT("Hutong: %d building(s) rebuilt at the chosen detail level."), Changed);
	RefreshCounts();
}

bool UHutongLayoutModeSettings::CanApplyConvert() const
{
	// As Detail Level: lit while any selected building is not already the chosen type and preset, so a
	// newly selected building can take the same choice without picking it again.
	const TArray<HutongDetailOps::FConvertTarget>& Targets = HutongDetailOps::ConvertTargets();
	const int32 Index = Targets.IndexOfByPredicate([this](const HutongDetailOps::FConvertTarget& T) { return T.Label == ConvertTo; });
	if (Index == INDEX_NONE) return false;
	// Exactly what Convert acts on: another type, or this type with another named preset.
	return HutongDetailOps::CollectSelected().ContainsByPredicate([&](const UHutongBuildingComponent* B)
	{
		return HutongDetailOps::FindConvertTargetIndex(B) != Index || (!ConvertPreset.IsEmpty() && B->Preset != ConvertPreset);
	});
}

void UHutongLayoutModeSettings::ApplyConvert()
{
	const HutongDetailOps::FConvertTarget Target = HutongDetailOps::FindConvertTarget(ConvertTo);
	if (!Target.IsValid()) return;
	const int32 Changed = HutongDetailOps::Convert(HutongDetailOps::CollectSelected(), Target, ConvertPreset);
	UE_LOG(LogTemp, Display, TEXT("Hutong: %d building(s) converted to %s."), Changed, *Target.Label);
	RefreshCounts();
}

bool UHutongLayoutModeSettings::CanApplyDivide() const
{
	return HutongDetailOps::CollectSelected().ContainsByPredicate(
		[](const UHutongBuildingComponent* B) { return HutongDetailOps::CanDivide(B); });
}

void UHutongLayoutModeSettings::ApplyDivide()
{
	int32 Divided = 0;
	FText WhyNot;
	{
		const FScopedTransaction Transaction(NSLOCTEXT("HutongDetailOps", "DivideBuildings", "Divide Hutong Buildings"));
		for (UHutongBuildingComponent* B : HutongDetailOps::CollectSelected())
		{
			if (!HutongDetailOps::CanDivide(B)) continue;
			FHutongPlanBays Bays;
			B->GetPlanBays(Bays);
			const int32 Line = DivideAtBayLine > 0 ? DivideAtBayLine : (Bays.Boundaries.Num() - 1) / 2;
			if (HutongDetailOps::DivideBuilding(B, Line, WhyNot)) ++Divided;
		}
	}
	if (Divided == 0 && !WhyNot.IsEmpty()) { UE_LOG(LogTemp, Warning, TEXT("Hutong: %s"), *WhyNot.ToString()); }
	UE_LOG(LogTemp, Display, TEXT("Hutong: %d building(s) divided."), Divided);
	RefreshCounts();
}

bool UHutongLayoutModeSettings::CanFuse() const
{
	return HutongDetailOps::CollectSelected().Num() == 2;
}

void UHutongLayoutModeSettings::FuseSelection()
{
	FText Message;
	if (HutongDetailOps::FuseSelected(Message)) { UE_LOG(LogTemp, Display, TEXT("Hutong: %s"), *Message.ToString()); }
	else { UE_LOG(LogTemp, Warning, TEXT("Hutong: %s"), *Message.ToString()); }
	RefreshCounts();
}

void UHutongLayoutModeSettings::SyncToSelection()
{
	// Mixed levels leave the dropdown where it was.
	const TArray<UHutongBuildingComponent*> Picked = HutongDetailOps::CollectSelected();
	if (Picked.Num() > 0 && !Picked.ContainsByPredicate([&](const UHutongBuildingComponent* B) { return B->DetailLevel != Picked[0]->DetailLevel; }))
	{
		TargetLevel = Picked[0]->DetailLevel;
	}
}

void UHutongLayoutModeSettings::GenerateLoadedGeometry()
{
	const int32 Built = HutongDetailOps::GeneratePlanned(HutongDetailOps::CollectLoaded(EditorWorld()));
	UE_LOG(LogTemp, Display, TEXT("Hutong: geometry generated for %d laid-out building(s)."), Built);
	RefreshCounts();
}

void UHutongLayoutModeSettings::GenerateSelectedGeometry()
{
	const int32 Built = HutongDetailOps::GeneratePlanned(HutongDetailOps::CollectSelected());
	UE_LOG(LogTemp, Display, TEXT("Hutong: geometry generated for %d selected laid-out building(s)."), Built);
	RefreshCounts();
}

void UHutongLayoutModeSettings::RevertLoadedToLayout()
{
	const int32 Reverted = HutongDetailOps::RevertToPlan(HutongDetailOps::CollectLoaded(EditorWorld()));
	UE_LOG(LogTemp, Display, TEXT("Hutong: %d loaded building(s) reverted to layout."), Reverted);
	RefreshCounts();
}

void UHutongLayoutModeSettings::RevertSelectedToLayout()
{
	const int32 Reverted = HutongDetailOps::RevertToPlan(HutongDetailOps::CollectSelected());
	UE_LOG(LogTemp, Display, TEXT("Hutong: %d selected building(s) reverted to layout."), Reverted);
	RefreshCounts();
}

void UHutongLayoutModeSettings::RebuildSelection()
{
	const int32 Changed = HutongDetailOps::Rebuild(HutongDetailOps::CollectSelected());
	UE_LOG(LogTemp, Display, TEXT("Hutong: %d building(s) rebuilt from their parameters."), Changed);
	RefreshCounts();
}

void UHutongLayoutModeSettings::RebuildLoaded()
{
	const int32 Changed = HutongDetailOps::Rebuild(HutongDetailOps::CollectLoaded(EditorWorld()));
	UE_LOG(LogTemp, Display, TEXT("Hutong: %d loaded building(s) rebuilt from their parameters."),
		Changed);
	RefreshCounts();
}

void UHutongLayoutModeSettings::DoExport(bool bSelection, const TCHAR* FallbackName, FString& Remembered)
{
	FString Dir, Name;
	DialogDefaults(Remembered, FallbackName, Dir, Name);
	// A fresh time-stamped name each time: an export never lands on an older file unasked.
	Name = FString::Printf(TEXT("%s_%s.%s"), FallbackName, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), HutongExchange::FileExtension);

	FString Path;
	if (!PickSaveFile(Dir, Name, Path)) return;

	HutongExchange::FResult Result;
	if (bSelection) HutongExchange::ExportSelection(EditorWorld(), Path, Result);
	else            HutongExchange::ExportAll(EditorWorld(), Path, Result);
	HutongExchange::Report(Result, TEXT("export"));

	// Remember only after a successful write, so a failed export does not aim the next dialog at
	// a file never written.
	if (Result.bSucceeded)
	{
		Remembered = Path;
		LastSceneFile = Path;
		SaveConfig();
	}
}

void UHutongLayoutModeSettings::ExportAll()
{
	DoExport(/*bSelection*/ false, TEXT("hutong_layout"), LastExportFile);
}

void UHutongLayoutModeSettings::ExportSelection()
{
	DoExport(/*bSelection*/ true, TEXT("hutong_selection"), LastSelectionExportFile);
}

void UHutongLayoutModeSettings::ImportLayout()
{
	if (bCustomizePlacement) PlaceSceneByHand();
	else ImportAtRecordedCoordinates();
}

bool UHutongLayoutModeSettings::HasSelection() const
{
	return HutongDetailOps::CollectSelected().Num() > 0;
}

bool UHutongLayoutModeSettings::HasSelectedLayout() const
{
	return HutongDetailOps::CollectSelected().ContainsByPredicate([](const UHutongBuildingComponent* B) { return B->bPlanOnly; });
}

bool UHutongLayoutModeSettings::HasSelectedBuilt() const
{
	return HutongDetailOps::CollectSelected().ContainsByPredicate([](const UHutongBuildingComponent* B) { return !B->bPlanOnly; });
}

void UHutongLayoutModeSettings::ImportAtRecordedCoordinates()
{
	FString Dir, Name;
	DialogDefaults(LastSceneFile, TEXT(""), Dir, Name);

	FString Path;
	if (!PickOpenFile(Dir, Name, Path)) return;

	HutongExchange::FResult Result;
	HutongExchange::ImportAtRecordedTransforms(EditorWorld(), Path,
		bUpdateMatchingPlacements ? HutongExchange::EMode::Sync : HutongExchange::EMode::Additive,
		ImportFolder, Result, &HutongImportTypes::ResolveUnknownTypes);
	HutongExchange::Report(Result, TEXT("import"));

	if (Result.bSucceeded) { LastSceneFile = Path; SaveConfig(); }
	HutongOverlaps::CheckAfterImport(Result.Placed);

	// Select the set as it lands so the move/rotate gizmos turn it as one: coordinates from
	// another level mean nothing here.
	if (Result.bSucceeded && Result.Placed.Num() > 0 && GEditor)
	{
		GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDeselectBSPSurfs*/ true);
		for (const TWeakObjectPtr<AActor>& Actor : Result.Placed)
		{
			if (AActor* A = Actor.Get())
			{
				GEditor->SelectActor(A, /*bInSelected*/ true, /*bNotify*/ false);
			}
		}
		GEditor->NoteSelectionChange();
		UE_LOG(LogTemp, Display,
			TEXT("Hutong import: %d actor(s) selected — move or rotate them as one set, or use "
				 "Place By Hand to drop the next one under the cursor."),
			Result.Placed.Num());
	}
	RefreshCounts();
}

void UHutongLayoutModeSettings::PlaceSceneByHand()
{
	FString Dir, Name;
	DialogDefaults(LastSceneFile, TEXT(""), Dir, Name);

	FString Path;
	if (!PickOpenFile(Dir, Name, Path)) return;

	// The Import tool seeds itself from this on start: the file is chosen here, the placement
	// is the tool's.
	LastSceneFile = Path;
	SaveConfig();
	if (!UHutongLayoutEdMode::StartTool(TEXT("HutongImportTool")))
	{
		UE_LOG(LogTemp, Warning, TEXT("Hutong: could not start the Import tool."));
	}
}

void UHutongLayoutModeSettings::RefreshCounts()
{
	LoadedBuildings = 0;
	LoadedTriangles = 0;

	for (const UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(EditorWorld()))
	{
		++LoadedBuildings;

		// Off the baked mesh rather than by rebuilding one.
		const AStaticMeshActor* Actor = Cast<AStaticMeshActor>(B->GetOwner());
		const UStaticMeshComponent* Comp = Actor ? Actor->GetStaticMeshComponent() : nullptr;
		const UStaticMesh* Mesh = Comp ? Comp->GetStaticMesh() : nullptr;
		if (Mesh && Mesh->GetNumLODs() > 0)
		{
			LoadedTriangles += Mesh->GetNumTriangles(0);
		}
	}
}

void UHutongLayoutModeSettings::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);

	if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHutongLayoutModeSettings, bShowPlanOutlines))
	{
		HutongPlanOutline::SetPlansVisible(bShowPlanOutlines);
	}
	if (Event.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHutongLayoutModeSettings, bPlansOverBuildings))
	{
		HutongPlanOutline::SetPlansOverBuildings(bPlansOverBuildings);
	}
}
