#include "PlaceLabelsEditor.h"

#include "PlaceLabelsCommands.h"
#include "PlaceLabelsStyle.h"
#include "PlaceLabelsVisualizerCommands.h"
#include "PlaceRegionComponent.h"
#include "Visualizers/PlaceRegionComponentVisualizer.h"
#include "Widgets/SPlaceLabelsPanel.h"
#include "Editor/UnrealEdEngine.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "UnrealEdGlobals.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SScrollBox.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FPlaceLabelsEditorModule"

namespace PlaceLabelsEditor
{
	const FName PanelTabId("PlaceLabelsPanel");

	void OpenPanelTab()
	{
		FGlobalTabmanager::Get()->TryInvokeTab(PanelTabId);
	}
}

TSharedRef<SDockTab> FPlaceLabelsEditorModule::SpawnPanelTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			// The panel sizes itself to its content and the tab can be docked anywhere, including somewhere shorter than the content; without a scroll box the problems list would simply be unreachable.
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SPlaceLabelsPanel)
			]
		];
}

void FPlaceLabelsEditorModule::StartupModule()
{
	// Style first: FPlaceLabelsCommands resolves its icons out of the style set at registration time.
	FPlaceLabelsStyle::Register();
	FPlaceLabelsCommands::Register();
	FPlaceLabelsVisualizerCommands::Register();

	// GUnrealEd exists by PostEngineInit, which is the loading phase this module declares.
	if (GUnrealEd)
	{
		RegionComponentClassName = UPlaceRegionComponent::StaticClass()->GetFName();

		TSharedPtr<FPlaceRegionComponentVisualizer> Visualizer =
			MakeShared<FPlaceRegionComponentVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(RegionComponentClassName, Visualizer);

		// After registration, not in the constructor: OnRegister maps the command list.
		Visualizer->OnRegister();
	}

	// A nomad tab rather than one owned by the level editor.
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()
			->RegisterNomadTabSpawner(PlaceLabelsEditor::PanelTabId,
				FOnSpawnTab::CreateStatic(&FPlaceLabelsEditorModule::SpawnPanelTab))
			.SetDisplayName(LOCTEXT("PanelTabTitle", "Place Labels"))
			.SetTooltipText(LOCTEXT("PanelTabTooltip",
				"Browse, name and repair every place region in the level. The same panel as the "
				"Place Labels mode sidebar, with room to work."))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
			.SetIcon(FSlateIcon(FPlaceLabelsStyle::GetStyleSetName(), "PlaceLabels.BeginPenTool"));
	}
}

void FPlaceLabelsEditorModule::ShutdownModule()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PlaceLabelsEditor::PanelTabId);
	}

	if (GUnrealEd && RegionComponentClassName != NAME_None)
	{
		GUnrealEd->UnregisterComponentVisualizer(RegionComponentClassName);
	}

	FPlaceLabelsVisualizerCommands::Unregister();
	FPlaceLabelsCommands::Unregister();
	FPlaceLabelsStyle::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPlaceLabelsEditorModule, PlaceLabelsEditor)
