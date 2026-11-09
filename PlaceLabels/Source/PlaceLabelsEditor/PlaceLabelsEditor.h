#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class SDockTab;
class FSpawnTabArgs;

namespace PlaceLabelsEditor
{
	extern const FName PanelTabId;

	void OpenPanelTab();
}

class FPlaceLabelsEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	static TSharedRef<SDockTab> SpawnPanelTab(const FSpawnTabArgs& Args);

	// Cached: StaticClass() at shutdown may hit an unwinding UObject system.
	FName RegionComponentClassName;
};
