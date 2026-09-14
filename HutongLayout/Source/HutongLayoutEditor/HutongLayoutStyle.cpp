#include "HutongLayoutStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"
#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FHutongLayoutStyle::StyleSet = nullptr;

FName FHutongLayoutStyle::GetStyleSetName()
{
	static const FName Name("HutongLayoutStyle");
	return Name;
}

void FHutongLayoutStyle::Register()
{
	if (StyleSet.IsValid()) return;

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("HutongLayout"));
	if (!Plugin.IsValid()) return;

	StyleSet = MakeShared<FSlateStyleSet>(GetStyleSetName());
	StyleSet->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources") / TEXT("Icons"));

	// 20/16, not 40/20: the editor's own mode palettes run at this size, and at 40 these buttons were the loudest thing in the panel and pushed their labels into truncating.
	const FVector2D Icon16(16.0f, 16.0f);
	const FVector2D Icon20(20.0f, 20.0f);

	auto AddToolIcon = [&](const FName& StyleName, const FString& SvgName)
	{
		const FString Path = StyleSet->RootToContentDir(SvgName, TEXT(".svg"));
		StyleSet->Set(StyleName, new FSlateVectorImageBrush(Path, Icon20));
		StyleSet->Set(FName(*(StyleName.ToString() + TEXT(".Small"))), new FSlateVectorImageBrush(Path, Icon16));
	};

	// One line per tool, and the style name has to be "HutongLayout.<command member>" exactly.
	AddToolIcon("HutongLayout.BeginWallTool", TEXT("WallTool"));
	AddToolIcon("HutongLayout.BeginCourtWallTool", TEXT("CourtWallTool"));
	AddToolIcon("HutongLayout.BeginSiheyuanTool", TEXT("SiheyuanTool"));
	AddToolIcon("HutongLayout.BeginEarPassageTool", TEXT("EarPassageTool"));
	AddToolIcon("HutongLayout.BeginGateHouseTool", TEXT("GateHouseTool"));
	AddToolIcon("HutongLayout.BeginPaifangTool", TEXT("PaifangTool"));
	AddToolIcon("HutongLayout.BeginInnerGateTool", TEXT("InnerGateTool"));
	AddToolIcon("HutongLayout.BeginCorridorTool", TEXT("CorridorTool"));
	AddToolIcon("HutongLayout.BeginScreenWallTool", TEXT("ScreenWallTool"));
	AddToolIcon("HutongLayout.BeginShopfrontTool", TEXT("ShopfrontTool"));
	AddToolIcon("HutongLayout.BeginStoreyTool", TEXT("StoreyTool"));
	AddToolIcon("HutongLayout.BeginPavilionTool", TEXT("PavilionTool"));
	AddToolIcon("HutongLayout.BeginHallTool", TEXT("HallTool"));
	AddToolIcon("HutongLayout.BeginPathTool", TEXT("PathTool"));
	AddToolIcon("HutongLayout.BeginCompoundTool", TEXT("CompoundTool"));
	AddToolIcon("HutongLayout.BeginStreetRowTool", TEXT("StreetRowTool"));
	AddToolIcon("HutongLayout.BeginGalleryTool", TEXT("GalleryTool"));
	AddToolIcon("HutongLayout.BeginFlowerBedTool", TEXT("FlowerBedTool"));
	AddToolIcon("HutongLayout.BeginWaterJarTool", TEXT("WaterJarTool"));
	AddToolIcon("HutongLayout.BeginMeasureTool", TEXT("MeasureTool"));
	AddToolIcon("HutongLayout.BeginImportTool", TEXT("ImportTool"));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FHutongLayoutStyle::Unregister()
{
	if (!StyleSet.IsValid()) return;
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
	StyleSet.Reset();
}
