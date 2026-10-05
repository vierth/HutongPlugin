#include "Tools/HeightsWindow.h"
#include "Tools/HeightsTool.h"
#include "Tools/HutongPresetDefaults.h"
#include "Generation/HutongCanon.h"
#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "HutongHeightsWindow"

namespace
{
	const FName ColBuilding("Building"), ColRole("Role"), ColPreset("Preset"), ColCurrent("Current"), ColNew("New"), ColChange("Change"), ColSuggested("Suggested");

	FText RoleName(EHutongCourtRole Role)
	{
		return StaticEnum<EHutongCourtRole>()->GetDisplayNameTextByValue((int64)Role);
	}

	FText Cm(double V) { return FText::FromString(FString::Printf(TEXT("%.0f cm"), V)); }

	class SHeightsRow : public SMultiColumnTableRow<TSharedPtr<int32>>
	{
	public:
		SLATE_BEGIN_ARGS(SHeightsRow) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, const TSharedRef<STableViewBase>& Owner, TWeakObjectPtr<UHutongHeightsTool> InTool, int32 InIndex)
		{
			Tool = InTool;
			Index = InIndex;
			SMultiColumnTableRow::Construct(FSuperRowType::FArguments().Padding(FMargin(0.0f, 3.0f)), Owner);
		}

		const FHutongEaveRow* Row() const
		{
			return (Tool.IsValid() && Tool->GetRows().IsValidIndex(Index)) ? &Tool->GetRows()[Index] : nullptr;
		}
		bool IsReference() const { return Tool.IsValid() && Tool->GetReferenceRow() == Index; }

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override
		{
			const FHutongEaveRow* R = Row();
			if (!R) return SNullWidget::NullWidget;
			const FMargin Pad(6.0f, 0.0f);

			if (Column == ColBuilding)
			{
				return SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(Pad)
					[
						SNew(STextBlock).Text(FText::FromString(R->Label)).Font(FAppStyle::GetFontStyle("BoldFont"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(Pad)
					[
						SNew(STextBlock).Text(FText::FromString(R->Detail)).Font(FAppStyle::GetFontStyle("SmallFont"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					];
			}
			if (Column == ColRole)
			{
				return SNew(SBox).Padding(Pad).VAlign(VAlign_Center)
				[
					SNew(SComboButton)
					.OnGetMenuContent(this, &SHeightsRow::RoleMenu)
					.ButtonContent()
					[
						SNew(STextBlock).Text_Lambda([this]() { const FHutongEaveRow* X = Row(); return X ? RoleName(X->Role) : FText(); })
					]
				];
			}
			if (Column == ColPreset)
			{
				if (Tool->GetPresetOptions(Index).Num() <= 1)
				{
					return SNew(SBox).Padding(Pad).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("NoPresets", "—")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
					];
				}
				return SNew(SBox).Padding(Pad).VAlign(VAlign_Center)
				[
					SNew(SComboButton)
					.OnGetMenuContent(this, &SHeightsRow::PresetMenu)
					.ButtonContent()
					[
						SNew(STextBlock).Text_Lambda([this]()
						{
							const FHutongEaveRow* X = Row();
							return X ? FText::FromString(X->NewPreset.IsEmpty() ? TEXT("(own parameters)") : X->NewPreset) : FText();
						})
						.ColorAndOpacity_Lambda([this]()
						{
							const FHutongEaveRow* X = Row();
							return (X && X->NewPreset != X->Preset) ? FSlateColor(FLinearColor(1.0f, 0.75f, 0.2f)) : FSlateColor::UseForeground();
						})
					]
				];
			}
			if (Column == ColCurrent)
			{
				return SNew(SBox).Padding(Pad).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Cm(R->CurrentEave)).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				];
			}
			if (Column == ColNew)
			{
				if (!R->bEditable)
				{
					return SNew(SBox).Padding(Pad).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("Fixed", "set in Details")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
					];
				}
				return SNew(SBox).Padding(Pad).VAlign(VAlign_Center)
				[
					SNew(SSpinBox<double>)
					.MinValue(60.0).MaxValue(2000.0).MinSliderValue(150.0).MaxSliderValue(700.0).Delta(1.0)
					.Value_Lambda([this]() { const FHutongEaveRow* X = Row(); return X ? FMath::RoundToDouble(X->NewEave) : 0.0; })
					.OnValueChanged_Lambda([this](double V) { if (Tool.IsValid()) Tool->SetNewEave(Index, V); })
					.OnValueCommitted_Lambda([this](double V, ETextCommit::Type) { if (Tool.IsValid()) Tool->SetNewEave(Index, V); })
				];
			}
			if (Column == ColChange)
			{
				return SNew(SBox).Padding(Pad).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const FHutongEaveRow* X = Row();
						const double D = X ? FMath::RoundToDouble(X->NewEave - X->CurrentEave) : 0.0;
						return FMath::IsNearlyZero(D) ? FText::FromString(TEXT("—")) : FText::FromString(FString::Printf(TEXT("%+.0f cm"), D));
					})
					.ColorAndOpacity_Lambda([this]()
					{
						const FHutongEaveRow* X = Row();
						const bool bChanged = X && FMath::Abs(X->NewEave - X->CurrentEave) > 0.5;
						return bChanged ? FSlateColor(FLinearColor(1.0f, 0.75f, 0.2f)) : FSlateColor::UseSubduedForeground();
					})
				];
			}
			if (Column == ColSuggested)
			{
				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(Pad).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text_Lambda([this]() { return SuggestionText(); })
						.ColorAndOpacity_Lambda([this]() { return IsReference() ? FSlateColor(FLinearColor(0.45f, 0.85f, 1.0f)) : FSlateColor::UseForeground(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(Pad).VAlign(VAlign_Center)
					[
						SNew(SButton)
						.Text(LOCTEXT("UseOne", "Use"))
						.ToolTipText(LOCTEXT("UseOneTip", "Sets this row's New Eave to its suggestion."))
						.Visibility_Lambda([this]() { return (!IsReference() && Tool.IsValid() && Tool->GetSuggestion(Index) > 0.0) ? EVisibility::Visible : EVisibility::Hidden; })
						.OnClicked_Lambda([this]() { if (Tool.IsValid()) Tool->UseSuggestion(Index); return FReply::Handled(); })
					];
			}
			return SNullWidget::NullWidget;
		}

		FText SuggestionText() const
		{
			if (!Tool.IsValid()) return FText();
			const double S = Tool->GetSuggestion(Index);
			const FString Note = Tool->GetSuggestionNote(Index);
			if (IsReference() || S <= 0.0) return FText::FromString(Note);
			return FText::FromString(FString::Printf(TEXT("%.0f cm  (%s)"), S, *Note));
		}

		TSharedRef<SWidget> PresetMenu()
		{
			FMenuBuilder Menu(true, nullptr);
			if (Tool.IsValid())
			{
				for (const FString& Name : Tool->GetPresetOptions(Index))
				{
					Menu.AddMenuEntry(FText::FromString(Name.IsEmpty() ? TEXT("(own parameters)") : Name), FText(), FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([this, Name]() { if (Tool.IsValid()) Tool->SetPreset(Index, Name); })));
				}
			}
			return Menu.MakeWidget();
		}

		TSharedRef<SWidget> RoleMenu()
		{
			FMenuBuilder Menu(true, nullptr);
			const UEnum* Enum = StaticEnum<EHutongCourtRole>();
			for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
			{
				const EHutongCourtRole Role = (EHutongCourtRole)Enum->GetValueByIndex(i);
				if (Role == EHutongCourtRole::Auto) continue;
				Menu.AddMenuEntry(RoleName(Role), FText(), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([this, Role]() { if (Tool.IsValid()) Tool->SetRole(Index, Role); })));
			}
			return Menu.MakeWidget();
		}

		TWeakObjectPtr<UHutongHeightsTool> Tool;
		int32 Index = 0;
	};

	class SHeightsPanel : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHeightsPanel) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&, UHutongHeightsTool* InTool)
		{
			Tool = InTool;
			FNumberFormattingOptions Two;
			Two.SetMaximumFractionalDigits(2);
			using namespace HutongPresets;
			const FText Ranking = FText::Format(LOCTEXT("Ranking",
				"Height is the eave (檐柱 top), or a wall's top. Ranking: Main Hall (正房) 1 · Side House (廂房) {0} · Front Row (倒座房) {1} · Rear Row (後罩房) {2} · Ear Room (耳房) {3} · Lane Wall (院牆) {4} · Court Wall (隔牆) {5}. Gate house (大門) ridge: {6} cm over the row beside it."),
				FText::AsNumber(EaveRatio(EHutongCourtRole::SideHouse), &Two), FText::AsNumber(EaveRatio(EHutongCourtRole::FrontRow), &Two),
				FText::AsNumber(EaveRatio(EHutongCourtRole::RearRow), &Two), FText::AsNumber(EaveRatio(EHutongCourtRole::EarRoom), &Two),
				FText::AsNumber(EaveRatio(EHutongCourtRole::LaneWall), &Two), FText::AsNumber(EaveRatio(EHutongCourtRole::CourtWall), &Two),
				FText::AsNumber(HutongCanon::Gate::RidgeAboveRowCm));

			ChildSlot
			[
				SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder")).Padding(10.0f)
				[
					SNew(SVerticalBox)
					// Any court in the level, whole: its buildings become the selection and the table.
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(STextBlock).Text(LOCTEXT("Open", "Open a court")).Font(FAppStyle::GetFontStyle("BoldFont"))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SComboButton)
							.ToolTipText(LOCTEXT("OpenTip", "Selects every building of that court and lists them here."))
							.OnGetMenuContent_Lambda([this]()
							{
								FMenuBuilder Menu(true, nullptr);
								const TArray<FString> Courts = Tool.IsValid() ? Tool->GetAllCourts() : TArray<FString>();
								if (Courts.Num() == 0)
								{
									Menu.AddMenuEntry(LOCTEXT("NoCourts", "No courts yet: assign one below or with the Courts tool"), FText(), FSlateIcon(), FUIAction());
								}
								for (const FString& Court : Courts)
								{
									Menu.AddMenuEntry(FText::FromString(Court), FText(), FSlateIcon(),
										FUIAction(FExecuteAction::CreateLambda([this, Court]() { if (Tool.IsValid()) Tool->OpenCourt(Court); })));
								}
								return Menu.MakeWidget();
							})
							.ButtonContent()
							[
								SNew(STextBlock).Text(LOCTEXT("Pick", "Choose…"))
							]
						]
					]
					// Court.
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(STextBlock).Text(LOCTEXT("Court", "Courtyard unit (院落)")).Font(FAppStyle::GetFontStyle("BoldFont"))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(SEditableTextBox)
							.Text_Lambda([this]() { return Tool.IsValid() ? FText::FromString(Tool->GetCourtName()) : FText(); })
							.OnTextCommitted_Lambda([this](const FText& T, ETextCommit::Type) { if (Tool.IsValid()) Tool->SetCourtName(T.ToString()); })
							.HintText(LOCTEXT("CourtHint", "e.g. 3M6_Courtyard_1"))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
						[
							SNew(SButton).Text(LOCTEXT("Suggest", "Suggest Name"))
							.ToolTipText(LOCTEXT("SuggestTip", "The next free name on the map tile under the selection."))
							.OnClicked_Lambda([this]() { if (Tool.IsValid()) Tool->SuggestCourtName(); return FReply::Handled(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
						[
							SNew(SButton).Text(LOCTEXT("Assign", "Assign To Selection"))
							.ToolTipText(LOCTEXT("AssignTip", "Stores the name on every selected building; a wall adds it to its courts. Empty clears."))
							.OnClicked_Lambda([this]() { if (Tool.IsValid()) Tool->AssignCourt(); return FReply::Handled(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
						[
							SNew(SButton).Text(LOCTEXT("SelectCourt", "Select Whole Court"))
							.ToolTipText(LOCTEXT("SelectCourtTip", "Adds every loaded building sharing a court with the selection."))
							.OnClicked_Lambda([this]() { if (Tool.IsValid()) Tool->SelectCourt(); return FReply::Handled(); })
						]
					]
					// The table.
					+ SVerticalBox::Slot().FillHeight(1.0f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SAssignNew(List, SListView<TSharedPtr<int32>>)
							.ListItemsSource(&Items)
							.SelectionMode(ESelectionMode::None)
							.OnGenerateRow_Lambda([this](TSharedPtr<int32> Item, const TSharedRef<STableViewBase>& Owner) -> TSharedRef<ITableRow>
							{
								return SNew(SHeightsRow, Owner, Tool, *Item);
							})
							.HeaderRow
							(
								SNew(SHeaderRow)
								+ SHeaderRow::Column(ColBuilding).DefaultLabel(LOCTEXT("ColBuilding", "Building")).FillWidth(0.22f)
								+ SHeaderRow::Column(ColRole).DefaultLabel(LOCTEXT("ColRole", "Role in court")).FillWidth(0.14f)
								+ SHeaderRow::Column(ColPreset).DefaultLabel(LOCTEXT("ColPreset", "Preset / rank")).FillWidth(0.16f)
								+ SHeaderRow::Column(ColCurrent).DefaultLabel(LOCTEXT("ColCurrent", "Height now")).FillWidth(0.07f)
								+ SHeaderRow::Column(ColNew).DefaultLabel(LOCTEXT("ColNew", "New height (cm)")).FillWidth(0.10f)
								+ SHeaderRow::Column(ColChange).DefaultLabel(LOCTEXT("ColChange", "Change")).FillWidth(0.06f)
								+ SHeaderRow::Column(ColSuggested).DefaultLabel(LOCTEXT("ColSuggested", "Suggested")).FillWidth(0.25f)
							)
						]
						+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Visibility_Lambda([this]() { return Items.Num() == 0 ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
							.Text(LOCTEXT("Empty", "Select buildings in the viewport: click, Shift+click to add, or drag a box."))
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(Ranking).AutoWrapText(true).Font(FAppStyle::GetFontStyle("SmallFont"))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
					[
						SNew(SSeparator)
					]
					// Actions.
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SCheckBox)
							.IsChecked_Lambda([this]() { return Tool.IsValid() && Tool->GetKeepProportions() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
							.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { if (Tool.IsValid()) Tool->SetKeepProportions(S == ECheckBoxState::Checked); })
							.ToolTipText(LOCTEXT("KeepTip", "Editing one New Eave scales every other row by the same factor."))
							[
								SNew(STextBlock).Text(LOCTEXT("Keep", "Keep proportions (one row moves the rest)"))
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
						[
							SNew(SButton).Text(LOCTEXT("Reset", "Reset"))
							.ToolTipText(LOCTEXT("ResetTip", "Puts every New Eave back to the eave as built."))
							.OnClicked_Lambda([this]() { if (Tool.IsValid()) Tool->Reset(); return FReply::Handled(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
						[
							SNew(SButton).Text(LOCTEXT("UseAll", "Use All Suggested"))
							.ToolTipText(LOCTEXT("UseAllTip", "Fills every New Eave from its suggestion."))
							.OnClicked_Lambda([this]() { if (Tool.IsValid()) Tool->UseSuggested(); return FReply::Handled(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
						[
							SNew(SButton)
							.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
							.Text(LOCTEXT("Apply", "Apply"))
							.ToolTipText(LOCTEXT("ApplyTip", "Sets every changed eave and rebuilds those buildings, as one undo step."))
							.OnClicked_Lambda([this]() { if (Tool.IsValid()) Tool->Apply(); return FReply::Handled(); })
						]
					]
				]
			];
		}

		virtual void Tick(const FGeometry& Geometry, const double CurrentTime, const float DeltaTime) override
		{
			SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
			if (!Tool.IsValid() || Tool->GetRowsRevision() == SeenRevision) return;
			SeenRevision = Tool->GetRowsRevision();
			Items.Reset();
			for (int32 i = 0; i < Tool->GetRows().Num(); ++i) Items.Add(MakeShared<int32>(i));
			// Rows are built once per set: rebuild them, not only refresh.
			List->RebuildList();
		}

		TWeakObjectPtr<UHutongHeightsTool> Tool;
		TArray<TSharedPtr<int32>> Items;
		TSharedPtr<SListView<TSharedPtr<int32>>> List;
		int32 SeenRevision = -1;
	};
}

TSharedRef<SWidget> HutongHeightsWindow::MakeContent(UHutongHeightsTool* Tool)
{
	return SNew(SHeightsPanel, Tool);
}

#undef LOCTEXT_NAMESPACE
