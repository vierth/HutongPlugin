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
	// Style set first: FHutongLayoutCommands resolves its icons out of it at registration time.
	FHutongLayoutStyle::Register();
	FHutongLayoutCommands::Register();

	// After the commands, though nothing depends on the order.
	HutongPresets::RegisterBuiltInPresets();

	HutongPanelCustomizations::Register();

	// Plans draw whether or not the mode is up, so whether they are wanted has to be settled
	// before anybody enters it.
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
