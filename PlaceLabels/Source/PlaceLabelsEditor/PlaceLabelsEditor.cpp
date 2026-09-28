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
			// Scroll box: a tab docked shorter than the panel would hide the problems list.
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SPlaceLabelsPanel)
			]
		];
}

void FPlaceLabelsEditorModule::StartupModule()
{
	// Style first: FPlaceLabelsCommands resolves icons from it at registration.
	FPlaceLabelsStyle::Register();
	FPlaceLabelsCommands::Register();
	FPlaceLabelsVisualizerCommands::Register();

	// GUnrealEd exists by PostEngineInit, this module's loading phase.
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
