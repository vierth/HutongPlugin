#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "HutongLayoutStyle.h"

class FHutongLayoutCommands : public TCommands<FHutongLayoutCommands>
{
public:
	FHutongLayoutCommands()
		: TCommands<FHutongLayoutCommands>(
			TEXT("HutongLayout"),
			NSLOCTEXT("Contexts", "HutongLayout", "Hutong Layout"),
			NAME_None,
			FHutongLayoutStyle::GetStyleSetName())
	{}

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> BeginWallTool;
	TSharedPtr<FUICommandInfo> BeginCourtWallTool;
	TSharedPtr<FUICommandInfo> BeginSiheyuanTool;
	TSharedPtr<FUICommandInfo> BeginEarPassageTool;
	TSharedPtr<FUICommandInfo> BeginGateHouseTool;
	TSharedPtr<FUICommandInfo> BeginPaifangTool;
	TSharedPtr<FUICommandInfo> BeginInnerGateTool;
	TSharedPtr<FUICommandInfo> BeginCorridorTool;
	TSharedPtr<FUICommandInfo> BeginScreenWallTool;
	TSharedPtr<FUICommandInfo> BeginShopfrontTool;
	TSharedPtr<FUICommandInfo> BeginStoreyTool;
	TSharedPtr<FUICommandInfo> BeginPavilionTool;
	TSharedPtr<FUICommandInfo> BeginHallTool;
	TSharedPtr<FUICommandInfo> BeginPathTool;
	TSharedPtr<FUICommandInfo> BeginCompoundTool;
	TSharedPtr<FUICommandInfo> BeginStreetRowTool;
	TSharedPtr<FUICommandInfo> BeginGalleryTool;
	TSharedPtr<FUICommandInfo> BeginMeasureTool;
	TSharedPtr<FUICommandInfo> BeginImportTool;
	TSharedPtr<FUICommandInfo> BeginFlowerBedTool;
	TSharedPtr<FUICommandInfo> BeginWaterJarTool;
};
