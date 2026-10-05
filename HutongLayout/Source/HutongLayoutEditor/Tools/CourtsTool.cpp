#include "Tools/CourtsTool.h"
#include "Tools/CourtsWindow.h"
#include "Tools/HutongDetailOps.h"
#include "Generation/HutongBuildingComponent.h"
#include "HutongLayoutEdMode.h"
#include "Editor.h"
#include "InteractiveToolManager.h"
#include "ScopedTransaction.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/SWindow.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "HutongCourtsTool"

UInteractiveTool* UHutongCourtsToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongCourtsTool>(SceneState.ToolManager);
}

void UHutongCourtsToolProperties::OpenWindow() { if (Tool.IsValid()) Tool->OpenWindow(); }

namespace
{
	void NotifyCourts(const FText& Text)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = 4.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

void UHutongCourtsTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongCourtsToolProperties>(this);
	RegisterSettings(Settings, false);
	Settings->Tool = this;
}

void UHutongCourtsTool::Setup()
{
	Super::Setup();
	// Placement settings mean nothing to a tool that places nothing.
	for (UInteractiveToolPropertySet* Set : { (UInteractiveToolPropertySet*)MetadataSettings.Get(), (UInteractiveToolPropertySet*)Placement.Get(),
			(UInteractiveToolPropertySet*)DetailSettings.Get(), (UInteractiveToolPropertySet*)Snap.Get(), (UInteractiveToolPropertySet*)Appearance.Get() })
	{
		if (Set) SetToolPropertySourceEnabled(Set, false);
	}
	RefreshFromSelection(true);
	OpenWindow();

	// Courts change without the selection changing: an undo, a Details edit, the other tool.
	UndoHandle = FEditorDelegates::PostUndoRedo.AddWeakLambda(this, [this]() { bCourtsDirty = true; });
	PropertyHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddWeakLambda(this,
		[this](UObject* Object, FPropertyChangedEvent& Event)
		{
			if (Object && Object->IsA<UHutongBuildingComponent>()) bCourtsDirty = true;
		});
}

void UHutongCourtsTool::Shutdown(EToolShutdownType ShutdownType)
{
	FEditorDelegates::PostUndoRedo.Remove(UndoHandle);
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyHandle);
	if (Window.IsValid())
	{
		const TSharedPtr<SWindow> Closing = Window;
		Window.Reset();
		Closing->RequestDestroyWindow();
	}
	Super::Shutdown(ShutdownType);
}

void UHutongCourtsTool::OpenWindow()
{
	if (!FSlateApplication::IsInitialized() || IsRunningCommandlet()) return;
	if (Window.IsValid())
	{
		Window->BringToFront();
		return;
	}
	Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Courtyard Units (院落)"))
		.ClientSize(FVector2D(760.0, 480.0))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		// The viewport keeps focus for clicking.
		.FocusWhenFirstShown(false)
		[
			HutongCourtsWindow::MakeContent(this)
		];
	Window->SetOnWindowClosed(FOnWindowClosed::CreateWeakLambda(this, [this](const TSharedRef<SWindow>&) { Window.Reset(); }));
	const TSharedPtr<SWindow> Root = FGlobalTabmanager::Get()->GetRootWindow();
	if (Root.IsValid()) FSlateApplication::Get().AddWindowAsNativeChild(Window.ToSharedRef(), Root.ToSharedRef());
	else FSlateApplication::Get().AddWindow(Window.ToSharedRef());
}

UWorld* UHutongCourtsTool::EditingWorld() const
{
	return GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();
}

void UHutongCourtsTool::OnTick(float DeltaTime)
{
	Super::OnTick(DeltaTime);
	RefreshFromSelection(bCourtsDirty);
	bCourtsDirty = false;
}

void UHutongCourtsTool::RefreshCourts()
{
	Courts = HutongCourts::Summaries(EditingWorld());
	++CourtsRevision;
}

void UHutongCourtsTool::RefreshFromSelection(bool bForce)
{
	const TArray<UHutongBuildingComponent*> Selected = HutongDetailOps::CollectSelected();
	if (!bForce && Selected.Num() == SeenSelection.Num()
		&& !Selected.ContainsByPredicate([&](const UHutongBuildingComponent* B) { return !SeenSelection.Contains(B); }))
	{
		return;
	}
	SeenSelection.Reset();
	SelectedCourts.Reset();
	for (UHutongBuildingComponent* B : Selected)
	{
		SeenSelection.Add(B);
		SelectedCourts.Append(B->GetCourts());
	}
	TArray<FString> Names = SelectedCourts.Array();
	Names.Sort();
	SelectionText = Selected.Num() == 0 ? FString(TEXT("Nothing selected: click, Shift+click or drag a box in the viewport."))
		: FString::Printf(TEXT("%d building%s selected, %s"), Selected.Num(), Selected.Num() == 1 ? TEXT("") : TEXT("s"),
			Names.Num() == 0 ? TEXT("in no court") : *FString::Printf(TEXT("in %s"), *FString::Join(Names, TEXT(", "))));

	// A fresh suggestion for a new court, unless typed.
	if (Selected.Num() > 0 && (CourtName.IsEmpty() || bNameSuggested))
	{
		CourtName = HutongCourts::SuggestName(EditingWorld(), Selected);
		bNameSuggested = true;
	}
	RefreshCourts();
}

void UHutongCourtsTool::SuggestName()
{
	CourtName = HutongCourts::SuggestName(EditingWorld(), HutongDetailOps::CollectSelected());
	bNameSuggested = true;
}

void UHutongCourtsTool::AssignNew()
{
	const FString Name = CourtName.TrimStartAndEnd();
	if (Name.IsEmpty())
	{
		NotifyCourts(LOCTEXT("NoName", "Give the court a name first (Suggest Name fills one in)."));
		return;
	}
	AddSelectedTo(Name);
}

void UHutongCourtsTool::AddSelectedTo(const FString& Court)
{
	const int32 Count = HutongCourts::Assign(HutongDetailOps::CollectSelected(), Court);
	if (Count == 0)
	{
		NotifyCourts(LOCTEXT("NothingSelected", "Select the buildings to add in the viewport first."));
		return;
	}
	NotifyCourts(FText::Format(LOCTEXT("Added", "{0} building(s) now in {1}."), Count, FText::FromString(Court)));
	RefreshFromSelection(true);
}

void UHutongCourtsTool::RemoveSelectedFrom(const FString& Court)
{
	const int32 Count = HutongCourts::Remove(HutongDetailOps::CollectSelected(), Court);
	NotifyCourts(Count == 0
		? FText::Format(LOCTEXT("NoneIn", "No selected building is in {0}."), FText::FromString(Court))
		: FText::Format(LOCTEXT("Removed", "Took {0} building(s) out of {1}."), Count, FText::FromString(Court)));
	RefreshFromSelection(true);
}

void UHutongCourtsTool::SelectCourt(const FString& Court)
{
	HutongCourts::SelectCourts(EditingWorld(), { Court }, /*bAdd*/ false);
}

void UHutongCourtsTool::EditHeights(const FString& Court)
{
	SelectCourt(Court);
	// Next tick: this tool is the one being shut down, from inside its own window.
	if (!GEditor) return;
	GEditor->GetTimerManager()->SetTimerForNextTick([]() { UHutongLayoutEdMode::StartTool(TEXT("HutongHeightsTool")); });
}

void UHutongCourtsTool::RebuildFolders()
{
	UWorld* World = EditingWorld();
	int32 Moved = 0;
	{
		const FScopedTransaction Transaction(LOCTEXT("RebuildFolders", "File Buildings By Court"));
		Moved = HutongCourts::FileInFolders(World, HutongDetailOps::CollectLoaded(World));
	}
	NotifyCourts(FText::Format(LOCTEXT("Filed", "Filed {0} building(s) under Courts."), Moved));
	RefreshCourts();
}

TArray<FText> UHutongCourtsTool::GetToolHelpLines() const
{
	TArray<FText> Lines;
	Lines.Add(LOCTEXT("HelpSelect", "Select buildings in the viewport: click, Shift+click to add, or drag a box."));
	Lines.Add(LOCTEXT("HelpNew", "New court: keep the suggested name or type one, then Assign To Selection."));
	Lines.Add(LOCTEXT("HelpExisting", "Existing court: select buildings, then Add Selected or Remove Selected on its row."));
	Lines.Add(LOCTEXT("HelpFolders", "Courts are filed in the Outliner under Courts / tile / court; shared walls under the tile's Shared walls."));
	Lines.Add(LOCTEXT("HelpEsc", "Esc puts the tool down and closes the window."));
	return Lines;
}

FText UHutongCourtsTool::GetStagePromptText() const
{
	return SeenSelection.Num() == 0
		? LOCTEXT("PromptNone", "Select buildings in the viewport; name a new court or add them to one in the Courtyard Units window.")
		: LOCTEXT("PromptSome", "In the Courtyard Units window: Assign To Selection for a new court, or Add Selected on an existing court's row.");
}

FText UHutongCourtsTool::GetKeyHintText() const
{
	return LOCTEXT("KeyHint", "Click select · Shift+click add · drag box · Esc put the tool down");
}

TArray<FText> UHutongCourtsTool::GetStageNames() const
{
	return { LOCTEXT("StageSelect", "Select"), LOCTEXT("StageAssign", "Assign or Add") };
}

int32 UHutongCourtsTool::GetStageIndex() const
{
	return SeenSelection.Num() > 0 ? 1 : 0;
}

#undef LOCTEXT_NAMESPACE
