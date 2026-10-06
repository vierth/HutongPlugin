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

	// 20/16, not 40/20: matches the editor's mode palettes; at 40 the buttons dominated and truncated their labels.
	const FVector2D Icon16(16.0f, 16.0f);
	const FVector2D Icon20(20.0f, 20.0f);

	auto AddToolIcon = [&](const FName& StyleName, const FString& SvgName)
	{
		const FString Path = StyleSet->RootToContentDir(SvgName, TEXT(".svg"));
		StyleSet->Set(StyleName, new FSlateVectorImageBrush(Path, Icon20));
		StyleSet->Set(FName(*(StyleName.ToString() + TEXT(".Small"))), new FSlateVectorImageBrush(Path, Icon16));
	};

	// One line per tool; the style name must be exactly "HutongLayout.<command member>".
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
	AddToolIcon("HutongLayout.BeginFrameTool", TEXT("FrameTool"));
	AddToolIcon("HutongLayout.BeginPathTool", TEXT("PathTool"));
	AddToolIcon("HutongLayout.BeginCompoundTool", TEXT("CompoundTool"));
	AddToolIcon("HutongLayout.BeginStreetRowTool", TEXT("StreetRowTool"));
	AddToolIcon("HutongLayout.BeginGalleryTool", TEXT("GalleryTool"));
	AddToolIcon("HutongLayout.BeginGalleryWallsTool", TEXT("GalleryWallsTool"));
	AddToolIcon("HutongLayout.BeginGalleryHousesTool", TEXT("GalleryHousesTool"));
	AddToolIcon("HutongLayout.BeginGalleryGatesTool", TEXT("GalleryGatesTool"));
	AddToolIcon("HutongLayout.BeginGalleryCourtyardTool", TEXT("GalleryCourtyardTool"));
	AddToolIcon("HutongLayout.BeginGalleryStreetTool", TEXT("GalleryStreetTool"));
	AddToolIcon("HutongLayout.BeginGalleryTemplesTool", TEXT("GalleryTemplesTool"));
	AddToolIcon("HutongLayout.BeginFlowerBedTool", TEXT("FlowerBedTool"));
	AddToolIcon("HutongLayout.BeginWaterJarTool", TEXT("WaterJarTool"));
	AddToolIcon("HutongLayout.BeginUnknownTool", TEXT("UnknownTool"));
	AddToolIcon("HutongLayout.BeginCityWallTool", TEXT("CityWallTool"));
	AddToolIcon("HutongLayout.BeginSmallBuildingTool", TEXT("SmallBuildingTool"));
	AddToolIcon("HutongLayout.BeginMeasureTool", TEXT("MeasureTool"));
	AddToolIcon("HutongLayout.BeginHeightsTool", TEXT("HeightsTool"));
	AddToolIcon("HutongLayout.BeginCourtsTool", TEXT("CourtsTool"));
	AddToolIcon("HutongLayout.BeginImportTool", TEXT("ImportTool"));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FHutongLayoutStyle::Unregister()
{
	if (!StyleSet.IsValid()) return;
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
	StyleSet.Reset();
}
