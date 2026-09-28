#include "HutongLayoutModeSettings.h"
#include "ObjectTools.h"
#include "Engine/StaticMesh.h"
#include "Generation/HutongActorSpawn.h"
#include "HutongLayoutEdMode.h"

#include "Tools/HutongDetailOps.h"
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

	bool PickOpenFile(const FString& DefaultDir, FString& OutPath)
	{
		IDesktopPlatform* Platform = FDesktopPlatformModule::Get();
		if (!Platform) return false;

		const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		TArray<FString> Files;
		if (!Platform->OpenFileDialog(Parent, TEXT("Import a Hutong scene"),
			DefaultDir, FString(), HutongExchange::SceneFileTypes, EFileDialogFlags::None, Files))
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

void UHutongLayoutModeSettings::PromoteSelection()
{
	const int32 Changed = HutongDetailOps::SetLevel(HutongDetailOps::CollectSelected(), TargetLevel);
	UE_LOG(LogTemp, Display, TEXT("Hutong: %d building(s) rebuilt at the target detail."), Changed);
	RefreshCounts();
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

void UHutongLayoutModeSettings::ConvertSelection()
{
	const HutongDetailOps::FConvertTarget Target = HutongDetailOps::FindConvertTarget(ConvertTo);
	if (!Target.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Hutong: pick a type in Convert To first."));
		return;
	}

	const int32 Changed = HutongDetailOps::Convert(
		HutongDetailOps::CollectSelected(), Target, ConvertPreset);
	UE_LOG(LogTemp, Display, TEXT("Hutong: %d building(s) converted to %s."), Changed, *Target.Label);
	RefreshCounts();
}

void UHutongLayoutModeSettings::DivideSelection()
{
	const TArray<UHutongBuildingComponent*> Picked = HutongDetailOps::CollectSelected();
	int32 Divided = 0;
	FText WhyNot;
	{
		const FScopedTransaction Transaction(NSLOCTEXT("HutongDetailOps", "DivideBuildings", "Divide Hutong Buildings"));
		for (UHutongBuildingComponent* B : Picked)
		{
			if (!HutongDetailOps::CanDivide(B)) continue;
			FHutongPlanBays Bays;
			B->GetPlanBays(Bays);
			const int32 Count = Bays.Boundaries.Num() - 1;
			const int32 Line = DivideAtBayLine > 0 ? DivideAtBayLine : Count / 2;
			if (HutongDetailOps::DivideBuilding(B, Line, WhyNot)) ++Divided;
		}
	}
	if (Divided == 0 && !WhyNot.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Hutong: %s"), *WhyNot.ToString());
	}
	UE_LOG(LogTemp, Display, TEXT("Hutong: %d building(s) divided."), Divided);
	RefreshCounts();
}

void UHutongLayoutModeSettings::FuseSelection()
{
	FText Message;
	const bool bFused = HutongDetailOps::FuseSelected(Message);
	if (bFused)
	{
		UE_LOG(LogTemp, Display, TEXT("Hutong: %s"), *Message.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Hutong: %s"), *Message.ToString());
	}
	RefreshCounts();
}

void UHutongLayoutModeSettings::DemoteSelectionToMassing()
{
	const int32 Changed = HutongDetailOps::SetLevel(
		HutongDetailOps::CollectSelected(), EHutongDetail::Massing);
	UE_LOG(LogTemp, Display, TEXT("Hutong: %d building(s) demoted to massing."), Changed);
	RefreshCounts();
}

void UHutongLayoutModeSettings::SetLoadedRegionToTarget()
{
	const int32 Changed = HutongDetailOps::SetLevel(
		HutongDetailOps::CollectLoaded(EditorWorld()), TargetLevel);
	UE_LOG(LogTemp, Display, TEXT("Hutong: %d loaded building(s) rebuilt at the target detail."),
		Changed);
	RefreshCounts();
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

	FString Path;
	if (!PickSaveFile(Dir, Name, Path)) return;

	HutongExchange::FResult Result;
	if (bSelection) HutongExchange::ExportSelection(EditorWorld(), Path, Result);
	else            HutongExchange::ExportLoaded(EditorWorld(), Path, Result);
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

void UHutongLayoutModeSettings::ExportLoaded()
{
	DoExport(/*bSelection*/ false, TEXT("HutongScene.hutong.json"), LastExportFile);
}

void UHutongLayoutModeSettings::ExportSelection()
{
	DoExport(/*bSelection*/ true, TEXT("HutongSelection.hutong.json"), LastSelectionExportFile);
}

void UHutongLayoutModeSettings::ImportAtRecordedCoordinates()
{
	FString Dir, Name;
	DialogDefaults(LastSceneFile, TEXT(""), Dir, Name);

	FString Path;
	if (!PickOpenFile(Dir, Path)) return;

	HutongExchange::FResult Result;
	HutongExchange::ImportAtRecordedTransforms(EditorWorld(), Path,
		bUpdateMatchingPlacements ? HutongExchange::EMode::Sync : HutongExchange::EMode::Additive,
		ImportFolder, Result, &HutongImportTypes::ResolveUnknownTypes);
	HutongExchange::Report(Result, TEXT("import"));

	if (Result.bSucceeded) { LastSceneFile = Path; SaveConfig(); }

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
	if (!PickOpenFile(Dir, Path)) return;

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
}
