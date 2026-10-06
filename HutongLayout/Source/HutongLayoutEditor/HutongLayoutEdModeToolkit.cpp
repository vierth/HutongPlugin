#include "HutongLayoutEdModeToolkit.h"
#include "Tools/HutongPanelCustomizations.h"
#include "HutongLayoutCommands.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongPanelFilter.h"
#include "HutongLayoutEdMode.h"
#include "HutongLayoutModeSettings.h"
#include "Generation/HutongPlanOutlineComponent.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "InteractiveToolManager.h"
#include "Tools/UEdMode.h"
#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Generation/HutongBuildingComponent.h"
#include "Tools/HutongDetailOps.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Selection.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "HutongLayoutEdModeToolkit"

FHutongLayoutEdModeToolkit::~FHutongLayoutEdModeToolkit()
{
	if (SelectionChangedHandle.IsValid())
	{
		USelection::SelectionChangedEvent.Remove(SelectionChangedHandle);
	}
}

void FHutongLayoutEdModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost,
	TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);

	FPropertyEditorModule& PropertyEditor =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.bAllowSearch = false;
	Args.bHideSelectionTip = true;
	Args.bShowOptions = false;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	Args.ColumnWidth = NameColumnShare;
	SelectedBuildingView = PropertyEditor.CreateDetailView(Args);

	// Simple view: the tool panel and selected building show only what a student decides.
	const FIsPropertyVisible Filter = FIsPropertyVisible::CreateRaw(this, &FHutongLayoutEdModeToolkit::IsPropertyVisible);
	if (DetailsView.IsValid()) DetailsView->SetIsPropertyVisibleDelegate(Filter);
	SelectedBuildingView->SetIsPropertyVisibleDelegate(Filter);

	// Read the level editor's selection, never write it: writing restarted the active tool
	// mid-click and dropped the plan handles.
	SelectionChangedHandle = USelection::SelectionChangedEvent.AddRaw(
		this, &FHutongLayoutEdModeToolkit::OnSelectionChanged);
	RefreshSelectedBuilding();
}

void FHutongLayoutEdModeToolkit::OnSelectionChanged(UObject* Object)
{
	// Actor selection only: the component selection raises this too.
	if (GEditor && Object != GEditor->GetSelectedActors()) return;
	if (UHutongLayoutModeSettings* Settings = UHutongLayoutEdMode::GetActiveSettings()) Settings->SyncToSelection();
	RefreshSelectedBuilding();
	// The Selection section shows only with something selected; the layout is built once per refresh.
	if (ModeDetailsView.IsValid()) ModeDetailsView->ForceRefresh();
}

void FHutongLayoutEdModeToolkit::RefreshSelectedBuilding()
{
	if (!SelectedBuildingView.IsValid()) return;

	TArray<UObject*> Objects;
	SelectedBuildingLabel = FText::GetEmpty();
	for (UHutongBuildingComponent* B : HutongDetailOps::CollectSelected())
	{
		Objects.Add(B);
	}
	if (Objects.Num() == 1)
	{
		const UHutongBuildingComponent* B = Cast<UHutongBuildingComponent>(Objects[0]);
		SelectedBuildingLabel = FText::Format(LOCTEXT("SelectedOne", "Selected: {0}"),
			B ? B->GetTypeLabel() : FText::GetEmpty());
	}
	else if (Objects.Num() > 1)
	{
		SelectedBuildingLabel = FText::Format(LOCTEXT("SelectedMany", "Selected: {0} buildings"),
			FText::AsNumber(Objects.Num()));
	}
	SelectedBuildingView->SetObjects(Objects, /*bForceRefresh*/ true);
}

FText FHutongLayoutEdModeToolkit::GetSelectedBuildingText() const
{
	return SelectedBuildingLabel;
}

EVisibility FHutongLayoutEdModeToolkit::GetSelectedBuildingVisibility() const
{
	return SelectedBuildingLabel.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

FName FHutongLayoutEdModeToolkit::GetToolkitFName() const
{
	return FName("HutongLayoutEdMode");
}

FText FHutongLayoutEdModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Hutong Layout");
}

// Five palettes, not one: seventeen buttons wrap into a grid too narrow for their labels.
namespace HutongPalettes
{
	static const FName Buildings("Buildings");
	static const FName Gates("Gates");
	static const FName Enclosure("Enclosure");
	static const FName Courtyard("Courtyard");
	static const FName Layout("Layout");

	// Actions on placed buildings (promotion, rebuild, export, import), not on the one being
	// placed; kept off every tool's panel.
	static const FName Scene("Scene");
}

void FHutongLayoutEdModeToolkit::GetToolPaletteNames(TArray<FName>& OutPaletteNames) const
{
	OutPaletteNames.Add(HutongPalettes::Buildings);
	OutPaletteNames.Add(HutongPalettes::Gates);
	OutPaletteNames.Add(HutongPalettes::Enclosure);
	OutPaletteNames.Add(HutongPalettes::Courtyard);
	OutPaletteNames.Add(HutongPalettes::Layout);
	OutPaletteNames.Add(HutongPalettes::Scene);
}

FText FHutongLayoutEdModeToolkit::GetToolPaletteDisplayName(FName PaletteName) const
{
	if (PaletteName == HutongPalettes::Buildings) return LOCTEXT("PaletteBuildings", "Buildings");
	if (PaletteName == HutongPalettes::Gates)     return LOCTEXT("PaletteGates", "Gates");
	if (PaletteName == HutongPalettes::Enclosure) return LOCTEXT("PaletteEnclosure", "Walls");
	if (PaletteName == HutongPalettes::Courtyard) return LOCTEXT("PaletteCourtyard", "Garden");
	if (PaletteName == HutongPalettes::Layout)    return LOCTEXT("PaletteLayout", "Tools");
	if (PaletteName == HutongPalettes::Scene)     return LOCTEXT("PaletteScene", "Scene");
	return FText::FromName(PaletteName);
}

// A tab asks what to place next and the panel answers for the active tool, so a switch closes
// a tool not on the new tab (else e.g. House params sat under Gates, no button pressed) and the
// panel shows help until a tool is picked. Switching to the active tool's tab leaves it alone.
void FHutongLayoutEdModeToolkit::OnToolPaletteChanged(FName PaletteName)
{
	UHutongLayoutEdMode* Mode = Cast<UHutongLayoutEdMode>(OwningEditorMode.Get());
	UInteractiveToolManager* ToolManager = Mode ? Mode->GetToolManager() : nullptr;
	if (!ToolManager) return;

	const FString Active = ToolManager->GetActiveToolName(EToolSide::Left);
	if (Active.IsEmpty() || Mode->IsToolOnPalette(Active, PaletteName)) return;

	// Cancel, not accept: a tab switch is not the click that finishes a half-drawn rect.
	ToolManager->DeactivateTool(EToolSide::Left, EToolShutdownType::Cancel);
}

void FHutongLayoutEdModeToolkit::BuildToolPalette(FName PaletteName, FToolBarBuilder& ToolbarBuilder)
{
	const FHutongLayoutCommands& Commands = FHutongLayoutCommands::Get();

	if (PaletteName == HutongPalettes::Buildings)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginSiheyuanTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginEarPassageTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginShopfrontTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginStoreyTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginHallTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPavilionTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginFrameTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginSmallBuildingTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginStreetRowTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginCompoundTool);
	}
	else if (PaletteName == HutongPalettes::Gates)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginGateHouseTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginInnerGateTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPaifangTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginScreenWallTool);
	}
	else if (PaletteName == HutongPalettes::Enclosure)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginWallTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginCourtWallTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginCityWallTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginCorridorTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginPathTool);
	}
	else if (PaletteName == HutongPalettes::Courtyard)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginFlowerBedTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginWaterJarTool);
	}
	else if (PaletteName == HutongPalettes::Layout)
	{
		ToolbarBuilder.AddToolBarButton(Commands.BeginGalleryTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginGalleryWallsTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginGalleryHousesTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginGalleryGatesTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginGalleryCourtyardTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginGalleryStreetTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginGalleryTemplesTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginUnknownTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginMeasureTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginHeightsTool);
		ToolbarBuilder.AddToolBarButton(Commands.BeginCourtsTool);
		// The import *tool* stays with the placement tools: it drags a set onto the ground, and the
		// Scene tab has no tool panel for its path and folder.
		ToolbarBuilder.AddToolBarButton(Commands.BeginImportTool);
	}
}

bool FHutongLayoutEdModeToolkit::IsScenePaletteActive() const
{
	return GetCurrentPalette() == HutongPalettes::Scene;
}

EVisibility FHutongLayoutEdModeToolkit::GetToolPanelVisibility() const
{
	return IsScenePaletteActive() ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility FHutongLayoutEdModeToolkit::GetScenePanelVisibility() const
{
	return IsScenePaletteActive() ? EVisibility::Visible : EVisibility::Collapsed;
}

URectDragToolBase* FHutongLayoutEdModeToolkit::GetActiveRectTool() const
{
	UEdMode* Mode = GetScriptableEditorMode().Get();
	if (!Mode) return nullptr;
	UInteractiveToolManager* ToolManager = Mode->GetToolManager();
	if (!ToolManager) return nullptr;
	return Cast<URectDragToolBase>(ToolManager->GetActiveTool(EToolSide::Left));
}

// Polled every frame by the Slate attribute.
FText FHutongLayoutEdModeToolkit::GetPlacementPromptText() const
{
	const URectDragToolBase* Tool = GetActiveRectTool();
	return Tool ? Tool->GetStagePromptText()
		: LOCTEXT("NoToolPrompt", "Pick a tool above.");
}

FSlateColor FHutongLayoutEdModeToolkit::GetPlacementPromptColor() const
{
	const URectDragToolBase* Tool = GetActiveRectTool();
	const bool bPlacing = Tool && Tool->IsPlacingActive();
	return bPlacing ? FSlateColor(FLinearColor(1.0f, 0.9f, 0.15f))
		: FSlateColor::UseForeground();
}

FText FHutongLayoutEdModeToolkit::GetHoverText() const
{
	const URectDragToolBase* Tool = GetActiveRectTool();
	return Tool ? Tool->GetHoverSummaryText() : FText::GetEmpty();
}

FText FHutongLayoutEdModeToolkit::GetHoverHeaderText() const
{
	const URectDragToolBase* Tool = GetActiveRectTool();
	return Tool && Tool->IsHoverUnderCursor()
		? LOCTEXT("HoverHeader", "Under the cursor")
		: LOCTEXT("SelectedHeader", "Selected");
}

EVisibility FHutongLayoutEdModeToolkit::GetHoverVisibility() const
{
	// Mid-placement the panel shows only the piece going down: students read the hover readout as
	// describing what they were placing.
	const URectDragToolBase* Tool = GetActiveRectTool();
	if (!Tool || Tool->IsPlacingActive()) return EVisibility::Collapsed;
	return GetHoverText().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

bool FHutongLayoutEdModeToolkit::IsPropertyVisible(const FPropertyAndParent& PropertyAndParent) const
{
	const UHutongLayoutModeSettings* Settings = UHutongLayoutEdMode::GetActiveSettings();
	const bool bAdvanced = Settings && Settings->bShowAdvancedSettings;
	return HutongPanel::IsVisible(PropertyAndParent.Property, PropertyAndParent.ParentProperties, bAdvanced);
}

ECheckBoxState FHutongLayoutEdModeToolkit::GetShowAdvancedState() const
{
	const UHutongLayoutModeSettings* Settings = UHutongLayoutEdMode::GetActiveSettings();
	return Settings && Settings->bShowAdvancedSettings ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FHutongLayoutEdModeToolkit::OnShowAdvancedChanged(ECheckBoxState State)
{
	UHutongLayoutModeSettings* Settings = UHutongLayoutEdMode::GetActiveSettings();
	if (!Settings) return;
	Settings->bShowAdvancedSettings = (State == ECheckBoxState::Checked);
	Settings->SaveConfig();
	// The filter is asked at row build, so rebuild the views.
	if (DetailsView.IsValid()) DetailsView->ForceRefresh();
	if (SelectedBuildingView.IsValid()) SelectedBuildingView->ForceRefresh();
}

EVisibility FHutongLayoutEdModeToolkit::GetHelpVisibility() const
{
	// Hidden outright when no tool is active.
	return GetActiveRectTool() ? EVisibility::Visible : EVisibility::Collapsed;
}

FText FHutongLayoutEdModeToolkit::GetHelpText() const
{
	const URectDragToolBase* Tool = GetActiveRectTool();
	if (!Tool)
	{
		return FText::GetEmpty();
	}

	FString Joined;
	for (const FText& Line : Tool->GetToolHelpLines())
	{
		if (!Joined.IsEmpty()) Joined += TEXT("\n");
		Joined += TEXT("\u2022  ") + Line.ToString();
	}
	return FText::FromString(Joined);
}

FText FHutongLayoutEdModeToolkit::GetKeyHintText() const
{
	const URectDragToolBase* Tool = GetActiveRectTool();
	return Tool ? Tool->GetKeyHintText() : FText::GetEmpty();
}

static TSharedPtr<FUICommandInfo> ActiveToolCommand(const FModeToolkit& Toolkit)
{
	const UHutongLayoutEdMode* Mode = Cast<UHutongLayoutEdMode>(Toolkit.GetScriptableEditorMode().Get());
	if (!Mode || !Mode->GetToolManager()) return nullptr;
	return Mode->FindToolCommand(Mode->GetToolManager()->GetActiveToolName(EToolSide::Left));
}

FText FHutongLayoutEdModeToolkit::GetToolTitleText() const
{
	const TSharedPtr<FUICommandInfo> Command = ActiveToolCommand(*this);
	return Command ? Command->GetLabel() : FText::GetEmpty();
}

FText FHutongLayoutEdModeToolkit::GetToolDescriptionText() const
{
	// The tooltip's first line: the sentence that says what the tool places.
	const TSharedPtr<FUICommandInfo> Command = ActiveToolCommand(*this);
	if (!Command) return FText::GetEmpty();
	FString First;
	Command->GetDescription().ToString().Split(TEXT("\n"), &First, nullptr);
	return First.IsEmpty() ? Command->GetDescription() : FText::FromString(First);
}

FText FHutongLayoutEdModeToolkit::GetStageLabel(int32 Index) const
{
	const URectDragToolBase* Tool = GetActiveRectTool();
	if (!Tool) return FText::GetEmpty();
	const TArray<FText> Names = Tool->GetStageNames();
	if (!Names.IsValidIndex(Index)) return FText::GetEmpty();
	return FText::Format(LOCTEXT("StageLabel", "{0} {1}"), Index + 1, Names[Index]);
}

FSlateColor FHutongLayoutEdModeToolkit::GetStageColor(int32 Index) const
{
	const URectDragToolBase* Tool = GetActiveRectTool();
	const bool bCurrent = Tool && Tool->GetStageIndex() == Index;
	return bCurrent ? FSlateColor(FLinearColor(1.0f, 0.9f, 0.15f)) : FSlateColor::UseSubduedForeground();
}

EVisibility FHutongLayoutEdModeToolkit::GetStageVisibility(int32 Index) const
{
	const URectDragToolBase* Tool = GetActiveRectTool();
	return Tool && Tool->GetStageNames().IsValidIndex(Index) ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> FHutongLayoutEdModeToolkit::MakeStageRow() const
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (int32 i = 0; i < 4; ++i)
	{
		Row->AddSlot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 10.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(this, &FHutongLayoutEdModeToolkit::GetStageLabel, i)
			.ColorAndOpacity(this, &FHutongLayoutEdModeToolkit::GetStageColor, i)
			.Visibility(this, &FHutongLayoutEdModeToolkit::GetStageVisibility, i)
			.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
		];
	}
	return Row;
}

ECheckBoxState FHutongLayoutEdModeToolkit::GetPlanOnlyState() const
{
	const UHutongLayoutModeSettings* Settings = UHutongLayoutEdMode::GetActiveSettings();
	return Settings && Settings->bPlanOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FHutongLayoutEdModeToolkit::OnPlanOnlyChanged(ECheckBoxState State)
{
	if (UHutongLayoutModeSettings* Settings = UHutongLayoutEdMode::GetActiveSettings())
	{
		Settings->bPlanOnly = (State == ECheckBoxState::Checked);
		Settings->SaveConfig();
	}
}

ECheckBoxState FHutongLayoutEdModeToolkit::GetShowPlansState() const
{
	return HutongPlanOutline::ArePlansVisible() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FHutongLayoutEdModeToolkit::OnShowPlansChanged(ECheckBoxState State)
{
	const bool bVisible = (State == ECheckBoxState::Checked);
	HutongPlanOutline::SetPlansVisible(bVisible);

	// The mode settings' checkbox is a mirror seeded on Enter and visible on the Scene tab; update
	// it or it contradicts this one.
	if (UHutongLayoutModeSettings* Settings = UHutongLayoutEdMode::GetActiveSettings())
	{
		Settings->bShowPlanOutlines = bVisible;
	}
}

TSharedRef<SWidget> FHutongLayoutEdModeToolkit::MakePlanRow() const
{
	FHutongLayoutEdModeToolkit* Self = const_cast<FHutongLayoutEdModeToolkit*>(this);
	// Two rows: side by side the label ran under the button.
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(8.0f, 4.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SCheckBox)
				.IsChecked(this, &FHutongLayoutEdModeToolkit::GetPlanOnlyState)
				.OnCheckStateChanged(Self, &FHutongLayoutEdModeToolkit::OnPlanOnlyChanged)
				.ToolTipText(LOCTEXT("PlanOnlyTip", "New buildings are placed as outlines only."))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PlanOnly", "Layout only (no geometry)"))
					.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
					.AutoWrapText(true)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SCheckBox)
				.IsChecked(this, &FHutongLayoutEdModeToolkit::GetShowPlansState)
				.OnCheckStateChanged(Self, &FHutongLayoutEdModeToolkit::OnShowPlansChanged)
				.ToolTipText(LOCTEXT("ShowPlansTip", "Show every building's footprint outline."))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ShowPlans", "Show plan outlines"))
					.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
					.AutoWrapText(true)
				]
			]
		];
}

TSharedPtr<SWidget> FHutongLayoutEdModeToolkit::GetInlineContent() const
{
	// Two panels in one slot, picked by the palette tab: the tool being placed, or what is down.
	// Bound, not rebuilt, since GetInlineContent runs once.
	return SNew(SVerticalBox)

		// The tool side, under one binding: the plan row, the help block and the tool's own panel.
		+ SVerticalBox::Slot()
		[
			SNew(SVerticalBox)
			.Visibility(this, &FHutongLayoutEdModeToolkit::GetToolPanelVisibility)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				MakePlanRow()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				// The whole block goes with the help.
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(8.0f, 6.0f))
				.Visibility(this, &FHutongLayoutEdModeToolkit::GetHelpVisibility)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						// What is being placed: palette label, then the tooltip's first line.
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							// What it places on hover: beside the title it wrapped down the panel.
							SNew(STextBlock)
							.Text(this, &FHutongLayoutEdModeToolkit::GetToolTitleText)
							.ToolTipText(this, &FHutongLayoutEdModeToolkit::GetToolDescriptionText)
							.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						// The clicks the placement takes, the current one lit.
						MakeStageRow()
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(STextBlock)
						.Text(this, &FHutongLayoutEdModeToolkit::GetPlacementPromptText)
						.ColorAndOpacity(this, &FHutongLayoutEdModeToolkit::GetPlacementPromptColor)
						.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
						.AutoWrapText(true)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock)
						.Text(this, &FHutongLayoutEdModeToolkit::GetKeyHintText)
						.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.AutoWrapText(true)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SExpandableArea)
						.InitiallyCollapsed(true)
						.HeaderPadding(FMargin(0.0f, 2.0f))
						.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
						.BorderImage(FAppStyle::GetBrush("NoBorder"))
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("HelpHeader", "How this tool works"))
							.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						]
						.BodyContent()
						[
							// Polled, like the prompt above it.
							SNew(STextBlock)
							.Text(this, &FHutongLayoutEdModeToolkit::GetHelpText)
							.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.AutoWrapText(true)
						]
					]
				]
			]

			// What is already down under the cursor or selected, in its own headed box so it is not read
			// as the piece being placed.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
				.Padding(FMargin(8.0f, 6.0f))
				.Visibility(this, &FHutongLayoutEdModeToolkit::GetHoverVisibility)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock)
						.Text(this, &FHutongLayoutEdModeToolkit::GetHoverHeaderText)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &FHutongLayoutEdModeToolkit::GetHoverText)
						.ColorAndOpacity(FSlateColor(FLinearColor(0.25f, 0.85f, 1.0f)))
						.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
						.AutoWrapText(true)
					]
				]
			]

			// The simple / advanced switch, right above the settings it hides.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(8.0f, 6.0f, 8.0f, 4.0f)
			[
				SNew(SVerticalBox)
				.Visibility(this, &FHutongLayoutEdModeToolkit::GetHelpVisibility)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SCheckBox)
					.IsChecked(this, &FHutongLayoutEdModeToolkit::GetShowAdvancedState)
					.OnCheckStateChanged(const_cast<FHutongLayoutEdModeToolkit*>(this), &FHutongLayoutEdModeToolkit::OnShowAdvancedChanged)
					.ToolTipText(LOCTEXT("AdvancedTip", "Show every setting, not just the main ones."))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ShowAdvanced", "Show advanced settings"))
						.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
					]
				]
			]

			+ SVerticalBox::Slot()
			[
				DetailsView.ToSharedRef()
			]
		]

		// Actions on placed buildings (geometry, selection, export, import, housekeeping, information), on their own tab.
		+ SVerticalBox::Slot()
		[
			SNew(SVerticalBox)
			.Visibility(this, &FHutongLayoutEdModeToolkit::GetScenePanelVisibility)

			+ SVerticalBox::Slot()
			[
				ModeDetailsView.ToSharedRef()
			]

			// What is selected, edited here rather than in the Details panel's component tree.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f, 8.0f, 4.0f, 2.0f)
			[
				SNew(STextBlock)
				.Visibility(this, &FHutongLayoutEdModeToolkit::GetSelectedBuildingVisibility)
				.Text(this, &FHutongLayoutEdModeToolkit::GetSelectedBuildingText)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.78f, 1.0f)))
			]

			+ SVerticalBox::Slot()
			[
				SNew(SBox)
				.Visibility(this, &FHutongLayoutEdModeToolkit::GetSelectedBuildingVisibility)
				[
					SelectedBuildingView.ToSharedRef()
				]
			]

			// Which build is running, under everything: inside the settings list the selected
			// building's view covered it.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f, 6.0f, 4.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(HutongPanelCustomizations::PluginUpdatedText())
				.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
}

#undef LOCTEXT_NAMESPACE
