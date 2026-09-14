#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

// Starts a property set's categories collapsed, and takes off the rows the holder does not decide.
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

namespace HutongPanelCustomizations
{
	void Register();
	void Unregister();
}
