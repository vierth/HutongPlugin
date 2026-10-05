#include "Tools/CourtsWindow.h"
#include "Tools/CourtsTool.h"
#include "Styling/AppStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "HutongCourtsWindow"

namespace
{
	const FName ColCourt("Court"), ColTile("Tile"), ColCount("Count"), ColActions("Actions");
	using FItem = TSharedPtr<HutongCourts::FSummary>;

	class SCourtRow : public SMultiColumnTableRow<FItem>
	{
	public:
		SLATE_BEGIN_ARGS(SCourtRow) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, const TSharedRef<STableViewBase>& Owner, TWeakObjectPtr<UHutongCourtsTool> InTool, FItem InItem)
		{
			Tool = InTool;
			Item = InItem;
			SMultiColumnTableRow::Construct(FSuperRowType::FArguments().Padding(FMargin(0.0f, 3.0f)), Owner);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
		{
			const FMargin Pad(6.0f, 0.0f);
			const FString Name = Item->Name;
			if (Column == ColCourt)
			{
				return SNew(SBox).Padding(Pad).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(Name)).Font(FAppStyle::GetFontStyle("BoldFont"))
					// The courts the selection is in stand out.
					.ColorAndOpacity_Lambda([this, Name]()
					{
						return Tool.IsValid() && Tool->IsSelectionIn(Name) ? FSlateColor(FLinearColor(0.45f, 0.85f, 1.0f)) : FSlateColor::UseForeground();
					})
				];
			}
			if (Column == ColTile)
			{
				return SNew(SBox).Padding(Pad).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(Item->Tile)).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				];
			}
			if (Column == ColCount)
			{
				return SNew(SBox).Padding(Pad).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::AsNumber(Item->Buildings))
				];
			}
			auto Button = [this](const FText& Label, const FText& Tip, TFunction<void()> Do, TFunction<bool()> Enabled = nullptr)
			{
				return SNew(SButton).Text(Label).ToolTipText(Tip)
					.IsEnabled_Lambda([Enabled]() { return !Enabled || Enabled(); })
					.OnClicked_Lambda([this, Do]() { if (Tool.IsValid()) Do(); return FReply::Handled(); });
			};
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
				[
					Button(LOCTEXT("Select", "Select"), LOCTEXT("SelectTip", "Selects every building of this court."),
						[this, Name]() { Tool->SelectCourt(Name); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
				[
					Button(LOCTEXT("Add", "Add Selected"), LOCTEXT("AddTip", "Puts the selected buildings in this court and files them with it."),
						[this, Name]() { Tool->AddSelectedTo(Name); }, [this]() { return Tool.IsValid() && Tool->GetSelectedCount() > 0; })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
				[
					Button(LOCTEXT("Remove", "Remove Selected"), LOCTEXT("RemoveTip", "Takes the selected buildings out of this court."),
						[this, Name]() { Tool->RemoveSelectedFrom(Name); }, [this, Name]() { return Tool.IsValid() && Tool->IsSelectionIn(Name); })
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
				[
					Button(LOCTEXT("Heights", "Edit Heights"), LOCTEXT("HeightsTip", "Selects this court and opens it in the heights tool."),
						[this, Name]() { Tool->EditHeights(Name); })
				];
		}

		TWeakObjectPtr<UHutongCourtsTool> Tool;
		FItem Item;
	};

	class SCourtsPanel : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SCourtsPanel) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, UHutongCourtsTool* InTool)
		{
			Tool = InTool;
			ChildSlot
			[
				SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder")).Padding(10.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(STextBlock).AutoWrapText(true)
						.Text_Lambda([this]() { return Tool.IsValid() ? FText::FromString(Tool->GetSelectionText()) : FText(); })
					]
					// A new court from the selection.
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(STextBlock).Text(LOCTEXT("New", "New court")).Font(FAppStyle::GetFontStyle("BoldFont"))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(SEditableTextBox)
							.Text_Lambda([this]() { return Tool.IsValid() ? FText::FromString(Tool->GetCourtName()) : FText(); })
							.OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { if (Tool.IsValid()) Tool->SetCourtName(T.ToString()); })
							.HintText(LOCTEXT("NameHint", "e.g. 3M6_Courtyard_1"))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
						[
							SNew(SButton).Text(LOCTEXT("Suggest", "Suggest Name"))
							.ToolTipText(LOCTEXT("SuggestTip", "The next free name on the map tile under the selection."))
							.OnClicked_Lambda([this]() { if (Tool.IsValid()) Tool->SuggestName(); return FReply::Handled(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
						[
							SNew(SButton)
							.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
							.Text(LOCTEXT("Assign", "Assign To Selection"))
							.ToolTipText(LOCTEXT("AssignTip", "Puts the selected buildings in the named court."))
							.IsEnabled_Lambda([this]() { return Tool.IsValid() && Tool->GetSelectedCount() > 0; })
							.OnClicked_Lambda([this]() { if (Tool.IsValid()) Tool->AssignNew(); return FReply::Handled(); })
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SAssignNew(List, SListView<FItem>)
							.ListItemsSource(&Items)
							.SelectionMode(ESelectionMode::None)
							.OnGenerateRow_Lambda([this](FItem Item, const TSharedRef<STableViewBase>& Owner) -> TSharedRef<ITableRow>
							{
								return SNew(SCourtRow, Owner, Tool, Item);
							})
							.HeaderRow
							(
								SNew(SHeaderRow)
								+ SHeaderRow::Column(ColCourt).DefaultLabel(LOCTEXT("ColCourt", "Court")).FillWidth(0.30f)
								+ SHeaderRow::Column(ColTile).DefaultLabel(LOCTEXT("ColTile", "Map tile")).FillWidth(0.10f)
								+ SHeaderRow::Column(ColCount).DefaultLabel(LOCTEXT("ColCount", "Buildings")).FillWidth(0.10f)
								+ SHeaderRow::Column(ColActions).DefaultLabel(LOCTEXT("ColActions", "")).FillWidth(0.50f)
							)
						]
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Visibility_Lambda([this]() { return Items.Num() == 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
							.Text(LOCTEXT("Empty", "No courts yet: select a courtyard's buildings and Assign To Selection."))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
					[
						SNew(SSeparator)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock).AutoWrapText(true).Font(FAppStyle::GetFontStyle("SmallFont"))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
							.Text(LOCTEXT("Folders", "Courts are filed in the Outliner under Courts / map tile / court; shared walls under the tile's Shared walls."))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton).Text(LOCTEXT("Rebuild", "Rebuild Folders"))
							.ToolTipText(LOCTEXT("RebuildTip", "Files every loaded building again from the court it carries."))
							.OnClicked_Lambda([this]() { if (Tool.IsValid()) Tool->RebuildFolders(); return FReply::Handled(); })
						]
					]
				]
			];
		}

		virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override
		{
			SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
			if (!Tool.IsValid() || Tool->GetCourtsRevision() == SeenRevision) return;
			SeenRevision = Tool->GetCourtsRevision();
			Items.Reset();
			for (const HutongCourts::FSummary& S : Tool->GetCourts()) Items.Add(MakeShared<HutongCourts::FSummary>(S));
			List->RebuildList();
		}

		TWeakObjectPtr<UHutongCourtsTool> Tool;
		TArray<FItem> Items;
		TSharedPtr<SListView<FItem>> List;
		int32 SeenRevision = -1;
	};
}

TSharedRef<SWidget> HutongCourtsWindow::MakeContent(UHutongCourtsTool* Tool)
{
	return SNew(SCourtsPanel, Tool);
}

#undef LOCTEXT_NAMESPACE
