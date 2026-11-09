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

	virtual TSharedPtr<SWidget> GetInlineContent() const override;

	static FText PluginUpdatedText();
};
