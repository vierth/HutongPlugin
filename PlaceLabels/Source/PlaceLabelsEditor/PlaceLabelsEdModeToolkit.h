#pragma once

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"

class UInteractiveTool;

class FPlaceLabelsEdModeToolkit : public FModeToolkit
{
public:
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual void GetToolPaletteNames(TArray<FName>& OutPaletteNames) const override;
	virtual FText GetToolPaletteDisplayName(FName PaletteName) const override;
	virtual void BuildToolPalette(FName PaletteName, FToolBarBuilder& ToolbarBuilder) override;

	// Place Labels panel on top of the default mode and tool details.
	virtual TSharedPtr<SWidget> GetInlineContent() const override;

	// "Plugin last updated <time>": the last commit to change this plugin, in local time.
	static FText PluginUpdatedText();
};
