#include "Widgets/SPlaceLabelsPanel.h"

#include "PlaceLabelHierarchy.h"
#include "PlaceLabelTypes.h"
#include "PlaceLabelsEditor.h"
#include "PlaceLabelsEdMode.h"
#include "PlaceLabelsEditorUtils.h"
#include "PlaceLabelsExchange.h"
#include "PlaceLabelsTopology.h"
#include "PlaceRegionComponent.h"
#include "Tools/PlaceRegionEditCore.h"
#include "Tools/PlaceRegionEditTool.h"
#include "Tools/PlaceRegionPenTool.h"
#include "Tools/PlaceRegionSelectTool.h"
#include "DesktopPlatformModule.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "Engine/Selection.h"
#include "InteractiveToolManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "SPlaceLabelsPanel"

namespace
{
	const FName ColIdColour("Colour");
	const FName ColIdName("Name");
	const FName ColIdType("Type");
	const FName ColIdSize("Size");
	const FName ColIdIssues("Issues");

	const FLinearColor SevereColour(1.0f, 0.25f, 0.2f);
	const FLinearColor WarnColour(1.0f, 0.6f, 0.15f);

	const FString AllTypesLabel(TEXT("All types"));

	// The Place Labels tools, if that mode is up. Declared here rather than beside its first
	// caller: the inline editor asks it which region the form is on.
	UInteractiveTool* GetActivePlaceLabelsTool()
	{
		UEdMode* Mode = GLevelEditorModeTools().GetActiveScriptableMode(
			UPlaceLabelsEdMode::EM_PlaceLabelsModeId);
		if (!Mode)
		{
			return nullptr;
		}
		UInteractiveToolManager* ToolManager = Mode->GetToolManager();
		return ToolManager ? ToolManager->GetActiveTool(EToolSide::Left) : nullptr;
	}

	// HutongLayout draws a coloured polygon on the ground for each of its laid-out buildings, over
	// exactly the ground a region is traced onto. Its console variable is the switch rather than a
	// call into the plugin: the two share no code, and what is being asked for is one bool. Absent
	// HutongLayout the variable is not registered and the checkbox is not offered.
	const TCHAR* HutongPlansVarName = TEXT("hutong.ShowPlanOutlines");

	IConsoleVariable* FindHutongPlansVar()
	{
		return IConsoleManager::Get().FindConsoleVariable(HutongPlansVarName);
	}

	// The browser and the problems list both live inside a mode panel that is already scrollable.
	constexpr float RegionListMaxHeight = 260.0f;
	constexpr float ProblemListMaxHeight = 170.0f;

	// Cheap enough to run every second: it walks loaded actors and reads cached data off each.
	constexpr float RefreshIntervalSeconds = 1.0f;
}

// A region row. Multi-column so the colour swatch and the warning badge line up down the list.
class SPlaceRegionTableRow : public SMultiColumnTableRow<TSharedPtr<FPlaceRegionRow>>
{
public:
	SLATE_BEGIN_ARGS(SPlaceRegionTableRow) {}
		SLATE_ARGUMENT(TSharedPtr<FPlaceRegionRow>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
	{
		Item = InArgs._Item;
		SMultiColumnTableRow<TSharedPtr<FPlaceRegionRow>>::Construct(FSuperRowType::FArguments(),
			OwnerTable);
	}

	// Every cell is bound, not baked.
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (!Item.IsValid())
		{
			return SNullWidget::NullWidget;
		}
		TSharedPtr<FPlaceRegionRow> Row = Item;

		if (ColumnName == ColIdColour)
		{
			// The type's own editor colour.
			return SNew(SBox)
				.WidthOverride(10.0f)
				.HeightOverride(10.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("WhiteBrush"))
					.ColorAndOpacity_Lambda([Row]() { return FSlateColor(Row->Colour); })
				];
		}

		if (ColumnName == ColIdName)
		{
			return SNew(STextBlock)
				.Text_Lambda([Row]() { return Row->DisplayName; })
				.ToolTipText_Lambda([Row]() { return Row->DisplayName; })
				.ColorAndOpacity_Lambda([Row]()
				{
					return Row->IssueCount > 0
						? FSlateColor(Row->bHasSevereIssue ? SevereColour : WarnColour)
						: FSlateColor::UseForeground();
				});
		}

		if (ColumnName == ColIdType)
		{
			return SNew(STextBlock)
				.Text_Lambda([Row]() { return Row->TypeLabel; })
				.ColorAndOpacity(FSlateColor::UseSubduedForeground());
		}

		if (ColumnName == ColIdSize)
		{
			return SNew(STextBlock)
				.Text_Lambda([Row]()
				{
					return FText::FromString(FString::Printf(TEXT("%d · %.0f m²"),
						Row->Corners, Row->AreaSquareMetres));
				})
				.ColorAndOpacity(FSlateColor::UseSubduedForeground());
		}

		if (ColumnName == ColIdIssues)
		{
			// Always a text block, empty when there is nothing to report.
			return SNew(STextBlock)
				.Text_Lambda([Row]()
				{
					return Row->IssueCount > 0 ? FText::AsNumber(Row->IssueCount) : FText::GetEmpty();
				})
				.ColorAndOpacity_Lambda([Row]()
				{
					return FSlateColor(Row->bHasSevereIssue ? SevereColour : WarnColour);
				})
				.ToolTipText(LOCTEXT("RowIssueTip",
					"This region has problems — see the Problems section below."));
		}

		return SNullWidget::NullWidget;
	}

private:
	TSharedPtr<FPlaceRegionRow> Item;
};

void SPlaceLabelsPanel::Construct(const FArguments& InArgs)
{
	TypeFilterOptions.Add(MakeShared<FString>(AllTypesLabel));
	SelectedTypeFilter = TypeFilterOptions[0];

	auto MakeActionButton = [](const FText& Label, const FText& Tip, FOnClicked OnClicked)
	{
		return SNew(SBox)
			.Padding(FMargin(0.0f, 0.0f, 4.0f, 4.0f))
			[
				SNew(SButton)
				.Text(Label)
				.ToolTipText(Tip)
				.OnClicked(OnClicked)
			];
	};

	ChildSlot
	[
		SNew(SVerticalBox)

		// ---- Stage prompt.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(8.0f, 6.0f, 8.0f, 4.0f))
		[
			SNew(STextBlock)
			.Text(this, &SPlaceLabelsPanel::GetPromptText)
			.ColorAndOpacity(this, &SPlaceLabelsPanel::GetPromptColour)
			.AutoWrapText(true)
		]

		// ---- The two halves of the panel. Drawing regions and naming them is the work; welding,
		// exporting, importing and creating the type assets are done once a session, and eight
		// buttons standing over the browser are eight buttons in the way of it.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(8.0f, 2.0f, 8.0f, 4.0f))
		[
			SNew(SSegmentedControl<EPlaceLabelsPanelTab>)
			.Value_Lambda([this]() { return ActiveTab; })
			.OnValueChanged_Lambda([this](EPlaceLabelsPanelTab NewTab) { ActiveTab = NewTab; })

			+ SSegmentedControl<EPlaceLabelsPanelTab>::Slot(EPlaceLabelsPanelTab::Regions)
			.Text(LOCTEXT("TabRegions", "Regions"))
			.ToolTip(LOCTEXT("TabRegionsTip",
				"Drawing, naming and fixing the regions in this level."))

			+ SSegmentedControl<EPlaceLabelsPanelTab>::Slot(EPlaceLabelsPanelTab::Tools)
			.Text(LOCTEXT("TabTools", "Tools"))
			.ToolTip(LOCTEXT("TabToolsTip",
				"Hierarchy, welding, import and export, the starter type assets, and what the "
				"viewport draws. Set once and left alone."))
		]

		// ---- Regions: what the session is actually spent on.
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SVerticalBox)
			.Visibility(this, &SPlaceLabelsPanel::GetRegionsTabVisibility)

			// ---- Help, folded away. It is worth reading once and worth hiding forever after.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(4.0f, 0.0f, 4.0f, 4.0f))
			[
				SNew(SExpandableArea)
				.InitiallyCollapsed(true)
				.AreaTitle(LOCTEXT("HelpTitle", "How to draw and edit regions"))
				.AreaTitleFont(FAppStyle::GetFontStyle("SmallFontBold"))
				.Padding(FMargin(8.0f, 4.0f))
				.BodyContent()
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().Padding(0, 1)
					[
						SNew(STextBlock).AutoWrapText(true).Font(FAppStyle::GetFontStyle("SmallFont"))
						.Text(LOCTEXT("HelpSelect",
						"Select is the tool that takes no clicks: with it up, clicking a region "
						"picks it as anywhere else in the editor, the form below follows what is "
						"picked, and the ordinary gizmo moves it. It is how you put the pen down."))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 1)
				[
					SNew(STextBlock).AutoWrapText(true).Font(FAppStyle::GetFontStyle("SmallFont"))
					.Text(LOCTEXT("HelpPen",
							"Pen: click the ground for each corner. Corners stay editable while you draw — "
							"drag one to move it, click an edge to add one, Alt+click or Delete to remove one. "
							"Click the first corner, or press Enter, to close the outline."))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 1)
					[
						SNew(STextBlock).AutoWrapText(true).Font(FAppStyle::GetFontStyle("SmallFont"))
						.Text(LOCTEXT("HelpNaming",
							"The closed outline turns green and is still adjustable. The form below is "
							"on the region you are drawing from the first corner until you place it — it "
							"says New Region across the top — so what you type there lands on the polygon "
							"in front of you and never on the last one. Fill in the type and name, then "
							"click Create Region: Enter belongs to the text field you are typing in, and "
							"only places the region once focus is back in the viewport. A region with no "
							"name or type is placed anyway and drawn in amber until you come back to it."))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 1)
					[
						SNew(STextBlock).AutoWrapText(true).Font(FAppStyle::GetFontStyle("SmallFont"))
						.Text(LOCTEXT("HelpMetadata",
							"Source and Evidence say where a name came from and how far the source "
							"actually says it, on the same 1–5 scale the Hutong Layout plugin uses. Both "
							"are kept between placements, since a lane is traced off one sheet in one "
							"sitting."))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 1)
					[
						SNew(STextBlock).AutoWrapText(true).Font(FAppStyle::GetFontStyle("SmallFont"))
						.Text(LOCTEXT("HelpEdit",
							"Edit: click any region already placed to pick it up — the form below follows "
							"it — then the same gestures — "
							"drag a corner, click an edge to add one, Alt+click to remove one. Click empty "
							"ground when you are done."))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 1)
					[
						SNew(STextBlock).AutoWrapText(true).Font(FAppStyle::GetFontStyle("SmallFont"))
						.Text(LOCTEXT("HelpSnapping",
							"Corners snap onto neighbouring regions so shared boundaries meet exactly. "
							"Hold Alt to ignore snapping; hold Shift while drawing to constrain to 15° steps. "
							"Click a HutongLayout building to trace its footprint exactly."))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 1)
					[
						SNew(STextBlock).AutoWrapText(true).Font(FAppStyle::GetFontStyle("SmallFont"))
						.Text(LOCTEXT("HelpBoundaries",
							"Boundaries that nearly meet are listed under Problems and marked in amber in "
							"the viewport. Weld closes them exactly. Nothing is welded unless you ask — a "
							"gap you drew on purpose is left alone."))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 1)
					[
						SNew(STextBlock).AutoWrapText(true).Font(FAppStyle::GetFontStyle("SmallFont"))
						.Text(LOCTEXT("HelpStale",
							"Adding or moving a region can stale other regions' parents — press Recompute "
							"Hierarchy after a batch of edits."))
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SSeparator).Thickness(1.0f)
			]

			// ---- The browser.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 2.0f, 8.0f, 2.0f))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(SearchBox, SSearchBox)
					.HintText(LOCTEXT("SearchHint", "Search regions"))
					.OnTextChanged(this, &SPlaceLabelsPanel::OnSearchTextChanged)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.MinDesiredWidth(90.0f)
					[
						SAssignNew(TypeFilterCombo, SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&TypeFilterOptions)
						.OnGenerateWidget(this, &SPlaceLabelsPanel::MakeTypeFilterEntry)
						.OnSelectionChanged(this, &SPlaceLabelsPanel::OnTypeFilterChanged)
						[
							SNew(STextBlock).Text(this, &SPlaceLabelsPanel::GetTypeFilterLabel)
						]
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SCheckBox)
					.ToolTipText(LOCTEXT("ProblemsOnlyTip", "Show only regions with something wrong."))
					.IsChecked_Lambda([this]()
					{
						return bProblemsOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
					{
						bProblemsOnly = (State == ECheckBoxState::Checked);
						// Filtering on problems is meaningless until the report has been run at least once, and the report only runs on demand when the section is collapsed.
						if (bProblemsOnly)
						{
							RebuildProblemRows();
						}
						ApplyFilter();
					})
					[
						SNew(STextBlock)
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.Text(LOCTEXT("ProblemsOnly", "Problems"))
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 2.0f))
			[
				SNew(SBox)
				.MaxDesiredHeight(RegionListMaxHeight)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(1.0f)
					[
						SAssignNew(RegionListView, SListView<TSharedPtr<FPlaceRegionRow>>)
						.ListItemsSource(&VisibleRegionRows)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow(this, &SPlaceLabelsPanel::MakeRegionRow)
						.OnSelectionChanged_Lambda(
							[this](TSharedPtr<FPlaceRegionRow> Item, ESelectInfo::Type SelectInfo)
							{
								if (SelectInfo != ESelectInfo::Direct)
								{
									OnRegionClicked(Item);
								}
							})
						.OnMouseButtonDoubleClick(this, &SPlaceLabelsPanel::OnRegionDoubleClicked)
						.HeaderRow(
							SNew(SHeaderRow)
							+ SHeaderRow::Column(ColIdColour).DefaultLabel(FText::GetEmpty())
								.FixedWidth(18.0f)
							+ SHeaderRow::Column(ColIdName).DefaultLabel(LOCTEXT("ColName", "Name"))
								.FillWidth(1.0f)
							+ SHeaderRow::Column(ColIdType).DefaultLabel(LOCTEXT("ColType", "Type"))
								.FillWidth(0.55f)
							+ SHeaderRow::Column(ColIdSize).DefaultLabel(LOCTEXT("ColSize", "Size"))
								.FixedWidth(96.0f)
							+ SHeaderRow::Column(ColIdIssues).DefaultLabel(FText::GetEmpty())
								.FixedWidth(22.0f))
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 0.0f, 8.0f, 4.0f))
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle("SmallFont"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(this, &SPlaceLabelsPanel::GetSummaryText)
			]

			// ---- The inline editor, right under the list that sends you here.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 0.0f, 8.0f, 4.0f))
			[
				BuildRegionEditor()
			]

			// ---- Problems, collapsed until asked for.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(4.0f, 0.0f, 4.0f, 4.0f))
			[
				SNew(SExpandableArea)
				.InitiallyCollapsed(true)
				.AreaTitleFont(FAppStyle::GetFontStyle("SmallFontBold"))
				.Padding(FMargin(6.0f, 4.0f))
				.OnAreaExpansionChanged_Lambda([this](bool bExpanded)
				{
					bProblemsExpanded = bExpanded;
					if (bExpanded)
					{
						RebuildProblemRows();
					}
				})
				.HeaderContent()
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("SmallFontBold"))
					.Text(this, &SPlaceLabelsPanel::GetProblemsHeaderText)
				]
				.BodyContent()
				[
					SNew(SBox)
					.MaxDesiredHeight(ProblemListMaxHeight)
					[
						SAssignNew(ProblemListView, SListView<TSharedPtr<FPlaceLabelsProblemRow>>)
						.ListItemsSource(&ProblemRows)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow(this, &SPlaceLabelsPanel::MakeProblemRow)
						.OnSelectionChanged_Lambda(
							[this](TSharedPtr<FPlaceLabelsProblemRow> Item, ESelectInfo::Type SelectInfo)
							{
								if (SelectInfo != ESelectInfo::Direct)
								{
									OnProblemClicked(Item);
								}
							})
					]
				]
			]
		]

		// ---- Tools.
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SVerticalBox)
			.Visibility(this, &SPlaceLabelsPanel::GetToolsTabVisibility)

			// ---- Actions, wrapped rather than stacked. Eight full-width buttons is a wall.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 0.0f, 4.0f, 2.0f))
			[
				SNew(SWrapBox)
				.UseAllottedSize(true)

				+ SWrapBox::Slot()
				[
					MakeActionButton(
						LOCTEXT("RecomputeHierarchy", "Recompute Hierarchy"),
						LOCTEXT("RecomputeHierarchyTip",
							"Re-resolves every region's parent, coarsest type first. Regions with an "
							"explicit parent, or with Auto Parent switched off, are left alone."),
						FOnClicked::CreateSP(this, &SPlaceLabelsPanel::OnRecomputeHierarchy))
				]
				+ SWrapBox::Slot()
				[
					MakeActionButton(
						LOCTEXT("CreateStarterTypes", "Create Starter Types"),
						LOCTEXT("CreateStarterTypesTip",
							"Creates district, area, avenue, hutong, river, lake, moat, bridge, "
							"compound, temple and building type assets in /Game/PlaceLabels/Types, "
							"with their parenting rules already set up. Existing assets are never "
							"overwritten, so this also adds a type that was introduced after you "
							"last pressed it."),
						FOnClicked::CreateSP(this, &SPlaceLabelsPanel::OnCreateStarterTypes))
				]
				+ SWrapBox::Slot()
				[
					MakeActionButton(
						LOCTEXT("WeldAll", "Weld All Boundaries"),
						LOCTEXT("WeldAllTip",
							"Makes every pair of boundaries that nearly meet share exact corners, at the "
							"tolerance set below. Undoable in one step. Boundaries further apart than the "
							"tolerance are left completely alone."),
						FOnClicked::CreateSP(this, &SPlaceLabelsPanel::OnWeldAll))
				]
				+ SWrapBox::Slot()
				[
					MakeActionButton(
						LOCTEXT("ExportGeoJson", "Export GeoJSON"),
						LOCTEXT("ExportGeoJsonTip",
							"Writes every region's outline and labels to a GeoJSON file. Coordinates are "
							"Unreal world centimetres, not longitude and latitude. Re-importing it "
							"restores the outlines."),
						FOnClicked::CreateSP(this, &SPlaceLabelsPanel::OnExportGeoJson))
				]
				+ SWrapBox::Slot()
				[
					MakeActionButton(
						LOCTEXT("ExportCsv", "Export CSV"),
						LOCTEXT("ExportCsvTip",
							"Writes one row per region for bulk naming in a spreadsheet. Importing it "
							"back writes names and types only, and never touches geometry."),
						FOnClicked::CreateSP(this, &SPlaceLabelsPanel::OnExportCsv))
				]
				+ SWrapBox::Slot()
				[
					MakeActionButton(
						LOCTEXT("OpenTab", "Open in a Tab"),
						LOCTEXT("OpenTabTip",
							"Opens this panel as a dockable tab, which can be docked to the right, along "
							"the bottom, or floated onto a second monitor. It stays open whether or not "
							"Place Labels mode is active. Also under Window > Place Labels."),
						FOnClicked::CreateLambda([]()
						{
							PlaceLabelsEditor::OpenPanelTab();
							return FReply::Handled();
						}))
				]
				+ SWrapBox::Slot()
				[
					MakeActionButton(
						LOCTEXT("Import", "Import..."),
						LOCTEXT("ImportTip",
							"Reads a GeoJSON or CSV file back. Rows and features are matched to regions "
							"by their id, so re-importing an edited export updates rather than duplicates."),
						FOnClicked::CreateSP(this, &SPlaceLabelsPanel::OnImport))
				]
			]

			// ---- The other plugin's plans, which are drawn on the same ground these regions are.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 2.0f, 8.0f, 0.0f))
			[
				SNew(SCheckBox)
				.Visibility_Lambda([]()
				{
					return FindHutongPlansVar() ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.IsChecked_Lambda([]()
				{
					IConsoleVariable* Var = FindHutongPlansVar();
					return Var && Var->GetBool() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([](ECheckBoxState NewState)
				{
					if (IConsoleVariable* Var = FindHutongPlansVar())
					{
						Var->Set(NewState == ECheckBoxState::Checked, ECVF_SetByCode);
					}
				})
				.ToolTipText(LOCTEXT("ShowHutongPlansTip",
					"Draws HutongLayout's laid-out buildings — the coloured footprint polygons. Off "
					"hides all of them, stops them taking clicks and withdraws the offer to trace a "
					"building's footprint, which is what to do while tracing regions over a street "
					"that has been laid out. Nothing placed is changed, and the same switch is in the "
					"Hutong Layout mode panel."))
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.Text(LOCTEXT("ShowHutongPlans", "Show HutongLayout plan outlines"))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 2.0f, 8.0f, 4.0f))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.Text(LOCTEXT("WeldToleranceLabel", "Weld tolerance"))
					.ToolTipText(LOCTEXT("WeldToleranceTip",
						"How far apart two boundaries may be and still count as meant to be the same "
						"boundary. Used both to report near-misses and to weld them. Keep it well below "
						"the smallest gap you would ever draw on purpose."))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(80.0f)
					[
						SNew(SSpinBox<double>)
						.MinValue(0.1)
						.MaxValue(1000.0)
						.MinSliderValue(1.0)
						.MaxSliderValue(200.0)
						.Delta(1.0)
						.Value_Lambda([this]() { return WeldTolerance; })
						.OnValueChanged_Lambda([this](double NewValue)
						{
							WeldTolerance = NewValue;
						})
						.OnValueCommitted_Lambda([this](double NewValue, ETextCommit::Type)
						{
							WeldTolerance = NewValue;
							RebuildProblemRows();
						})
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.Text(LOCTEXT("Centimetres", "cm"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		]
	];

	RegisterActiveTimer(RefreshIntervalSeconds,
		FWidgetActiveTimerDelegate::CreateSP(this, &SPlaceLabelsPanel::OnRefreshTimer));

	BuildConfidenceOptions();
	RebuildTypeOptions();
	RefreshNow();
}

TSharedRef<SWidget> SPlaceLabelsPanel::BuildRegionEditor()
{
	// One row of the editor: a label of fixed width so the three name fields line up, and the widget beside it.
	auto MakeField = [](const FText& Label, TSharedRef<SWidget> Field)
	{
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(52.0f)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text(Label)
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				Field
			];
	};

	auto MakeNameBox = [this](ENameField Field, const FText& Hint)
	{
		return SNew(SEditableTextBox)
			.HintText(Hint)
			// Bound rather than set once, so selecting a different region in the list refills the form.
			.Text(this, &SPlaceLabelsPanel::GetEditorName, Field)
			.OnTextCommitted(this, &SPlaceLabelsPanel::CommitEditorName, Field);
	};

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(8.0f, 6.0f))
		.Visibility(this, &SPlaceLabelsPanel::GetEditorVisibility)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle("SmallFontBold"))
				.Text(this, &SPlaceLabelsPanel::GetEditorHeaderText)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
			[
				MakeField(LOCTEXT("FieldType", "Type"),
					SAssignNew(EditorTypeCombo, SComboBox<TSharedPtr<FPlaceTypeOption>>)
					.OptionsSource(&TypeOptions)
					.OnGenerateWidget(this, &SPlaceLabelsPanel::MakeTypeOptionEntry)
					.OnSelectionChanged(this, &SPlaceLabelsPanel::OnEditorTypeChanged)
					.ToolTipText(LOCTEXT("FieldTypeTip",
						"District, hutong, compound... Sets the outline colour, which region wins "
						"when several overlap, and how this one finds its parent."))
					[
						SNew(STextBlock).Text(this, &SPlaceLabelsPanel::GetEditorTypeLabel)
					])
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
			[
				// 大柵欄, not 大柵欄胡同 — the type supplies the 胡同.
				MakeField(LOCTEXT("FieldChinese", "中文"),
					MakeNameBox(ENameField::Chinese, LOCTEXT("HintChinese", "the name as written")))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
			[
				MakeField(LOCTEXT("FieldPinyin", "Pinyin"),
					MakeNameBox(ENameField::Pinyin, LOCTEXT("HintPinyin", "romanisation")))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
			[
				MakeField(LOCTEXT("FieldEnglish", "English"),
					MakeNameBox(ENameField::English, LOCTEXT("HintEnglish", "optional gloss")))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
			[
				MakeField(LOCTEXT("FieldNote", "Note"),
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("HintNote", "anything the readout should show"))
					.Text(this, &SPlaceLabelsPanel::GetEditorNote)
					.OnTextCommitted(this, &SPlaceLabelsPanel::CommitEditorNote))
			]

			// Metadata: the half of the answer the outline cannot give.
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
			[
				MakeField(LOCTEXT("FieldSource", "Source"),
					SNew(SEditableTextBox)
					.HintText(LOCTEXT("HintSource", "where this name was read"))
					.ToolTipText(LOCTEXT("FieldSourceTip",
						"Where this region's name and extent came from: a sheet of the 乾隆京城全圖, "
						"a gazetteer, a street sign, somebody who lives there. Free text, so a page "
						"or plate number belongs here too."))
					.Text(this, &SPlaceLabelsPanel::GetEditorSource)
					.OnTextCommitted(this, &SPlaceLabelsPanel::CommitEditorSource))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f)
			[
				MakeField(LOCTEXT("FieldConfidence", "Evidence"),
					SAssignNew(EditorConfidenceCombo, SComboBox<TSharedPtr<FPlaceConfidenceOption>>)
					.OptionsSource(&ConfidenceOptions)
					.OnGenerateWidget(this, &SPlaceLabelsPanel::MakeConfidenceEntry)
					.OnSelectionChanged(this, &SPlaceLabelsPanel::OnEditorConfidenceChanged)
					.ToolTipText(LOCTEXT("FieldConfidenceTip",
						"How far the source actually says this: 5 is named in a source with this "
						"extent, 1 is a name put here to fill a gap. The numbers are the scale, so "
						"they mean the same thing here as in the Hutong Layout plugin."))
					[
						SNew(STextBlock).Text(this, &SPlaceLabelsPanel::GetEditorConfidenceLabel)
					])
			]

			// The region about to be placed: the same form, and the buttons that finish it.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SWrapBox)
				.UseAllottedSize(true)
				.Visibility(this, &SPlaceLabelsPanel::GetPendingButtonsVisibility)

				+ SWrapBox::Slot().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CreateRegion", "Create Region"))
					.ToolTipText(LOCTEXT("CreateRegionTip",
						"Places the outline you have drawn, with what is typed above. The name is "
						"cleared afterwards and the type, source and evidence are kept, since those "
						"belong to the run of polygons being traced rather than to one of them."))
					.OnClicked(this, &SPlaceLabelsPanel::OnCreatePendingRegion)
				]
				+ SWrapBox::Slot()
				[
					SNew(SButton)
					.Text(LOCTEXT("DiscardOutline", "Discard Outline"))
					.ToolTipText(LOCTEXT("DiscardOutlineTip",
						"Throws the outline away without placing anything. Escape does the same."))
					.OnClicked(this, &SPlaceLabelsPanel::OnDiscardPendingOutline)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(SWrapBox)
				.UseAllottedSize(true)
				.Visibility(this, &SPlaceLabelsPanel::GetPlacedButtonsVisibility)

				+ SWrapBox::Slot().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("FrameRegion", "Frame"))
					.ToolTipText(LOCTEXT("FrameRegionTip", "Fly the viewport to this region."))
					.OnClicked(this, &SPlaceLabelsPanel::OnFrameSelected)
				]
				+ SWrapBox::Slot().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("RecomputeParent", "Recompute Parent"))
					.ToolTipText(LOCTEXT("RecomputeParentTip",
						"Re-resolve just this region's parent. Use Recompute Hierarchy above after "
						"a batch of edits."))
					.OnClicked(this, &SPlaceLabelsPanel::OnRecomputeSelectedParent)
				]
				+ SWrapBox::Slot()
				[
					SNew(SButton)
					.Text(LOCTEXT("DeleteRegion", "Delete Region"))
					.ToolTipText(LOCTEXT("DeleteRegionTip",
						"Removes this region from the level. Undoable."))
					.OnClicked(this, &SPlaceLabelsPanel::OnDeleteSelected)
				]
			]
		];
}

EVisibility SPlaceLabelsPanel::GetEditorVisibility() const
{
	// Collapsed, not hidden: an empty form taking up a third of the panel while nothing is selected is worse than no form.
	return (SelectedRegion.IsValid() || IsAuthoringNewRegion())
		? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SPlaceLabelsPanel::GetPendingButtonsVisibility() const
{
	return IsAuthoringNewRegion() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SPlaceLabelsPanel::GetPlacedButtonsVisibility() const
{
	return (!IsAuthoringNewRegion() && SelectedRegion.IsValid())
		? EVisibility::Visible : EVisibility::Collapsed;
}

UPlaceRegionPenTool* SPlaceLabelsPanel::GetAuthoringPen() const
{
	UPlaceRegionPenTool* Pen = Cast<UPlaceRegionPenTool>(GetActivePlaceLabelsTool());
	return (Pen && Pen->IsActive() && Pen->GetPendingSettings()) ? Pen : nullptr;
}

UPlaceRegionPenToolProperties* SPlaceLabelsPanel::GetAuthoringSettings() const
{
	UPlaceRegionPenTool* Pen = GetAuthoringPen();
	return Pen ? Pen->GetPendingSettings() : nullptr;
}

FReply SPlaceLabelsPanel::OnCreatePendingRegion()
{
	if (UPlaceRegionPenTool* Pen = GetAuthoringPen())
	{
		Pen->ConfirmRegion();
		RefreshNow();
	}
	return FReply::Handled();
}

FReply SPlaceLabelsPanel::OnDiscardPendingOutline()
{
	if (UPlaceRegionPenTool* Pen = GetAuthoringPen())
	{
		Pen->CancelDrawing();
	}
	return FReply::Handled();
}

FText SPlaceLabelsPanel::GetEditorHeaderText() const
{
	if (const UPlaceRegionPenTool* Pen = GetAuthoringPen())
	{
		return Pen->IsAwaitingConfirm()
			? LOCTEXT("EditorHeaderPending", "New region — outline drawn, not placed yet")
			: LOCTEXT("EditorHeaderDrawing", "New region — still being drawn");
	}

	const UPlaceRegionComponent* Region = SelectedRegion.Get();
	if (!Region)
	{
		return FText::GetEmpty();
	}
	return FText::Format(LOCTEXT("EditorHeader", "Editing: {0}"),
		Region->Name.IsEmpty()
			? FText::FromString(Region->GetOwner() ? Region->GetOwner()->GetActorLabel()
												   : Region->GetName())
			: Region->Name.GetDisplayText());
}

void SPlaceLabelsPanel::SetSelectedRegion(UPlaceRegionComponent* Region)
{
	if (SelectedRegion.Get() == Region)
	{
		return;
	}
	SelectedRegion = Region;

	// Only on a selection change — this loads every type asset in the project.
	RebuildTypeOptions();
	SyncConfidenceSelection();
}

void SPlaceLabelsPanel::RebuildTypeOptions()
{
	TMap<FName, UPlaceLabelTypeAsset*> Types;
	PlaceLabelsExchange::GatherTypeAssets(Types);

	TypeOptions.Reset(Types.Num() + 1);

	TSharedRef<FPlaceTypeOption> NoType = MakeShared<FPlaceTypeOption>();
	NoType->Label = TEXT("(no type)");
	TypeOptions.Add(NoType);

	TArray<FName> Ids;
	Types.GetKeys(Ids);
	Ids.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });

	const UPlaceRegionPenToolProperties* Pending = GetAuthoringSettings();
	const UPlaceRegionComponent* Region = SelectedRegion.Get();
	const UPlaceLabelTypeAsset* Current = Pending ? Pending->Type.Get()
		: (Region ? Region->Type.Get() : nullptr);
	SelectedTypeOption = NoType;

	for (const FName& Id : Ids)
	{
		TSharedRef<FPlaceTypeOption> Option = MakeShared<FPlaceTypeOption>();
		Option->Type = Types[Id];
		Option->Label = Id.ToString();
		TypeOptions.Add(Option);

		if (Current && Current == Types[Id])
		{
			SelectedTypeOption = Option;
		}
	}

	if (EditorTypeCombo.IsValid())
	{
		EditorTypeCombo->RefreshOptions();
		EditorTypeCombo->SetSelectedItem(SelectedTypeOption);
	}
}

TSharedRef<SWidget> SPlaceLabelsPanel::MakeTypeOptionEntry(TSharedPtr<FPlaceTypeOption> Item)
{
	if (!Item.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	// A swatch beside each entry.
	const UPlaceLabelTypeAsset* Type = Item->Type.Get();
	const FLinearColor Swatch = Type ? Type->EditorOutlineColor : FLinearColor(1.0f, 0.55f, 0.1f);

	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 6.0f, 0.0f)
		[
			SNew(SBox).WidthOverride(10.0f).HeightOverride(10.0f)
			[
				SNew(SImage).Image(FAppStyle::GetBrush("WhiteBrush")).ColorAndOpacity(Swatch)
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString(Item->Label))
		];
}

FText SPlaceLabelsPanel::GetEditorTypeLabel() const
{
	if (SelectedTypeOption.IsValid())
	{
		return FText::FromString(SelectedTypeOption->Label);
	}
	return LOCTEXT("NoTypeOption", "(no type)");
}

void SPlaceLabelsPanel::OnEditorTypeChanged(TSharedPtr<FPlaceTypeOption> NewValue,
	ESelectInfo::Type SelectInfo)
{
	// Direct is the programmatic SetSelectedItem in RebuildTypeOptions, which is loading the form, not editing it.
	if (SelectInfo == ESelectInfo::Direct || !NewValue.IsValid())
	{
		return;
	}

	SelectedTypeOption = NewValue;

	if (UPlaceRegionPenToolProperties* Pending = GetAuthoringSettings())
	{
		Pending->Type = NewValue->Type.Get();
		GetAuthoringPen()->NotifyPendingSettingsChanged();
		return;
	}

	UPlaceRegionComponent* Region = SelectedRegion.Get();
	if (!Region)
	{
		return;
	}

	UPlaceLabelTypeAsset* NewType = NewValue->Type.Get();
	if (Region->Type == NewType)
	{
		return;
	}

	{
		const FScopedTransaction Transaction(LOCTEXT("SetRegionType", "Set Region Type"));
		Region->Modify();
		Region->Type = NewType;

		if (AActor* Owner = Region->GetOwner())
		{
			Owner->Modify();
			Owner->SetActorLabel(Owner->GetDefaultActorLabel());
		}

		// The type decides the parenting rule as well as the colour.
		if (Region->bAutoParent && !Region->ExplicitParent)
		{
			Region->RecomputeDerivedParent();
		}
	}

	// The outline colour is a function of the type, and it just changed.
	Region->MarkRenderStateDirty();
	GEditor->RedrawLevelEditingViewports(true);

	RefreshNow();
}

// One accessor for the three name fields, so the region about to be placed and the one already
// placed are the same three lines rather than two forms that can disagree.
FText& SPlaceLabelsPanel::NameFieldOf(FPlaceName& Name, ENameField Field)
{
	return Field == ENameField::Chinese ? Name.Chinese
		: Field == ENameField::Pinyin ? Name.Pinyin
		: Name.English;
}

FText SPlaceLabelsPanel::GetEditorName(ENameField Field) const
{
	if (UPlaceRegionPenToolProperties* Pending = GetAuthoringSettings())
	{
		return NameFieldOf(Pending->Name, Field);
	}

	const UPlaceRegionComponent* Region = SelectedRegion.Get();
	if (!Region)
	{
		return FText::GetEmpty();
	}
	switch (Field)
	{
	case ENameField::Chinese: return Region->Name.Chinese;
	case ENameField::Pinyin:  return Region->Name.Pinyin;
	case ENameField::English: return Region->Name.English;
	}
	return FText::GetEmpty();
}

void SPlaceLabelsPanel::CommitEditorName(const FText& NewText, ETextCommit::Type CommitType,
	ENameField Field)
{
	// Nothing is placed yet, so there is nothing to transact: the name is held on the tool until
	// Create Region, and that is what carries it onto the region it makes.
	if (UPlaceRegionPenToolProperties* Pending = GetAuthoringSettings())
	{
		FText& PendingTarget = NameFieldOf(Pending->Name, Field);
		if (!PendingTarget.EqualTo(NewText))
		{
			PendingTarget = NewText;
			GetAuthoringPen()->NotifyPendingSettingsChanged();
		}
		return;
	}

	UPlaceRegionComponent* Region = SelectedRegion.Get();
	if (!Region)
	{
		return;
	}

	FText& Target = Field == ENameField::Chinese ? Region->Name.Chinese
		: Field == ENameField::Pinyin ? Region->Name.Pinyin
		: Region->Name.English;

	if (Target.EqualTo(NewText))
	{
		return;
	}

	{
		const FScopedTransaction Transaction(LOCTEXT("SetRegionName", "Set Region Name"));
		Region->Modify();
		Target = NewText;

		// The actor label follows the name, so the outliner and the browser agree with the field that was just typed into.
		if (AActor* Owner = Region->GetOwner())
		{
			Owner->Modify();
			Owner->SetActorLabel(Owner->GetDefaultActorLabel());
		}
	}

	RefreshNow();
}

FText SPlaceLabelsPanel::GetEditorNote() const
{
	// A pending region has no note field of its own: the tool carries the name, type and metadata,
	// and a note about a place is written once the place exists.
	if (IsAuthoringNewRegion())
	{
		return FText::GetEmpty();
	}
	const UPlaceRegionComponent* Region = SelectedRegion.Get();
	return Region ? Region->Note : FText::GetEmpty();
}

void SPlaceLabelsPanel::CommitEditorNote(const FText& NewText, ETextCommit::Type CommitType)
{
	UPlaceRegionComponent* Region = IsAuthoringNewRegion() ? nullptr : SelectedRegion.Get();
	if (!Region || Region->Note.EqualTo(NewText))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetRegionNote", "Set Region Note"));
	Region->Modify();
	Region->Note = NewText;
}

FText SPlaceLabelsPanel::GetEditorSource() const
{
	if (const UPlaceRegionPenToolProperties* Pending = GetAuthoringSettings())
	{
		return Pending->Source;
	}
	const UPlaceRegionComponent* Region = SelectedRegion.Get();
	return Region ? Region->Source : FText::GetEmpty();
}

void SPlaceLabelsPanel::CommitEditorSource(const FText& NewText, ETextCommit::Type CommitType)
{
	if (UPlaceRegionPenToolProperties* Pending = GetAuthoringSettings())
	{
		if (!Pending->Source.EqualTo(NewText))
		{
			Pending->Source = NewText;
			GetAuthoringPen()->NotifyPendingSettingsChanged();
		}
		return;
	}

	UPlaceRegionComponent* Region = SelectedRegion.Get();
	if (!Region || Region->Source.EqualTo(NewText))
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetRegionSource", "Set Region Source"));
	Region->Modify();
	Region->Source = NewText;
}

void SPlaceLabelsPanel::BuildConfidenceOptions()
{
	ConfidenceOptions.Reset();

	const UEnum* Enum = StaticEnum<EPlaceConfidence>();
	if (!Enum)
	{
		return;
	}

	// Highest first: the ordinary answer is at the top of the list, where it can be picked without reading.
	for (int32 i = Enum->NumEnums() - 1; i >= 0; --i)
	{
		const int64 Value = Enum->GetValueByIndex(i);
		if (Value == static_cast<int64>(EPlaceConfidence::Unknown)
			|| Enum->HasMetaData(TEXT("Hidden"), i)
			|| Enum->GetNameStringByIndex(i).EndsWith(TEXT("_MAX")))
		{
			continue;
		}

		TSharedRef<FPlaceConfidenceOption> Option = MakeShared<FPlaceConfidenceOption>();
		Option->Value = static_cast<EPlaceConfidence>(Value);
		Option->Label = Enum->GetDisplayNameTextByIndex(i);
		ConfidenceOptions.Add(Option);
	}

	SyncConfidenceSelection();
}

void SPlaceLabelsPanel::SyncConfidenceSelection()
{
	EPlaceConfidence Current = EPlaceConfidence::Attested;
	if (const UPlaceRegionPenToolProperties* Pending = GetAuthoringSettings())
	{
		Current = Pending->Confidence;
	}
	else if (const UPlaceRegionComponent* Region = SelectedRegion.Get())
	{
		Current = Region->Confidence;
	}

	for (const TSharedPtr<FPlaceConfidenceOption>& Option : ConfidenceOptions)
	{
		if (Option.IsValid() && Option->Value == Current)
		{
			SelectedConfidenceOption = Option;
			break;
		}
	}

	if (EditorConfidenceCombo.IsValid())
	{
		EditorConfidenceCombo->SetSelectedItem(SelectedConfidenceOption);
	}
}

TSharedRef<SWidget> SPlaceLabelsPanel::MakeConfidenceEntry(TSharedPtr<FPlaceConfidenceOption> Item)
{
	return Item.IsValid()
		? StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(Item->Label))
		: SNullWidget::NullWidget;
}

FText SPlaceLabelsPanel::GetEditorConfidenceLabel() const
{
	return SelectedConfidenceOption.IsValid() ? SelectedConfidenceOption->Label : FText::GetEmpty();
}

void SPlaceLabelsPanel::OnEditorConfidenceChanged(TSharedPtr<FPlaceConfidenceOption> NewValue,
	ESelectInfo::Type SelectInfo)
{
	// Direct is SetSelectedItem loading the form, not somebody choosing.
	if (SelectInfo == ESelectInfo::Direct || !NewValue.IsValid())
	{
		return;
	}

	SelectedConfidenceOption = NewValue;

	if (UPlaceRegionPenToolProperties* Pending = GetAuthoringSettings())
	{
		Pending->Confidence = NewValue->Value;
		GetAuthoringPen()->NotifyPendingSettingsChanged();
		return;
	}

	UPlaceRegionComponent* Region = SelectedRegion.Get();
	if (!Region || Region->Confidence == NewValue->Value)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("SetRegionConfidence", "Set Region Evidence"));
	Region->Modify();
	Region->Confidence = NewValue->Value;
}

FReply SPlaceLabelsPanel::OnFrameSelected()
{
	SelectRegion(SelectedRegion.Get(), /*bFocusViewport*/ true);
	return FReply::Handled();
}

FReply SPlaceLabelsPanel::OnRecomputeSelectedParent()
{
	UPlaceRegionComponent* Region = SelectedRegion.Get();
	if (!Region)
	{
		return FReply::Handled();
	}

	{
		const FScopedTransaction Transaction(LOCTEXT("RecomputeOneParent", "Recompute Region Parent"));
		Region->RecomputeDerivedParent();
	}

	RefreshNow();
	return FReply::Handled();
}

FReply SPlaceLabelsPanel::OnDeleteSelected()
{
	UPlaceRegionComponent* Region = SelectedRegion.Get();
	AActor* Actor = Region ? Region->GetOwner() : nullptr;
	if (!Actor || !GEditor)
	{
		return FReply::Handled();
	}

	{
		const FScopedTransaction Transaction(LOCTEXT("DeleteRegionTx", "Delete Place Region"));
		if (UWorld* World = GetEditorWorld())
		{
			World->Modify();
		}
		Actor->Modify();
		Actor->Destroy();
	}

	SelectedRegion = nullptr;
	RefreshNow();
	GEditor->RedrawLevelEditingViewports(true);
	return FReply::Handled();
}

UWorld* SPlaceLabelsPanel::GetEditorWorld()
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

FText SPlaceLabelsPanel::GetPromptText() const
{
	UInteractiveTool* Tool = GetActivePlaceLabelsTool();
	if (const UPlaceRegionPenTool* Pen = Cast<UPlaceRegionPenTool>(Tool))
	{
		return Pen->GetStagePromptText();
	}
	if (const UPlaceRegionEditTool* Edit = Cast<UPlaceRegionEditTool>(Tool))
	{
		return Edit->GetStagePromptText();
	}
	if (const UPlaceRegionSelectTool* Select = Cast<UPlaceRegionSelectTool>(Tool))
	{
		return Select->GetStagePromptText();
	}
	if (!GLevelEditorModeTools().IsModeActive(UPlaceLabelsEdMode::EM_PlaceLabelsModeId))
	{
		return LOCTEXT("ModeNotActive",
			"Place Labels mode is not active — the list below still works, but drawing and editing "
			"need the mode switched on.");
	}
	return LOCTEXT("NoToolPrompt",
		"Select picks regions with the cursor. Pen draws a new one. Edit reshapes one that is "
		"already there.");
}

FSlateColor SPlaceLabelsPanel::GetPromptColour() const
{
	// Warm yellow only while something is actually in hand.
	UInteractiveTool* Tool = GetActivePlaceLabelsTool();
	const UPlaceRegionPenTool* Pen = Cast<UPlaceRegionPenTool>(Tool);
	const UPlaceRegionEditTool* Edit = Cast<UPlaceRegionEditTool>(Tool);

	const bool bBusy = (Pen && Pen->IsDrawing()) || (Edit && Edit->HasTarget());
	return bBusy ? FSlateColor(FLinearColor(1.0f, 0.9f, 0.15f)) : FSlateColor::UseForeground();
}

EActiveTimerReturnType SPlaceLabelsPanel::OnRefreshTimer(double, float)
{
	RebuildRegionRows();
	if (bProblemsExpanded)
	{
		RebuildProblemRows();
	}
	SyncSelectionFromEditor();

	// The dropdowns are loaded once per change of subject rather than every tick, so the moment
	// the form switches between the pending region and the selected one is the moment to reload them.
	const bool bAuthoring = IsAuthoringNewRegion();
	if (bAuthoring != bWasAuthoring)
	{
		bWasAuthoring = bAuthoring;
		RebuildTypeOptions();
		SyncConfidenceSelection();
	}

	return EActiveTimerReturnType::Continue;
}

void SPlaceLabelsPanel::SyncSelectionFromEditor()
{
	// Clicking a region in the viewport should fill in the form, not just highlight an outline.
	if (!GEditor)
	{
		return;
	}

	// Never while the form has focus.
	if (HasFocusedDescendants())
	{
		return;
	}

	// And never while the pen has an outline in hand: the form belongs to the region being drawn,
	// and the region that would be adopted here is the one just placed.
	if (IsAuthoringNewRegion())
	{
		return;
	}

	// The Edit tool takes clicks itself, so a region picked up there never reaches the level
	// editor's selection. It is still the region being worked on, and the form should be on it.
	if (const UPlaceRegionEditTool* Edit = Cast<UPlaceRegionEditTool>(GetActivePlaceLabelsTool()))
	{
		if (UPlaceRegionComponent* Target = Edit->GetTargetRegion())
		{
			if (Target != SelectedRegion.Get())
			{
				SetSelectedRegion(Target);

				if (RegionListView.IsValid())
				{
					if (TSharedPtr<FPlaceRegionRow>* Row = RowByRegion.Find(Target))
					{
						RegionListView->SetSelection(*Row, ESelectInfo::Direct);
						RegionListView->RequestScrollIntoView(*Row);
					}
				}
			}
			return;
		}
	}

	UPlaceRegionComponent* Only = nullptr;
	int32 Count = 0;

	if (USelection* Selection = GEditor->GetSelectedActors())
	{
		for (FSelectionIterator It(*Selection); It; ++It)
		{
			const AActor* Actor = Cast<AActor>(*It);
			if (UPlaceRegionComponent* Region =
					Actor ? Actor->FindComponentByClass<UPlaceRegionComponent>() : nullptr)
			{
				Only = Region;
				++Count;
			}
		}
	}

	// Exactly one, and only to *change* the selection — never to clear it.
	if (Count == 1 && Only != SelectedRegion.Get())
	{
		SetSelectedRegion(Only);

		if (RegionListView.IsValid())
		{
			if (TSharedPtr<FPlaceRegionRow>* Row = RowByRegion.Find(Only))
			{
				RegionListView->SetSelection(*Row, ESelectInfo::Direct);
				RegionListView->RequestScrollIntoView(*Row);
			}
		}
	}
}

void SPlaceLabelsPanel::RefreshNow()
{
	RebuildRegionRows();
	RebuildProblemRows();
}

void SPlaceLabelsPanel::RebuildRegionRows()
{
	TArray<TWeakObjectPtr<UPlaceRegionComponent>> Regions;
	PlaceLabelsEdit::GatherRegions(GetEditorWorld(), Regions);

	AllRegionRows.Reset(Regions.Num());

	TSet<FString> TypesPresent;

	// Rows are reused across refreshes, keyed by the component.
	TMap<TWeakObjectPtr<UPlaceRegionComponent>, TSharedPtr<FPlaceRegionRow>> NextRowByRegion;
	NextRowByRegion.Reserve(Regions.Num());

	for (const TWeakObjectPtr<UPlaceRegionComponent>& Weak : Regions)
	{
		UPlaceRegionComponent* Region = Weak.Get();
		if (!Region)
		{
			continue;
		}

		TSharedPtr<FPlaceRegionRow>* Existing = RowByRegion.Find(Weak);
		TSharedRef<FPlaceRegionRow> Row = Existing && Existing->IsValid()
			? Existing->ToSharedRef()
			: MakeShared<FPlaceRegionRow>();

		Row->Region = Weak;
		Row->bHasSevereIssue = false;

		// An unnamed region still has to be findable, and "" is not a row anyone can click on.
		Row->DisplayName = Region->Name.IsEmpty()
			? FText::Format(LOCTEXT("UnnamedRegion", "(unnamed) {0}"),
				FText::FromString(Region->GetOwner() ? Region->GetOwner()->GetActorLabel()
													 : Region->GetName()))
			: Region->Name.GetDisplayText();

		Row->TypeId = Region->GetTypeId();
		Row->TypeLabel = Region->Type
			? (Region->Type->TypeLabel.IsEmpty()
				? FText::FromName(Region->Type->TypeId)
				: Region->Type->TypeLabel.GetDisplayText())
			: LOCTEXT("NoTypeCell", "—");
		Row->Colour = Region->GetOutlineColor();
		Row->Corners = Region->LocalPoints.Num();
		Row->AreaSquareMetres = Region->GetWorldArea() / 10000.0;

		Row->IssueCount = Report.IssueCountFor(Region);
		Row->SearchKey = (Row->DisplayName.ToString() + TEXT(" ")
			+ Row->TypeLabel.ToString() + TEXT(" ") + Row->TypeId.ToString()).ToLower();

		if (!Row->TypeId.IsNone())
		{
			TypesPresent.Add(Row->TypeId.ToString());
		}

		NextRowByRegion.Add(Weak, Row);
		AllRegionRows.Add(Row);
	}

	// Rows for regions that have gone are dropped here.
	RowByRegion = MoveTemp(NextRowByRegion);

	for (const PlaceLabelsValidation::FRegionIssue& Issue : Report.Issues)
	{
		if (!PlaceLabelsValidation::IsSevere(Issue.Issue))
		{
			continue;
		}
		for (TSharedPtr<FPlaceRegionRow>& Row : AllRegionRows)
		{
			if (Row->Region == Issue.Region)
			{
				Row->bHasSevereIssue = true;
				break;
			}
		}
	}

	// Coarse types first, then alphabetical.
	AllRegionRows.Sort([](const TSharedPtr<FPlaceRegionRow>& A, const TSharedPtr<FPlaceRegionRow>& B)
	{
		const UPlaceRegionComponent* RA = A->Region.Get();
		const UPlaceRegionComponent* RB = B->Region.Get();
		const int32 PA = RA ? RA->GetDisplayPriority() : 0;
		const int32 PB = RB ? RB->GetDisplayPriority() : 0;
		if (PA != PB)
		{
			return PA < PB;
		}
		return A->DisplayName.CompareTo(B->DisplayName) < 0;
	});

	// Rebuild the filter list only when the set of types actually changed, or the combo would drop its selection every second.
	TArray<FString> SortedTypes = TypesPresent.Array();
	SortedTypes.Sort();

	bool bFilterListChanged = (TypeFilterOptions.Num() != SortedTypes.Num() + 1);
	if (!bFilterListChanged)
	{
		for (int32 i = 0; i < SortedTypes.Num(); ++i)
		{
			if (*TypeFilterOptions[i + 1] != SortedTypes[i])
			{
				bFilterListChanged = true;
				break;
			}
		}
	}

	if (bFilterListChanged)
	{
		const FString Previous = SelectedTypeFilter.IsValid() ? *SelectedTypeFilter : AllTypesLabel;

		TypeFilterOptions.Reset();
		TypeFilterOptions.Add(MakeShared<FString>(AllTypesLabel));
		for (const FString& Type : SortedTypes)
		{
			TypeFilterOptions.Add(MakeShared<FString>(Type));
		}

		SelectedTypeFilter = TypeFilterOptions[0];
		for (const TSharedPtr<FString>& Option : TypeFilterOptions)
		{
			if (*Option == Previous)
			{
				SelectedTypeFilter = Option;
				break;
			}
		}

		if (TypeFilterCombo.IsValid())
		{
			TypeFilterCombo->RefreshOptions();
			TypeFilterCombo->SetSelectedItem(SelectedTypeFilter);
		}
	}

	ApplyFilter();
	UpdateSummary();
}

void SPlaceLabelsPanel::ApplyFilter()
{
	const FString TypeFilter = SelectedTypeFilter.IsValid() ? *SelectedTypeFilter : AllTypesLabel;
	const bool bFilterByType = (TypeFilter != AllTypesLabel);

	TArray<TSharedPtr<FPlaceRegionRow>> NextVisible;
	NextVisible.Reserve(AllRegionRows.Num());

	for (const TSharedPtr<FPlaceRegionRow>& Row : AllRegionRows)
	{
		if (bProblemsOnly && Row->IssueCount == 0)
		{
			continue;
		}
		if (bFilterByType && Row->TypeId.ToString() != TypeFilter)
		{
			continue;
		}
		if (!SearchFilter.IsEmpty() && !Row->SearchKey.Contains(SearchFilter))
		{
			continue;
		}
		NextVisible.Add(Row);
	}

	// Pointer-wise comparison, which is exactly the identity the list view itself uses.
	if (NextVisible == VisibleRegionRows)
	{
		return;
	}

	VisibleRegionRows = MoveTemp(NextVisible);

	if (RegionListView.IsValid())
	{
		RegionListView->RequestListRefresh();
	}
}

void SPlaceLabelsPanel::RebuildProblemRows()
{
	PlaceLabelsValidation::ValidateWorld(GetEditorWorld(), WeldTolerance, Report);

	// Built into a fresh array and only swapped in if it differs, for the same reason the region list is.
	TArray<TSharedPtr<FPlaceLabelsProblemRow>> NextProblemRows;

	// Aliased so the body below reads the same as before the change.
	TArray<TSharedPtr<FPlaceLabelsProblemRow>>& ProblemRowsBuilder = NextProblemRows;

	for (const PlaceLabelsValidation::FRegionIssue& Issue : Report.Issues)
	{
		const UPlaceRegionComponent* Region = Issue.Region.Get();
		if (!Region)
		{
			continue;
		}

		const FText Label = Region->Name.IsEmpty()
			? FText::FromString(Region->GetOwner() ? Region->GetOwner()->GetActorLabel()
												   : Region->GetName())
			: Region->Name.GetDisplayText();

		TSharedRef<FPlaceLabelsProblemRow> Row = MakeShared<FPlaceLabelsProblemRow>();
		Row->Region = Issue.Region;
		Row->bSevere = PlaceLabelsValidation::IsSevere(Issue.Issue);
		Row->Text = Issue.Detail.IsEmpty()
			? FText::Format(LOCTEXT("ProblemLine", "{0} — {1}"), Label,
				PlaceLabelsValidation::DescribeIssue(Issue.Issue))
			: FText::Format(LOCTEXT("ProblemLineDetail", "{0} — {1}: {2}"), Label,
				PlaceLabelsValidation::DescribeIssue(Issue.Issue), Issue.Detail);

		ProblemRowsBuilder.Add(Row);
	}

	for (const PlaceLabelsTopology::FSeamIssue& Seam : Report.Seams)
	{
		const UPlaceRegionComponent* A = Seam.A.Get();
		const UPlaceRegionComponent* B = Seam.B.Get();
		if (!A || !B)
		{
			continue;
		}

		TSharedRef<FPlaceLabelsProblemRow> Row = MakeShared<FPlaceLabelsProblemRow>();
		Row->Region = Seam.A;
		Row->SecondRegion = Seam.B;
		Row->bIsSeam = true;
		Row->Text = FText::Format(
			LOCTEXT("SeamLine", "{0} / {1} — {2} of up to {3} cm in {4} place(s)"),
			A->Name.GetDisplayText(), B->Name.GetDisplayText(),
			Seam.bOverlapping ? LOCTEXT("SeamOverlap", "overlap") : LOCTEXT("SeamGap", "gap"),
			FText::AsNumber(FMath::RoundToInt(Seam.WorstOffsetCm)),
			FText::AsNumber(Seam.NearMissCount));

		ProblemRowsBuilder.Add(Row);
	}

	// Same problems as last time means the rows on screen are already right.
	bool bProblemsChanged = (NextProblemRows.Num() != ProblemRows.Num());
	if (!bProblemsChanged)
	{
		for (int32 i = 0; i < NextProblemRows.Num(); ++i)
		{
			if (!NextProblemRows[i]->Text.EqualTo(ProblemRows[i]->Text)
				|| NextProblemRows[i]->Region != ProblemRows[i]->Region)
			{
				bProblemsChanged = true;
				break;
			}
		}
	}

	if (bProblemsChanged)
	{
		ProblemRows = MoveTemp(NextProblemRows);
		if (ProblemListView.IsValid())
		{
			ProblemListView->RequestListRefresh();
		}
	}

	// The badges in the browser come out of this report, so they are only right once it has run.
	for (TSharedPtr<FPlaceRegionRow>& Row : AllRegionRows)
	{
		Row->IssueCount = Report.IssueCountFor(Row->Region.Get());
		Row->bHasSevereIssue = false;
	}
	for (const PlaceLabelsValidation::FRegionIssue& Issue : Report.Issues)
	{
		if (!PlaceLabelsValidation::IsSevere(Issue.Issue))
		{
			continue;
		}
		if (TSharedPtr<FPlaceRegionRow>* Row = RowByRegion.Find(Issue.Region))
		{
			(*Row)->bHasSevereIssue = true;
		}
	}

	if (bProblemsOnly)
	{
		ApplyFilter();
	}
}

TSharedRef<ITableRow> SPlaceLabelsPanel::MakeRegionRow(TSharedPtr<FPlaceRegionRow> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SPlaceRegionTableRow, OwnerTable).Item(Item);
}

TSharedRef<ITableRow> SPlaceLabelsPanel::MakeProblemRow(TSharedPtr<FPlaceLabelsProblemRow> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SHorizontalBox> Content = SNew(SHorizontalBox);

	Content->AddSlot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle("SmallFont"))
			.Text(Item.IsValid() ? Item->Text : FText::GetEmpty())
			.ToolTipText(Item.IsValid() ? Item->Text : FText::GetEmpty())
			.ColorAndOpacity(FSlateColor(Item.IsValid() && Item->bSevere ? SevereColour : WarnColour))
			.AutoWrapText(true)
		];

	// The repair sits on the problem.
	if (Item.IsValid() && Item->bIsSeam)
	{
		const TWeakObjectPtr<UPlaceRegionComponent> A = Item->Region;
		const TWeakObjectPtr<UPlaceRegionComponent> B = Item->SecondRegion;

		Content->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("WeldPair", "Weld"))
				.ToolTipText(LOCTEXT("WeldPairTip",
					"Make these two share exact corners along the run where they nearly meet."))
				.ContentPadding(FMargin(6.0f, 1.0f))
				.OnClicked(FOnClicked::CreateSP(this, &SPlaceLabelsPanel::OnWeldPair, A, B))
			];
	}

	return SNew(STableRow<TSharedPtr<FPlaceLabelsProblemRow>>, OwnerTable)
		.Padding(FMargin(2.0f, 1.0f))
		[
			Content
		];
}

void SPlaceLabelsPanel::SelectRegion(UPlaceRegionComponent* Region, bool bFocusViewport)
{
	AActor* Actor = Region ? Region->GetOwner() : nullptr;
	if (!Actor || !GEditor)
	{
		return;
	}

	GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDeselectBSPSurfs*/ true);
	GEditor->SelectActor(Actor, /*bInSelected*/ true, /*bNotify*/ true);

	if (bFocusViewport)
	{
		GEditor->MoveViewportCamerasToActor(*Actor, /*bActiveViewportOnly*/ false);
	}
}

void SPlaceLabelsPanel::OnRegionClicked(TSharedPtr<FPlaceRegionRow> Item)
{
	if (Item.IsValid())
	{
		SetSelectedRegion(Item->Region.Get());
		SelectRegion(Item->Region.Get(), /*bFocusViewport*/ false);
	}
}

void SPlaceLabelsPanel::OnRegionDoubleClicked(TSharedPtr<FPlaceRegionRow> Item)
{
	if (Item.IsValid())
	{
		SetSelectedRegion(Item->Region.Get());
		SelectRegion(Item->Region.Get(), /*bFocusViewport*/ true);
	}
}

void SPlaceLabelsPanel::OnProblemClicked(TSharedPtr<FPlaceLabelsProblemRow> Item)
{
	// Fly to the offending region *and* load it into the editor above.
	if (!Item.IsValid())
	{
		return;
	}

	UPlaceRegionComponent* Region = Item->Region.Get();
	SetSelectedRegion(Region);
	SelectRegion(Region, /*bFocusViewport*/ true);

	// Highlight the same region in the browser, so the two lists agree about what is in hand.
	if (RegionListView.IsValid())
	{
		if (TSharedPtr<FPlaceRegionRow>* Row = RowByRegion.Find(Region))
		{
			RegionListView->SetSelection(*Row, ESelectInfo::Direct);
			RegionListView->RequestScrollIntoView(*Row);
		}
	}
}

void SPlaceLabelsPanel::OnSearchTextChanged(const FText& NewText)
{
	SearchFilter = NewText.ToString().ToLower();
	ApplyFilter();
}

TSharedRef<SWidget> SPlaceLabelsPanel::MakeTypeFilterEntry(TSharedPtr<FString> Item)
{
	return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
}

void SPlaceLabelsPanel::OnTypeFilterChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type)
{
	if (NewValue.IsValid())
	{
		SelectedTypeFilter = NewValue;
		ApplyFilter();
	}
}

FText SPlaceLabelsPanel::GetTypeFilterLabel() const
{
	return FText::FromString(SelectedTypeFilter.IsValid() ? *SelectedTypeFilter : AllTypesLabel);
}

FText SPlaceLabelsPanel::GetSummaryText() const
{
	return CachedSummary;
}

void SPlaceLabelsPanel::UpdateSummary()
{
	const int32 Total = AllRegionRows.Num();
	const int32 Shown = VisibleRegionRows.Num();

	int32 Unnamed = 0;
	int32 Untyped = 0;
	for (const TSharedPtr<FPlaceRegionRow>& Row : AllRegionRows)
	{
		const UPlaceRegionComponent* Region = Row->Region.Get();
		if (!Region)
		{
			continue;
		}
		if (Region->Name.IsEmpty()) { ++Unnamed; }
		if (!Region->Type)          { ++Untyped; }
	}

	FString Line = (Shown == Total)
		? FString::Printf(TEXT("%d region%s"), Total, Total == 1 ? TEXT("") : TEXT("s"))
		: FString::Printf(TEXT("%d of %d regions"), Shown, Total);

	if (Unnamed > 0 || Untyped > 0)
	{
		Line += FString::Printf(TEXT("  ·  %d unnamed, %d untyped"), Unnamed, Untyped);
	}
	CachedSummary = FText::FromString(Line);
}

FText SPlaceLabelsPanel::GetProblemsHeaderText() const
{
	if (!bProblemsExpanded && ProblemRows.Num() == 0)
	{
		return LOCTEXT("ProblemsCollapsed", "Problems");
	}
	if (ProblemRows.Num() == 0)
	{
		return LOCTEXT("ProblemsNone", "Problems — none found");
	}
	return FText::Format(LOCTEXT("ProblemsCount", "Problems ({0})"),
		FText::AsNumber(ProblemRows.Num()));
}

FReply SPlaceLabelsPanel::OnRecomputeHierarchy()
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FReply::Handled();
	}

	int32 ChangedCount = 0;
	{
		const FScopedTransaction Transaction(
			LOCTEXT("RecomputeHierarchyTx", "Recompute Place Label Hierarchy"));
		ChangedCount = PlaceLabelsHierarchy::RecomputeAll(World);
	}

	Notify(FText::Format(
		LOCTEXT("HierarchyRecomputed", "Recomputed region hierarchy: {0} parent(s) changed."),
		FText::AsNumber(ChangedCount)), true);

	RefreshNow();
	return FReply::Handled();
}

FReply SPlaceLabelsPanel::OnCreateStarterTypes()
{
	TArray<FString> Created;
	TArray<FString> Skipped;
	PlaceLabelsEditorUtils::CreateStarterTypeAssets(Created, Skipped);

	Notify(Created.Num() > 0
		? FText::Format(LOCTEXT("StarterTypesCreated", "Created {0} place types in {1}."),
			FText::AsNumber(Created.Num()),
			FText::FromString(PlaceLabelsEditorUtils::StarterTypePackagePath))
		: LOCTEXT("StarterTypesAllExist",
			"The starter place types already exist; nothing was overwritten."), true);

	return FReply::Handled();
}

FReply SPlaceLabelsPanel::OnWeldAll()
{
	TArray<TWeakObjectPtr<UPlaceRegionComponent>> Weak;
	// The weld reshapes outlines, so it takes only the ones it can reshape.
	PlaceLabelsEdit::GatherRegions(GetEditorWorld(), Weak, /*bOnlyWithUsableOutline*/ true);

	TArray<UPlaceRegionComponent*> Regions;
	for (const TWeakObjectPtr<UPlaceRegionComponent>& W : Weak)
	{
		if (UPlaceRegionComponent* Region = W.Get())
		{
			Regions.Add(Region);
		}
	}

	PlaceLabelsTopology::FWeldReport WeldReport;
	{
		const FScopedTransaction Transaction(LOCTEXT("WeldAllTx", "Weld Region Boundaries"));
		PlaceLabelsTopology::WeldBoundaries(Regions, WeldTolerance, WeldReport);
	}

	FString Message = WeldReport.RegionsChanged == 0
		? FString(TEXT("Every boundary already meets exactly."))
		: FString::Printf(TEXT("Welded %d region(s): %d corner(s) moved, %d added, %d removed."),
			WeldReport.RegionsChanged, WeldReport.CornersMoved, WeldReport.CornersInserted,
			WeldReport.CornersRemoved);

	// A refusal is worth saying out loud.
	if (WeldReport.ClustersRefused > 0)
	{
		Message += FString::Printf(
			TEXT(" %d cluster(s) left alone — they spread further than the tolerance; try a smaller one."),
			WeldReport.ClustersRefused);
	}

	Notify(FText::FromString(Message), true);
	RefreshNow();
	return FReply::Handled();
}

FReply SPlaceLabelsPanel::OnWeldPair(TWeakObjectPtr<UPlaceRegionComponent> A,
	TWeakObjectPtr<UPlaceRegionComponent> B)
{
	UPlaceRegionComponent* RegionA = A.Get();
	UPlaceRegionComponent* RegionB = B.Get();
	if (!RegionA || !RegionB)
	{
		return FReply::Handled();
	}

	PlaceLabelsTopology::FWeldReport WeldReport;
	{
		const FScopedTransaction Transaction(LOCTEXT("WeldPairTx", "Weld Region Boundary"));
		PlaceLabelsTopology::WeldPair(RegionA, RegionB, WeldTolerance, WeldReport);
	}

	Notify(FText::Format(
		LOCTEXT("WeldPairDone", "{0} corner(s) moved, {1} added."),
		FText::AsNumber(WeldReport.CornersMoved), FText::AsNumber(WeldReport.CornersInserted)), true);

	RefreshNow();
	return FReply::Handled();
}

bool SPlaceLabelsPanel::PickSaveFile(const FString& Title, const FString& FileTypes,
	const FString& DefaultName, FString& OutPath)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return false;
	}

	const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	TArray<FString> Filenames;
	const bool bPicked = DesktopPlatform->SaveFileDialog(ParentWindowHandle, Title,
		FPaths::ProjectSavedDir(), DefaultName, FileTypes, EFileDialogFlags::None, Filenames);

	if (!bPicked || Filenames.Num() == 0)
	{
		return false;
	}
	OutPath = Filenames[0];
	return true;
}

bool SPlaceLabelsPanel::PickOpenFile(const FString& Title, const FString& FileTypes, FString& OutPath)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return false;
	}

	const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	TArray<FString> Filenames;
	const bool bPicked = DesktopPlatform->OpenFileDialog(ParentWindowHandle, Title,
		FPaths::ProjectSavedDir(), FString(), FileTypes, EFileDialogFlags::None, Filenames);

	if (!bPicked || Filenames.Num() == 0)
	{
		return false;
	}
	OutPath = Filenames[0];
	return true;
}

FReply SPlaceLabelsPanel::OnExportGeoJson()
{
	FString Path;
	if (!PickSaveFile(TEXT("Export regions as GeoJSON"), TEXT("GeoJSON (*.geojson)|*.geojson|JSON (*.json)|*.json"),
			TEXT("PlaceLabels.geojson"), Path))
	{
		return FReply::Handled();
	}

	PlaceLabelsExchange::FResult Result;
	PlaceLabelsExchange::ExportGeoJson(GetEditorWorld(), Path, Result);

	for (const FString& Problem : Result.Problems)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlaceLabels export: %s"), *Problem);
	}
	Notify(Result.Summarise(), Result.bSucceeded);
	return FReply::Handled();
}

FReply SPlaceLabelsPanel::OnExportCsv()
{
	FString Path;
	if (!PickSaveFile(TEXT("Export region names as CSV"), TEXT("CSV (*.csv)|*.csv"),
			TEXT("PlaceLabels.csv"), Path))
	{
		return FReply::Handled();
	}

	PlaceLabelsExchange::FResult Result;
	PlaceLabelsExchange::ExportCsv(GetEditorWorld(), Path, Result);

	for (const FString& Problem : Result.Problems)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlaceLabels export: %s"), *Problem);
	}
	Notify(Result.Summarise(), Result.bSucceeded);
	return FReply::Handled();
}

FReply SPlaceLabelsPanel::OnImport()
{
	FString Path;
	if (!PickOpenFile(TEXT("Import regions or names"),
			TEXT("Region data (*.geojson;*.json;*.csv)|*.geojson;*.json;*.csv"), Path))
	{
		return FReply::Handled();
	}

	const FString Extension = FPaths::GetExtension(Path).ToLower();
	const bool bIsCsv = (Extension == TEXT("csv"));

	PlaceLabelsExchange::FResult Result;
	{
		// One transaction over the whole file.
		const FScopedTransaction Transaction(bIsCsv
			? LOCTEXT("ImportCsvTx", "Import Place Label Names")
			: LOCTEXT("ImportGeoJsonTx", "Import Place Regions"));

		if (bIsCsv)
		{
			PlaceLabelsExchange::ImportCsv(GetEditorWorld(), Path, Result);
		}
		else
		{
			PlaceLabelsExchange::ImportGeoJson(GetEditorWorld(), Path, Result);
		}
	}

	for (const FString& Problem : Result.Problems)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlaceLabels import: %s"), *Problem);
	}
	Notify(Result.Summarise(), Result.bSucceeded);

	RefreshNow();
	return FReply::Handled();
}

void SPlaceLabelsPanel::Notify(const FText& Message, bool bSuccess)
{
	FNotificationInfo Info(Message);
	Info.ExpireDuration = bSuccess ? 5.0f : 8.0f;
	TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
	if (Item.IsValid())
	{
		Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}
}

#undef LOCTEXT_NAMESPACE
