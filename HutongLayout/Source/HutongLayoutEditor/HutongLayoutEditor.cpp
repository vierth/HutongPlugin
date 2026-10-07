#include "HutongLayoutEditor.h"
#include "HutongLayoutCommands.h"
#include "HutongLayoutStyle.h"
#include "Tools/HutongPresetDefaults.h"
#include "Tools/HutongPanelCustomizations.h"
#include "Tools/HutongContextMenu.h"
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
	// A Hutong section in the level editor's right-click menu on selected buildings.
	HutongContextMenu::Register();

	// Plans draw whether or not the mode is active, so load their visibility before anyone enters it.
	HutongPlanOutline::LoadVisibilityFromConfig();
	HutongPlanOutline::StartLayers();

	// New library meshes are saved once their background build is done (and all before a level save).
	HutongGen::StartLibrarySaver();
}

void FHutongLayoutEditorModule::ShutdownModule()
{
	HutongGen::StopLibrarySaver();
	HutongPlanOutline::StopLayers();
	HutongContextMenu::Unregister();
	HutongPanelCustomizations::Unregister();
	FHutongLayoutCommands::Unregister();
	FHutongLayoutStyle::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHutongLayoutEditorModule, HutongLayoutEditor)
