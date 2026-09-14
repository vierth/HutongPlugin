#include "PlaceLabelsStyle.h"

#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FPlaceLabelsStyle::StyleSet = nullptr;

FName FPlaceLabelsStyle::GetStyleSetName()
{
	static const FName Name("PlaceLabelsStyle");
	return Name;
}

void FPlaceLabelsStyle::Register()
{
	if (StyleSet.IsValid())
	{
		return;
	}

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("PlaceLabels"));
	if (!Plugin.IsValid())
	{
		return;
	}

	StyleSet = MakeShared<FSlateStyleSet>(GetStyleSetName());
	StyleSet->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources") / TEXT("Icons"));

	const FVector2D Icon20(20.0f, 20.0f);
	const FVector2D Icon40(40.0f, 40.0f);

	auto AddToolIcon = [&](const FName& StyleName, const FString& SvgName)
	{
		const FString Path = StyleSet->RootToContentDir(SvgName, TEXT(".svg"));
		StyleSet->Set(StyleName, new FSlateVectorImageBrush(Path, Icon40));
		StyleSet->Set(FName(*(StyleName.ToString() + TEXT(".Small"))),
			new FSlateVectorImageBrush(Path, Icon20));
	};

	AddToolIcon("PlaceLabels.BeginSelectTool", TEXT("SelectTool"));
	AddToolIcon("PlaceLabels.BeginPenTool", TEXT("PenTool"));
	AddToolIcon("PlaceLabels.BeginEditTool", TEXT("EditTool"));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FPlaceLabelsStyle::Unregister()
{
	if (!StyleSet.IsValid())
	{
		return;
	}
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
	StyleSet.Reset();
}
