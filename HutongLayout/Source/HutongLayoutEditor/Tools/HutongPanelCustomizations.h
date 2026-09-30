#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

// Collapses a property set's categories and hides the rows the holder does not decide.
class FHutongCollapsedCategoriesCustomization : public IDetailCustomization
{
public:
	// HiddenParamFields name fields inside the set's Params struct.
	static TSharedRef<IDetailCustomization> Make(TArray<FName> InCategories,
		TArray<FName> InHiddenParamFields = {});
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	TArray<FName> Categories;
	TArray<FName> HiddenParamFields;
};

// The preset picker's buttons: Reload always, Save and Delete only in the advanced view.
class FHutongPresetCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> Make() { return MakeShared<FHutongPresetCustomization>(); }
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

// A placed building in the level editor's Details, laid out as its tool: a simple / advanced switch
// (the mode panel's setting), the simple view's rows only, engine categories out of the simple view,
// and the tool's order — Metadata, Preset, the type's own settings, then Footprint, Detail,
// Appearance, Identity. On UHutongBuildingComponent, the class declaring the shared fields, so every
// type's layout asks it.
class FHutongBuildingComponentCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> Make() { return MakeShared<FHutongBuildingComponentCustomization>(); }
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

// The Scene tab: each Selection setting carries an Apply button, lit while applying it would change
// something; categories in working order.
class FHutongModeSettingsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> Make() { return MakeShared<FHutongModeSettingsCustomization>(); }
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

namespace HutongPanelCustomizations
{
	void Register();
	void Unregister();
}
