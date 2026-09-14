#include "Tools/HutongImportTypes.h"
#include "Tools/HutongExchange.h"

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "HutongImportTypes"

namespace HutongImportTypes
{

namespace
{
	// One row of the dialog: a type the file names, and what to build for it.
	struct FRow
	{
		FName Unknown;
		int32 Count = 0;
		// Index into the shared option list; 0 is "skip these".
		TSharedPtr<FString> Choice;
	};

	FText DescribeClass(const UClass* Class)
	{
		if (!Class) return LOCTEXT("SkipThese", "Skip these");
		return FText::Format(LOCTEXT("ClassChoice", "{0}  ({1})"),
			Class->GetDisplayNameText(), FText::FromString(Class->GetName()));
	}
}

bool SkipUnknownTypes(HutongExchange::FSceneFile& File)
{
	TMap<FName, int32> Unknown;
	HutongExchange::FindUnknownTypes(File, Unknown);
	for (const TPair<FName, int32>& Pair : Unknown)
	{
		File.TypeRemap.Add(Pair.Key, NAME_None);
	}
	return true;
}

bool ResolveUnknownTypes(HutongExchange::FSceneFile& File)
{
	TMap<FName, int32> Unknown;
	HutongExchange::FindUnknownTypes(File, Unknown);
	// A type the caller has already answered for is not asked about again.
	for (auto It = Unknown.CreateIterator(); It; ++It)
	{
		if (File.TypeRemap.Contains(It.Key())) It.RemoveCurrent();
	}
	// Nothing to ask about is the ordinary case, and it must not cost a dialog.
	if (Unknown.Num() == 0) return true;

	if (!FSlateApplication::IsInitialized() || GEditor == nullptr)
	{
		return SkipUnknownTypes(File);
	}

	TMap<FName, UClass*> Classes;
	HutongExchange::GatherBuildingComponentClasses(Classes);

	// "Skip these" first, then every type this build has, by name.
	TArray<FName> Known;
	Classes.GenerateKeyArray(Known);
	Known.Sort(FNameLexicalLess());

	TSharedRef<TArray<TSharedPtr<FString>>> Options = MakeShared<TArray<TSharedPtr<FString>>>();
	Options->Add(MakeShared<FString>(TEXT("")));
	for (const FName& Name : Known)
	{
		Options->Add(MakeShared<FString>(Name.ToString()));
	}

	TSharedRef<TArray<TSharedPtr<FRow>>> Rows = MakeShared<TArray<TSharedPtr<FRow>>>();
	TArray<FName> UnknownNames;
	Unknown.GenerateKeyArray(UnknownNames);
	UnknownNames.Sort(FNameLexicalLess());
	for (const FName& Name : UnknownNames)
	{
		TSharedRef<FRow> Row = MakeShared<FRow>();
		Row->Unknown = Name;
		Row->Count = Unknown[Name];
		// Skipping is the default: a wrong type placed silently is worse than one left out loudly.
		Row->Choice = (*Options)[0];
		Rows->Add(Row);
	}

	auto Describe = [&Classes](const TSharedPtr<FString>& Item)
	{
		if (!Item.IsValid() || Item->IsEmpty()) return LOCTEXT("SkipThese", "Skip these");
		UClass* const* Found = Classes.Find(FName(**Item));
		return DescribeClass(Found ? *Found : nullptr);
	};

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	Body->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		SNew(STextBlock)
			.AutoWrapText(true)
			.Text(LOCTEXT("UnknownIntro",
				"This file names building types that are not in this build — it was probably written before a type was renamed or split. Choose what to place for each; the choice applies to every building of that type in the file."))
	];

	for (const TSharedPtr<FRow>& Row : *Rows)
	{
		TSharedPtr<FRow> Captured = Row;
		Body->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::Format(LOCTEXT("UnknownRow", "{0}  —  {1} building(s)"),
					FText::FromName(Row->Unknown), FText::AsNumber(Row->Count)))
			]
			+ SHorizontalBox::Slot().FillWidth(0.5f)
			[
				SNew(SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&(*Options))
					.InitiallySelectedItem(Row->Choice)
					.OnGenerateWidget_Lambda([Describe](TSharedPtr<FString> Item)
					{
						return SNew(STextBlock).Text(Describe(Item));
					})
					.OnSelectionChanged_Lambda([Captured](TSharedPtr<FString> Item, ESelectInfo::Type)
					{
						if (Captured.IsValid() && Item.IsValid()) Captured->Choice = Item;
					})
					[
						SNew(STextBlock).Text_Lambda([Captured, Describe]
						{
							return Describe(Captured.IsValid() ? Captured->Choice : nullptr);
						})
					]
			]
		];
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("UnknownTitle", "Unrecognised building types"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(720.0f, 120.0f + 34.0f * Rows->Num()))
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	bool bImport = false;
	Window->SetContent(
		SNew(SBorder)
			.Padding(12.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SScrollBox) + SScrollBox::Slot()[ Body ]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 10.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
					[
						SNew(SButton)
							.Text(LOCTEXT("CancelImport", "Cancel Import"))
							.OnClicked_Lambda([Window]()
							{
								Window->RequestDestroyWindow();
								return FReply::Handled();
							})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
					[
						SNew(SButton)
							.Text(LOCTEXT("DoImport", "Import"))
							.OnClicked_Lambda([Window, &bImport]()
							{
								bImport = true;
								Window->RequestDestroyWindow();
								return FReply::Handled();
							})
					]
				]
			]);

	GEditor->EditorAddModalWindow(Window);
	if (!bImport) return false;

	for (const TSharedPtr<FRow>& Row : *Rows)
	{
		const FName To = (Row->Choice.IsValid() && !Row->Choice->IsEmpty())
			? FName(**Row->Choice) : NAME_None;
		File.TypeRemap.Add(Row->Unknown, To);
	}
	return true;
}

} // namespace HutongImportTypes

#undef LOCTEXT_NAMESPACE
