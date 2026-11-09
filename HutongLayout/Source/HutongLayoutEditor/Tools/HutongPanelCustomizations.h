#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class FHutongCollapsedCategoriesCustomization : public IDetailCustomization
{
public:
	// HiddenParamFields name fields inside the set's Params struct; HiddenOnFor, when set, limits hiding them to
	// objects of that class or below (a subclass declaring nothing is never asked for a layout of its own).
	static TSharedRef<IDetailCustomization> Make(TArray<FName> InCategories,
		TArray<FName> InHiddenParamFields = {}, FName InHiddenOnFor = NAME_None);
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	TArray<FName> Categories;
	TArray<FName> HiddenParamFields;
	FName HiddenOnFor;
};

class FHutongPresetCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> Make() { return MakeShared<FHutongPresetCustomization>(); }
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

// Registered on UHutongBuildingComponent, the class declaring the shared fields, so every type's layout asks it.
class FHutongBuildingComponentCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> Make() { return MakeShared<FHutongBuildingComponentCustomization>(); }
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

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

	FText PluginUpdatedText();
}
