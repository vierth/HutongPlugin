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
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Tools/HutongAudit.h"
#include "Tools/HutongSharing.h"
#include "Tools/HutongCourts.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace
{
	using HutongDetailOps::EditorWorld;

	// Folder only while it still exists.
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

void UHutongLayoutModeSettings::AuditLoaded()
{
	TArray<HutongAudit::FAudited> Audited;
	for (const UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(EditorWorld()))
	{
		Audited.Add({ B, HutongAudit::Audit(B, TracedOutline) });
	}
	FString Summary = HutongAudit::SummaryTable(Audited);
	UE_LOG(LogTemp, Display, TEXT("Hutong audit:\n%s"), *Summary);

	FString Dir, Name;
	DialogDefaults(LastAuditFile, TEXT("HutongAudit.csv"), Dir, Name);
	IDesktopPlatform* Platform = FDesktopPlatformModule::Get();
	TArray<FString> Files;
	const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	if (Platform && Platform->SaveFileDialog(Parent, TEXT("Write the audit as CSV"), Dir, Name,
		TEXT("Comma-separated values (*.csv)|*.csv"), EFileDialogFlags::None, Files) && Files.Num() > 0)
	{
		if (HutongAudit::WriteCsv(Audited, Files[0]))
		{
			LastAuditFile = Files[0];
			SaveConfig();
			Summary += FString::Printf(TEXT("\nEvery check written to %s"), *Files[0]);
		}
		else Summary += FString::Printf(TEXT("\nCould not write %s"), *Files[0]);
	}
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Summary),
		NSLOCTEXT("HutongLayout", "AuditTitle", "Loaded buildings against the Qing figures"));
}

void UHutongLayoutModeSettings::ReportMeshSharing()
{
	const TArray<UHutongBuildingComponent*> Buildings = HutongDetailOps::CollectLoaded(EditorWorld());
	TArray<HutongSharing::FRow> Rows;
	if (!HutongSharing::MeasureAll(Buildings, TracedOutline, /*bShowDialog*/ true, Rows)) return;
	FString Summary = HutongSharing::SummaryTable(Rows) + HutongSharing::DescribeWallPieces(HutongSharing::CountWallPieces(Buildings, &Rows));
	UE_LOG(LogTemp, Display, TEXT("Hutong mesh sharing:\n%s"), *Summary);

	FString Dir, Name;
	DialogDefaults(LastSharingFile, TEXT("HutongSharing.csv"), Dir, Name);
	IDesktopPlatform* Platform = FDesktopPlatformModule::Get();
	TArray<FString> Files;
	const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	if (Platform && Platform->SaveFileDialog(Parent, TEXT("Write the mesh sharing report as CSV"), Dir, Name,
		TEXT("Comma-separated values (*.csv)|*.csv"), EFileDialogFlags::None, Files) && Files.Num() > 0)
	{
		if (HutongSharing::WriteCsv(Rows, Files[0]))
		{
			LastSharingFile = Files[0];
			SaveConfig();
			Summary += FString::Printf(TEXT("\nEvery building written to %s"), *Files[0]);
		}
		else Summary += FString::Printf(TEXT("\nCould not write %s"), *Files[0]);
	}
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Summary),
		NSLOCTEXT("HutongLayout", "SharingTitle", "Meshes the loaded buildings would need"));
}

void UHutongLayoutModeSettings::ApplyMeshSharing()
{
	const FText Question = NSLOCTEXT("HutongLayout", "ApplySharingAsk",
		"Resize and move every building in the level to its shared size? Locked buildings, city walls and Unknowns stay. Ctrl+Z undoes it.");
	if (FMessageDialog::Open(EAppMsgType::YesNo, Question) != EAppReturnType::Yes) return;

	const HutongSharing::FApplied Applied = HutongSharing::Apply(HutongDetailOps::CollectAll(EditorWorld()), TracedOutline, /*bShowDialog*/ true);
	RefreshCounts();
	FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("HutongLayout", "ApplySharingDone",
		"{0} buildings resized, {1} moved. {2} pushed less to stay clear of something; {3} still collide (see the Mesh Sharing Report's CSV)."),
		Applied.Resized, Applied.Moved, Applied.HeldBack, Applied.Collides));
}

void UHutongLayoutModeSettings::ParentLoadedToTiles()
{
	const HutongCourts::FParented R = HutongCourts::ParentToTiles(EditorWorld(), HutongDetailOps::CollectLoaded(EditorWorld()));
	FNotificationInfo Info(FText::Format(NSLOCTEXT("HutongLayout", "ParentedToTiles",
		"{0} buildings parented to their map tile, {1} already were. {2} on no tile; {3} refused (a Static building under a Movable tile)."),
		R.Parented, R.Already, R.OffMap, R.Refused));
	Info.ExpireDuration = 6.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void UHutongLayoutModeSettings::DetachLoadedFromTiles()
{
	const int32 Detached = HutongCourts::DetachFromTiles(EditorWorld(), HutongDetailOps::CollectLoaded(EditorWorld()));
	FNotificationInfo Info(FText::Format(NSLOCTEXT("HutongLayout", "DetachedFromTiles", "{0} buildings detached from their map tile."), Detached));
	Info.ExpireDuration = 4.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

void UHutongLayoutModeSettings::DeleteUnusedGeneratedMeshes()
{
	const TArray<FAssetData> Unused = HutongGen::FindUnusedLibraryMeshes();
	UE_LOG(LogTemp, Display, TEXT("Hutong: %d unused generated mesh(es)."), Unused.Num());
	if (Unused.Num() == 0)
	{
		FNotificationInfo Info(NSLOCTEXT("HutongLayout", "NoUnusedMeshes", "No unused generated meshes."));
		Info.ExpireDuration = 4.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}
	const FText Question = FText::Format(NSLOCTEXT("HutongLayout", "DeleteUnusedAsk",
		"Delete {0} generated meshes no building uses?"), Unused.Num());
	if (FMessageDialog::Open(EAppMsgType::OkCancel, Question) != EAppReturnType::Ok) return;

	// Loading each mesh queues builds that stall thousands, so unloaded ones go as files.
	TArray<UObject*> Loaded;
	TArray<FString> OnDisk;
	for (const FAssetData& Asset : Unused)
	{
		FString Filename;
		if (Asset.IsAssetLoaded() || FindPackage(nullptr, *Asset.PackageName.ToString()))
		{
			if (UObject* Mesh = Asset.GetAsset()) Loaded.Add(Mesh);
		}
		else if (FPackageName::DoesPackageExist(Asset.PackageName.ToString(), &Filename))
		{
			OnDisk.Add(FPaths::ConvertRelativePathToFull(Filename));
		}
	}

	constexpr int32 BatchSize = 500;
	int32 Deleted = 0;
	FScopedSlowTask Progress(OnDisk.Num() + 1, NSLOCTEXT("HutongLayout", "DeletingUnused", "Deleting unused generated meshes"));
	Progress.MakeDialog(/*bShowCancelButton*/ true);
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	for (int32 First = 0; First < OnDisk.Num() && !Progress.ShouldCancel(); First += BatchSize)
	{
		const int32 Count = FMath::Min(BatchSize, OnDisk.Num() - First);
		Progress.EnterProgressFrame(Count);
		TArray<FString> Gone;
		for (int32 i = First; i < First + Count; ++i)
		{
			if (IFileManager::Get().Delete(*OnDisk[i], /*RequireExists*/ false, /*EvenReadOnly*/ true)) Gone.Add(OnDisk[i]);
		}
		Registry.ScanModifiedAssetFiles(Gone);
		Deleted += Gone.Num();
	}
	Progress.EnterProgressFrame(1);
	if (Loaded.Num() > 0 && !Progress.ShouldCancel())
	{
		Deleted += ObjectTools::DeleteObjects(Loaded, /*bShowConfirmation*/ false);
	}
	const FText Summary = FText::Format(NSLOCTEXT("HutongLayout", "DeletedUnused", "Deleted {0} of {1} unused generated meshes."),
		Deleted, Unused.Num());
	FNotificationInfo Info(Summary);
	Info.ExpireDuration = 6.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
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
	// Lit while any selected building differs from the choice, so a newly selected one takes it without re-picking.
	const TArray<HutongDetailOps::FConvertTarget>& Targets = HutongDetailOps::ConvertTargets();
	const int32 Index = Targets.IndexOfByPredicate([this](const HutongDetailOps::FConvertTarget& T) { return T.Label == ConvertTo; });
	if (Index == INDEX_NONE) return false;
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
	Name = FString::Printf(TEXT("%s_%s.%s"), FallbackName, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), HutongExchange::FileExtension);

	FString Path;
	if (!PickSaveFile(Dir, Name, Path)) return;

	HutongExchange::FResult Result;
	if (bSelection) HutongExchange::ExportSelection(EditorWorld(), Path, Result);
	else            HutongExchange::ExportAll(EditorWorld(), Path, Result);
	HutongExchange::Report(Result, TEXT("export"));

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
		ImportFolder, Result, &HutongImportTypes::ResolveUnknownTypes, bBuildGeometryOnImport);
	HutongExchange::Report(Result, TEXT("import"));

	if (Result.bSucceeded) { LastSceneFile = Path; SaveConfig(); }
	HutongOverlaps::CheckAfterImport(Result.Placed);

	// Selected so the gizmos turn the set as one.
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

		LoadedTriangles += HutongGen::CountBuiltTriangles(B->GetOwner(), /*bWaitForBuild*/ true);
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
