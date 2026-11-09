#include "HutongLayoutEditor.h"
#include "HutongLayoutCommands.h"
#include "HutongLayoutStyle.h"
#include "Tools/HutongPresetDefaults.h"
#include "Tools/HutongPanelCustomizations.h"
#include "Tools/HutongContextMenu.h"
#include "Tools/HutongSaveState.h"
#include "Generation/HutongPlanOutlineComponent.h"
#include "Generation/HutongActorSpawn.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FHutongLayoutEditorModule"

void FHutongLayoutEditorModule::StartupModule()
{
	// Style first: commands resolve their icons from it at registration.
	FHutongLayoutStyle::Register();
	FHutongLayoutCommands::Register();

	HutongPresets::RegisterBuiltInPresets();

	HutongPanelCustomizations::Register();
	HutongContextMenu::Register();

	// Plans draw in every mode, so load their visibility at startup.
	HutongPlanOutline::LoadVisibilityFromConfig();
	HutongPlanOutline::StartLayers();

	HutongGen::StartLibrarySaver();
	HutongSaveState::Start();
}

void FHutongLayoutEditorModule::ShutdownModule()
{
	HutongSaveState::Stop();
	HutongGen::StopLibrarySaver();
	HutongPlanOutline::StopLayers();
	HutongContextMenu::Unregister();
	HutongPanelCustomizations::Unregister();
	FHutongLayoutCommands::Unregister();
	FHutongLayoutStyle::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHutongLayoutEditorModule, HutongLayoutEditor)
