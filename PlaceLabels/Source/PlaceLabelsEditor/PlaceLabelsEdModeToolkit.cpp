#include "PlaceLabelsEdModeToolkit.h"

#include "PlaceLabelsCommands.h"
#include "Tools/PlaceRegionEditTool.h"
#include "Tools/PlaceRegionPenTool.h"
#include "Widgets/SPlaceLabelsPanel.h"
#include "InteractiveToolManager.h"
#include "Tools/UEdMode.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "PlaceLabelsPluginStamp.h"

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
	// Select first: the mode opens in it.
	ToolbarBuilder.AddToolBarButton(FPlaceLabelsCommands::Get().BeginSelectTool);
	ToolbarBuilder.AddToolBarButton(FPlaceLabelsCommands::Get().BeginPenTool);
	ToolbarBuilder.AddToolBarButton(FPlaceLabelsCommands::Get().BeginEditTool);
}

TSharedPtr<SWidget> FPlaceLabelsEdModeToolkit::GetInlineContent() const
{
	// The panel reads the active tool's prompt from the mode manager itself.
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
		]

		// The last commit to change this plugin, written into the source by .githooks/pre-commit.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 6.0f, 4.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(PluginUpdatedText())
			.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		];
}

FText FPlaceLabelsEdModeToolkit::PluginUpdatedText()
{
	const FDateTime Utc = FDateTime::FromUnixTimestamp(PLACELABELS_PLUGIN_UPDATED_UTC);
	// Shown in the machine's own time.
	const FText When = FText::FromString((Utc + (FDateTime::Now() - FDateTime::UtcNow())).ToString(TEXT("%Y-%m-%d %H:%M")));
	return FText::Format(LOCTEXT("Version", "Plugin last updated {0}"), When);
}

#undef LOCTEXT_NAMESPACE
