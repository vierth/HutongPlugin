#include "HutongLayoutEdModeToolkit.h"
#include "HutongLayoutCommands.h"
#include "Tools/RectDragToolBase.h"
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
	SelectedBuildingView = PropertyEditor.CreateDetailView(Args);

	// The level editor's selection is read, never written: writing it from here restarted the
	// active tool mid-click and the plan handles went with it.
	SelectionChangedHandle = USelection::SelectionChangedEvent.AddRaw(
		this, &FHutongLayoutEdModeToolkit::OnSelectionChanged);
	RefreshSelectedBuilding();
}

void FHutongLayoutEdModeToolkit::OnSelectionChanged(UObject* Object)
{
	// Actor selection only: the component selection raises this too.
	if (GEditor && Object != GEditor->GetSelectedActors()) return;
	RefreshSelectedBuilding();
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

// Five palettes, not one: seventeen buttons in a single palette wrap into a grid the panel is too narrow for and the labels truncate.
namespace HutongPalettes
{
	static const FName Buildings("Buildings");
	static const FName Gates("Gates");
	static const FName Enclosure("Enclosure");
	static const FName Courtyard("Courtyard");
	static const FName Layout("Layout");

	// What acts on buildings already down — promotion, rebuilding, export and import — rather than
	// on the one being placed. It was reaching every tool's panel as a collapsed section at the
	// foot of it, on every tool, on every visit.
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

// A tab is a question about what to place next, and the panel under it answers for the tool that
// is up. Left running across a switch, the House tool's parameters sat under the Gates tab with
// every button on it unpressed — a panel describing something the tab does not offer. So a tool
// that is not on the tab being switched to is closed, and the panel goes back to its help block
// until a tool on this tab is picked. Switching to the tab a tool is already on leaves it alone.
void FHutongLayoutEdModeToolkit::OnToolPaletteChanged(FName PaletteName)
{
	UHutongLayoutEdMode* Mode = Cast<UHutongLayoutEdMode>(OwningEditorMode.Get());
	UInteractiveToolManager* ToolManager = Mode ? Mode->GetToolManager() : nullptr;
	if (!ToolManager) return;

	const FString Active = ToolManager->GetActiveToolName(EToolSide::Left);
	if (Active.IsEmpty() || Mode->IsToolOnPalette(Active, PaletteName)) return;

	// Cancel, not accept: a half-drawn rectangle is a placement the user did not finish, and a tab
	// switch is not the click that finishes it.
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
		ToolbarBuilder.AddToolBarButton(Commands.BeginMeasureTool);
		// The import *tool* stays with the placement tools: it drags a set out onto the ground,
		// and the Scene tab has no tool panel to show its file path and folder in.
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
		: LOCTEXT("NoToolPrompt", "Pick a tool above to start placing.");
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

EVisibility FHutongLayoutEdModeToolkit::GetHoverVisibility() const
{
	return GetHoverText().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
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

	// The mode settings' own checkbox is a mirror seeded on Enter, and the Scene tab is showing it
	// right now: left alone it would sit there contradicting this one.
	if (UHutongLayoutModeSettings* Settings = UHutongLayoutEdMode::GetActiveSettings())
	{
		Settings->bShowPlanOutlines = bVisible;
	}
}

FReply FHutongLayoutEdModeToolkit::OnGenerateClicked()
{
	if (UHutongLayoutModeSettings* Settings = UHutongLayoutEdMode::GetActiveSettings())
	{
		Settings->GenerateLoadedGeometry();
	}
	return FReply::Handled();
}

TSharedRef<SWidget> FHutongLayoutEdModeToolkit::MakePlanRow() const
{
	FHutongLayoutEdModeToolkit* Self = const_cast<FHutongLayoutEdModeToolkit*>(this);
	// Two rows, not one: side by side the label ran under the button at the panel's width.
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
				.ToolTipText(LOCTEXT("PlanOnlyTip",
					"Places new buildings as footprint outlines with no geometry."))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PlanOnly", "Layout only (outlines, no geometry)"))
					.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
					.AutoWrapText(true)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SCheckBox)
				.IsChecked(this, &FHutongLayoutEdModeToolkit::GetShowPlansState)
				.OnCheckStateChanged(Self, &FHutongLayoutEdModeToolkit::OnShowPlansChanged)
				.ToolTipText(LOCTEXT("ShowPlansTip",
					"Draws the footprint outline of every laid-out building."))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ShowPlans", "Show plan outlines"))
					.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
					.AutoWrapText(true)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("Generate", "Generate Geometry"))
				.ToolTipText(LOCTEXT("GenerateTip",
					"Builds every laid-out building in the loaded region."))
				.OnClicked(Self, &FHutongLayoutEdModeToolkit::OnGenerateClicked)
			]
		];
}

TSharedPtr<SWidget> FHutongLayoutEdModeToolkit::GetInlineContent() const
{
	// Two panels in one slot, and the palette tab decides which is up: the tool being placed, or
	// what is already down. Bound rather than rebuilt, because GetInlineContent runs once.
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
						// What is being placed, in the palette's own words: label, then the first line of the tooltip.
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(this, &FHutongLayoutEdModeToolkit::GetToolTitleText)
							.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(this, &FHutongLayoutEdModeToolkit::GetToolDescriptionText)
							.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.AutoWrapText(true)
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
					.Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						// What the cursor is resting on.
						SNew(STextBlock)
						.Text(this, &FHutongLayoutEdModeToolkit::GetHoverText)
						.Visibility(this, &FHutongLayoutEdModeToolkit::GetHoverVisibility)
						.ColorAndOpacity(FSlateColor(FLinearColor(0.25f, 0.85f, 1.0f)))
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

			+ SVerticalBox::Slot()
			[
				DetailsView.ToSharedRef()
			]
		]

		// Promote, rebuild, export, import, count — everything that acts on buildings already
		// down, on its own tab and open, rather than collapsed at the foot of whichever tool
		// happened to be active.
		+ SVerticalBox::Slot()
		[
			SNew(SVerticalBox)
			.Visibility(this, &FHutongLayoutEdModeToolkit::GetScenePanelVisibility)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f, 4.0f, 4.0f, 6.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SceneHeader", "What is already placed: promote, rebuild, export and import."))
				.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]

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
		];
}

#undef LOCTEXT_NAMESPACE
