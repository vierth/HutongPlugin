#include "PlaceLabelsEdModeToolkit.h"

#include "PlaceLabelsCommands.h"
#include "Tools/PlaceRegionEditTool.h"
#include "Tools/PlaceRegionPenTool.h"
#include "Widgets/SPlaceLabelsPanel.h"
#include "InteractiveToolManager.h"
#include "Tools/UEdMode.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "PlaceLabelsEdModeToolkit"

FName FPlaceLabelsEdModeToolkit::GetToolkitFName() const
{
	return FName("PlaceLabelsEdMode");
}

FText FPlaceLabelsEdModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Place Labels");
}

void FPlaceLabelsEdModeToolkit::GetToolPaletteNames(TArray<FName>& OutPaletteNames) const
{
	OutPaletteNames.Add(FName("Tools"));
}

FText FPlaceLabelsEdModeToolkit::GetToolPaletteDisplayName(FName PaletteName) const
{
	return LOCTEXT("ToolsPalette", "Tools");
}

void FPlaceLabelsEdModeToolkit::BuildToolPalette(FName PaletteName, FToolBarBuilder& ToolbarBuilder)
{
	// Select first: it is the state the mode opens in and the one to come back to.
	ToolbarBuilder.AddToolBarButton(FPlaceLabelsCommands::Get().BeginSelectTool);
	ToolbarBuilder.AddToolBarButton(FPlaceLabelsCommands::Get().BeginPenTool);
	ToolbarBuilder.AddToolBarButton(FPlaceLabelsCommands::Get().BeginEditTool);
}

TSharedPtr<SWidget> FPlaceLabelsEdModeToolkit::GetInlineContent() const
{
	// The panel resolves the active tool's prompt itself, from the mode manager, rather than being handed it here.
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SPlaceLabelsPanel)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			ModeDetailsView.ToSharedRef()
		]

		+ SVerticalBox::Slot()
		[
			DetailsView.ToSharedRef()
		];
}

#undef LOCTEXT_NAMESPACE
