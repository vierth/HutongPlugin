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

namespace HutongPanelCustomizations
{
	void Register();
	void Unregister();
}
