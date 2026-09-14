#include "HutongPanelCustomizations.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "PropertyHandle.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

TSharedRef<IDetailCustomization> FHutongCollapsedCategoriesCustomization::Make(TArray<FName> InCategories,
	TArray<FName> InHiddenParamFields)
{
	TSharedRef<FHutongCollapsedCategoriesCustomization> C = MakeShared<FHutongCollapsedCategoriesCustomization>();
	C->Categories = MoveTemp(InCategories);
	C->HiddenParamFields = MoveTemp(InHiddenParamFields);
	return C;
}

void FHutongCollapsedCategoriesCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	for (const FName& Category : Categories)
	{
		DetailBuilder.EditCategory(Category).InitiallyCollapsed(true);
	}

	// The tool is the answer to these, so the panel does not offer them: the wall tools each force
	// their own role, and a role picker on top of that is two answers to one question.
	if (HiddenParamFields.Num() > 0)
	{
		const TSharedPtr<IPropertyHandle> Params = DetailBuilder.GetProperty(TEXT("Params"));
		if (Params.IsValid() && Params->IsValidHandle())
		{
			for (const FName& Field : HiddenParamFields)
			{
				const TSharedPtr<IPropertyHandle> Child = Params->GetChildHandle(Field);
				if (Child.IsValid() && Child->IsValidHandle()) DetailBuilder.HideProperty(Child);
			}
		}
	}
}

namespace
{
	struct FCollapsed
	{
		const TCHAR* Class;
		TArray<FName> Categories;
		TArray<FName> HiddenParamFields;
	};

	// **Register against the class that declares the properties, never a subclass of it.** A details
	// view asks for a layout once per class it found properties on, and walks *upward* from there —
	// a class declaring nothing of its own is never asked, whatever is registered under its name.
	// `Params` is declared on UHutongWallToolProperties, so the two wall tools' own set classes are
	// invisible to this and the role row they registered to hide stayed on the panel for both.
	const TArray<FCollapsed>& Collapsed()
	{
		static const TArray<FName> WallCategories = {
			TEXT("Cap"), TEXT("Window Details"), TEXT("Garden Doorway Details"), TEXT("Gate Details") };
		static const TArray<FCollapsed> Table = {
			{ TEXT("HutongSnapProperties"), { TEXT("Snapping") } },
			{ TEXT("HutongAppearanceProperties"), { TEXT("Appearance") } },
			// Both wall tools force their own role, so neither panel offers the picker.
			{ TEXT("HutongWallToolProperties"), WallCategories, { TEXT("Role") } },
			// The placed wall keeps its role editable: retyping a run is a Details edit, not a redraw.
			{ TEXT("HutongWallBuildingComponent"), WallCategories },
		};
		return Table;
	}
}

void HutongPanelCustomizations::Register()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	for (const FCollapsed& C : Collapsed())
	{
		const TArray<FName> Categories = C.Categories;
		const TArray<FName> Hidden = C.HiddenParamFields;
		PropertyEditor.RegisterCustomClassLayout(C.Class,
			FOnGetDetailCustomizationInstance::CreateLambda([Categories, Hidden]
			{
				return FHutongCollapsedCategoriesCustomization::Make(Categories, Hidden);
			}));
	}
}

void HutongPanelCustomizations::Unregister()
{
	FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (!PropertyEditor) return;
	for (const FCollapsed& C : Collapsed())
	{
		PropertyEditor->UnregisterCustomClassLayout(C.Class);
	}
}
