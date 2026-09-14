#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class SDockTab;
class FSpawnTabArgs;

namespace PlaceLabelsEditor
{
	// The dockable Place Labels tab.
	extern const FName PanelTabId;

	// Opens the tab, or brings it forward if it is already open.
	void OpenPanelTab();
}

class FPlaceLabelsEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	static TSharedRef<SDockTab> SpawnPanelTab(const FSpawnTabArgs& Args);

	// Cached at startup so shutdown does not have to touch StaticClass() while the UObject system may already be unwinding.
	FName RegionComponentClassName;
};
