#include "PlaceLabelHUDWidget.h"

#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

bool UPlaceLabelHUDWidget::GetInnermost(FPlaceLabelEntry& OutEntry) const
{
	if (Chain.Num() == 0)
	{
		return false;
	}
	OutEntry = Chain[0];
	return true;
}

void UPlaceLabelHUDWidget::SetChain(const TArray<FPlaceLabelEntry>& InChain)
{
	const bool bWasPopulated = Chain.Num() > 0;
	Chain = InChain;

	OnPlaceChainChanged();

	if (bWasPopulated && Chain.Num() == 0)
	{
		OnLeftAllRegions();
	}
}

void UPlaceLabelDefaultHUDWidget::SetChain(const TArray<FPlaceLabelEntry>& InChain)
{
	Super::SetChain(InChain);
	RebuildLines();
}

TSharedRef<SWidget> UPlaceLabelDefaultHUDWidget::RebuildWidget()
{
	LinesBox = SNew(SVerticalBox);
	RebuildLines();

	return SNew(SBox)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 48.0f, 48.0f))
		[
			LinesBox.ToSharedRef()
		];
}

void UPlaceLabelDefaultHUDWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	LinesBox.Reset();
}

void UPlaceLabelDefaultHUDWidget::RebuildLines()
{
	if (!LinesBox.IsValid())
	{
		return;
	}

	LinesBox->ClearChildren();

	for (const FPlaceLabelEntry& Entry : Chain)
	{
		// The region the player is actually in reads large and opaque; each step outward is smaller and fainter.
		const bool bLeaf = Entry.Depth == 0;

		FLinearColor Color = Entry.Type ? Entry.Type->AccentColor : FLinearColor::White;
		Color.A = bLeaf ? 1.0f : FMath::Max(0.35f, 0.8f - 0.15f * Entry.Depth);

		const FLinearColor SecondaryColor(Color.R, Color.G, Color.B, Color.A * 0.75f);

		const int32 NativeSize = bLeaf ? 34 : FMath::Max(16, 25 - 3 * Entry.Depth);
		const int32 PinyinSize = bLeaf ? 20 : FMath::Max(12, 16 - 2 * Entry.Depth);
		const int32 EnglishSize = bLeaf ? 18 : FMath::Max(11, 14 - 2 * Entry.Depth);

		// One row per region: 大柵欄 Dashilan Great Fence, side by side.
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
		bool bRowHasContent = false;

		// Every field is shown exactly as it was typed.
		auto AddPart = [&](const FText& Text, int32 FontSize, const FLinearColor& PartColor)
		{
			if (Text.IsEmpty())
			{
				return;
			}

			// The default Slate font is a composite whose fallback face covers CJK.
			Row->AddSlot()
				.AutoWidth()
				// Bottom, not centre: the parts are different sizes, and sitting them on a common baseline is what keeps the row from looking like it is falling apart.
				.VAlign(VAlign_Bottom)
				.Padding(bRowHasContent ? 10.0f : 0.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(Text)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", FontSize))
					.ColorAndOpacity(FSlateColor(PartColor))
					.ShadowOffset(FVector2D(2.0f, 2.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
				];

			bRowHasContent = true;
		};

		AddPart(Entry.Name.Chinese, NativeSize, Color);
		AddPart(Entry.Name.Pinyin, PinyinSize, SecondaryColor);
		AddPart(Entry.Name.English, EnglishSize, SecondaryColor);

		if (!bRowHasContent)
		{
			continue;
		}

		LinesBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0.0f, bLeaf ? 0.0f : 6.0f, 0.0f, 0.0f)
			[
				Row
			];
	}
}
