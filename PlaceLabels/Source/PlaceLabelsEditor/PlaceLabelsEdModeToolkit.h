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

	// Replaces the default (mode details + tool details) with the Place Labels panel on top.
	virtual TSharedPtr<SWidget> GetInlineContent() const override;
};
