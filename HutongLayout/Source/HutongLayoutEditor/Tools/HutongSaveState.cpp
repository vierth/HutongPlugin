#include "Tools/HutongSaveState.h"
#include "Tools/HutongDetailOps.h"
#include "Generation/HutongBuildingComponent.h"

#include "Containers/Ticker.h"
#include "Editor.h"
#include "Misc/CoreDelegates.h"
#include "Engine/World.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "HutongSaveState"

namespace HutongSaveState
{
	namespace
	{
		struct FTrack
		{
			TOptional<FDateTime> Edit;
			TOptional<FDateTime> Autosave;
			uint64 AutosaveFrame = 0;
			// The file's time, looked up once; unset inside a set Saved means no file.
			TOptional<TOptional<FDateTime>> Saved;
		};
		TMap<FName, FTrack> Tracks;
		int32 PendingAutosaved = 0;

		FDelegateHandle SavedHandle;
		FDelegateHandle DirtyHandle;
		FDelegateHandle ActorAddedHandle;
		FDelegateHandle ActorDeletedHandle;
		FDelegateHandle MapChangeHandle;
		FDelegateHandle EngineInitHandle;
		FDelegateHandle UndoHandle;
		FTSTicker::FDelegateHandle NoticeTicker;

		// Also recounted every ten seconds: streamed-in regions and settings changes send no event.
		FStatus CachedScene;
		double SceneTime = -1.0;
		bool bSceneStale = true;
		void MarkSceneStale() { bSceneStale = true; }

		FText SaveKey()
		{
			return PLATFORM_MAC ? LOCTEXT("SaveKeyMac", "Cmd+S") : LOCTEXT("SaveKey", "Ctrl+S");
		}

		// The engine's own autosave overwrites in place under this method: an ordinary save.
		bool AutosaveOverwrites()
		{
			return GetDefault<UEditorLoadingSavingSettings>()->AutoSaveMethod == EAutoSaveMethod::BackupAndOverwrite;
		}

		UPackage* PackageOf(const UHutongBuildingComponent* Building)
		{
			const AActor* Owner = Building ? Building->GetOwner() : nullptr;
			if (!Owner) return nullptr;
			UPackage* External = Owner->GetExternalPackage();
			return External ? External : Owner->GetPackage();
		}

		bool IsTracked(UPackage* Package)
		{
			if (UWorld::FindWorldInPackage(Package)) return true;
			const AActor* Actor = Cast<AActor>(Package->FindAssetInPackage());
			return Actor && Actor->FindComponentByClass<UHutongBuildingComponent>();
		}

		FText When(const FDateTime& Local)
		{
			const bool bToday = Local.GetDate() == FDateTime::Now().GetDate();
			return FText::FromString(Local.ToString(bToday ? TEXT("%H:%M") : TEXT("%Y-%m-%d %H:%M")));
		}

		TOptional<FDateTime> SavedTime(const UPackage* Package)
		{
			FTrack& T = Tracks.FindOrAdd(Package->GetFName());
			if (!T.Saved)
			{
				FString Filename;
				const FDateTime Utc = FPackageName::DoesPackageExist(Package->GetName(), &Filename)
					? IFileManager::Get().GetTimeStamp(*Filename) : FDateTime::MinValue();
				T.Saved.Emplace(Utc == FDateTime::MinValue() ? TOptional<FDateTime>()
					: TOptional<FDateTime>(Utc + (FDateTime::Now() - FDateTime::UtcNow())));
			}
			return *T.Saved;
		}

		void OnPackageSaved(const FString&, UPackage* Package, FObjectPostSaveContext Context)
		{
			if (!Package || Context.IsProceduralSave() || !IsTracked(Package)) return;
			const bool bBackup = Context.IsFromAutoSave() && !AutosaveOverwrites();
			if (Context.IsFromAutoSave() && !UWorld::FindWorldInPackage(Package)) ++PendingAutosaved;
			FTrack& T = Tracks.FindOrAdd(Package->GetFName());
			if (bBackup)
			{
				T.Autosave = FDateTime::Now();
				T.AutosaveFrame = GFrameCounter;
			}
			else
			{
				T = FTrack();
				T.Saved.Emplace(FDateTime::Now());
			}
			SceneTime = -1.0;
			bSceneStale = true;
		}

		void OnPackageMarkedDirty(UPackage* Package, bool)
		{
			if (!Package || !IsTracked(Package)) return;
			FTrack& T = Tracks.FindOrAdd(Package->GetFName());
			// A backed-up map package re-marked dirty in the autosave's own frame is skipped as an edit.
			if (T.Autosave && T.AutosaveFrame == GFrameCounter) return;
			T.Edit = FDateTime::Now();
			bSceneStale = true;
		}

		bool NotifyAutosaved(float)
		{
			if (PendingAutosaved > 0)
			{
				FNotificationInfo Info(AutosaveOverwrites()
					? FText::Format(LOCTEXT("AutosavedInPlace", "Autosave saved {0} changed {0}|plural(one=building,other=buildings)."), PendingAutosaved)
					: FText::Format(LOCTEXT("Autosaved",
						"Autosave backed up {0} changed {0}|plural(one=building,other=buildings). "
						"A backup copy only: the level is unsaved until you save ({1})."), PendingAutosaved, SaveKey()));
				Info.ExpireDuration = 6.0f;
				FSlateNotificationManager::Get().AddNotification(Info);
				PendingAutosaved = 0;
			}
			return true;
		}

		struct FCounts
		{
			int32 Unsaved = 0;
			int32 BackedUp = 0;
			int32 Changed() const { return Unsaved + BackedUp; }
			EState Worst() const { return Unsaved > 0 ? EState::Unsaved : BackedUp > 0 ? EState::BackedUp : EState::Saved; }
		};

		FCounts Count(const TArray<UHutongBuildingComponent*>& Buildings)
		{
			FCounts C;
			for (const UHutongBuildingComponent* B : Buildings)
			{
				const EState S = StateOf(B);
				C.Unsaved += S == EState::Unsaved;
				C.BackedUp += S == EState::BackedUp;
			}
			return C;
		}

		FStatus DescribeOne(const UHutongBuildingComponent* Building)
		{
			const EState State = StateOf(Building);
			const UPackage* Package = PackageOf(Building);
			const FTrack* T = Package ? Tracks.Find(Package->GetFName()) : nullptr;
			FStatus Out{ FText::GetEmpty(), ColourOf(State) };
			switch (State)
			{
			case EState::Saved:
			{
				const TOptional<FDateTime> At = Package ? SavedTime(Package) : TOptional<FDateTime>();
				Out.Text = At ? FText::Format(LOCTEXT("SavedAt", "Saved {0}"), When(*At)) : LOCTEXT("Saved", "Saved");
				break;
			}
			case EState::BackedUp:
				Out.Text = FText::Format(LOCTEXT("BackedUp", "Unsaved changes, backed up by autosave at {0}. {1} saves them to the level."),
					When(*T->Autosave), SaveKey());
				break;
			case EState::Unsaved:
				Out.Text = T && T->Edit
					? FText::Format(LOCTEXT("UnsavedSince", "Unsaved changes (last at {0}). {1} saves them."), When(*T->Edit), SaveKey())
					: FText::Format(LOCTEXT("Unsaved", "Unsaved changes. {0} saves them."), SaveKey());
				break;
			}
			return Out;
		}
	}

	EState StateOf(const UHutongBuildingComponent* Building)
	{
		const UPackage* Package = PackageOf(Building);
		if (!Package || !Package->IsDirty()) return EState::Saved;
		const FTrack* T = Tracks.Find(Package->GetFName());
		return T && T->Autosave && (!T->Edit || *T->Edit <= *T->Autosave) ? EState::BackedUp : EState::Unsaved;
	}

	FLinearColor ColourOf(EState State)
	{
		switch (State)
		{
		case EState::Unsaved: return FLinearColor(1.0f, 0.55f, 0.15f);
		case EState::BackedUp: return FLinearColor(1.0f, 0.85f, 0.3f);
		default: return FLinearColor(0.45f, 0.85f, 0.45f);
		}
	}

	FString LabelOf(EState State)
	{
		switch (State)
		{
		case EState::Unsaved: return TEXT("* unsaved");
		case EState::BackedUp: return TEXT("* unsaved (autosave backup)");
		default: return FString();
		}
	}

	FStatus Describe(const TArray<UHutongBuildingComponent*>& Buildings)
	{
		if (Buildings.Num() == 0) return {};
		if (Buildings.Num() == 1) return DescribeOne(Buildings[0]);

		const FCounts C = Count(Buildings);
		FStatus Out{ FText::GetEmpty(), ColourOf(C.Worst()) };
		if (C.Changed() == 0) Out.Text = FText::Format(LOCTEXT("AllSaved", "All {0} saved"), Buildings.Num());
		else if (C.BackedUp > 0)
		{
			Out.Text = FText::Format(LOCTEXT("SomeUnsavedBacked", "{0} of {1} unsaved ({2} backed up by autosave). {3} saves them."),
				C.Changed(), Buildings.Num(), C.BackedUp, SaveKey());
		}
		else Out.Text = FText::Format(LOCTEXT("SomeUnsaved", "{0} of {1} unsaved. {2} saves them."), C.Unsaved, Buildings.Num(), SaveKey());
		return Out;
	}

	const FStatus& SceneStatus()
	{
		const double Now = FPlatformTime::Seconds();
		if (SceneTime >= 0.0 && Now - SceneTime < (bSceneStale ? 1.0 : 10.0)) return CachedScene;
		SceneTime = Now;
		bSceneStale = false;

		const FCounts C = Count(HutongDetailOps::CollectLoaded(HutongDetailOps::EditorWorld()));
		const UEditorLoadingSavingSettings* Saving = GetDefault<UEditorLoadingSavingSettings>();
		const FText Autosave = !Saving->bAutoSaveEnable ? LOCTEXT("AutosaveOff", "Autosave is off.")
			: AutosaveOverwrites()
			? FText::Format(LOCTEXT("AutosaveInPlace", "Autosave saves the level every {0} min."), Saving->AutoSaveTimeMinutes)
			: FText::Format(LOCTEXT("AutosaveOn", "Autosave every {0} min keeps a backup copy; it does not save the level."), Saving->AutoSaveTimeMinutes);
		CachedScene.Colour = ColourOf(C.Worst());
		if (C.Changed() == 0)
		{
			CachedScene.Text = FText::Format(LOCTEXT("SceneSaved", "All loaded buildings saved. {0}"), Autosave);
			return CachedScene;
		}
		const FText Changed = C.BackedUp > 0
			? FText::Format(LOCTEXT("SceneUnsavedBacked", "{0} {0}|plural(one=building,other=buildings) unsaved ({1} backed up by autosave)."), C.Changed(), C.BackedUp)
			: FText::Format(LOCTEXT("SceneUnsaved", "{0} {0}|plural(one=building,other=buildings) unsaved."), C.Unsaved);
		CachedScene.Text = FText::Format(LOCTEXT("SceneSummary", "{0} {1} saves them. {2}"), Changed, SaveKey(), Autosave);
		return CachedScene;
	}

	const FStatus& FPolled::Get() const
	{
		if (Frame != GFrameCounter)
		{
			Frame = GFrameCounter;
			Status = Describe(Source());
		}
		return Status;
	}

	void Start()
	{
		SavedHandle = UPackage::PackageSavedWithContextEvent.AddStatic(&OnPackageSaved);
		DirtyHandle = UPackage::PackageMarkedDirtyEvent.AddStatic(&OnPackageMarkedDirty);
		NoticeTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&NotifyAutosaved), 0.5f);
		// This module loads before the engine exists.
		auto BindEngine = []
		{
			ActorAddedHandle = GEngine->OnLevelActorAdded().AddLambda([](AActor*) { MarkSceneStale(); });
			ActorDeletedHandle = GEngine->OnLevelActorDeleted().AddLambda([](AActor*) { MarkSceneStale(); });
		};
		if (GEngine) BindEngine();
		else EngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddLambda(BindEngine);
		MapChangeHandle = FEditorDelegates::MapChange.AddLambda([](uint32) { MarkSceneStale(); });
		UndoHandle = FEditorDelegates::PostUndoRedo.AddStatic(&MarkSceneStale);
	}

	void Stop()
	{
		UPackage::PackageSavedWithContextEvent.Remove(SavedHandle);
		UPackage::PackageMarkedDirtyEvent.Remove(DirtyHandle);
		FTSTicker::GetCoreTicker().RemoveTicker(NoticeTicker);
		FCoreDelegates::GetOnPostEngineInit().Remove(EngineInitHandle);
		if (GEngine)
		{
			GEngine->OnLevelActorAdded().Remove(ActorAddedHandle);
			GEngine->OnLevelActorDeleted().Remove(ActorDeletedHandle);
		}
		FEditorDelegates::MapChange.Remove(MapChangeHandle);
		FEditorDelegates::PostUndoRedo.Remove(UndoHandle);
	}
}

#undef LOCTEXT_NAMESPACE
