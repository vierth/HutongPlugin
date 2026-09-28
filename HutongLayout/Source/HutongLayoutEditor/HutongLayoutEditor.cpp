#include "HutongLayoutEditor.h"
#include "HutongLayoutCommands.h"
#include "HutongLayoutStyle.h"
#include "Tools/HutongPresetDefaults.h"
#include "Tools/HutongPanelCustomizations.h"
#include "Generation/HutongPlanOutlineComponent.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FHutongLayoutEditorModule"

void FHutongLayoutEditorModule::StartupModule()
{
	// Style first: commands resolve their icons from it at registration.
	FHutongLayoutStyle::Register();
	FHutongLayoutCommands::Register();

	HutongPresets::RegisterBuiltInPresets();

	HutongPanelCustomizations::Register();

	// Plans draw whether or not the mode is active, so load their visibility before anyone enters it.
	HutongPlanOutline::LoadVisibilityFromConfig();
}

void FHutongLayoutEditorModule::ShutdownModule()
{
	HutongPanelCustomizations::Unregister();
	FHutongLayoutCommands::Unregister();
	FHutongLayoutStyle::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FHutongLayoutEditorModule, HutongLayoutEditor)
