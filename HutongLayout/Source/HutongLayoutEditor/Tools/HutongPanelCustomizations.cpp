#include "HutongPanelCustomizations.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "HutongLayoutEdMode.h"
#include "HutongLayoutModeSettings.h"
#include "HutongPresets.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SWrapBox.h"
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

	// The tool already decides these (each wall tool forces its role), so the panel hides them.
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

// Rebuilt by the toolkit's ForceRefresh when the advanced switch flips, so reading it here suffices.
void FHutongPresetCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1) return;
	const TWeakObjectPtr<UHutongPresetProperties> Presets = Cast<UHutongPresetProperties>(Objects[0].Get());
	if (!Presets.IsValid()) return;

	const UHutongLayoutModeSettings* Settings = UHutongLayoutEdMode::GetActiveSettings();
	const bool bAdvanced = Settings && Settings->bShowAdvancedSettings;

	auto Button = [Presets](const FText& Label, const FText& Tip, void (UHutongPresetProperties::*Action)())
	{
		return SNew(SButton)
			.Text(Label)
			.ToolTipText(Tip)
			.OnClicked_Lambda([Presets, Action]
			{
				if (UHutongPresetProperties* P = Presets.Get()) (P->*Action)();
				return FReply::Handled();
			});
	};

	TSharedRef<SWrapBox> Buttons = SNew(SWrapBox).UseAllottedSize(true);
	Buttons->AddSlot().Padding(0.0f, 2.0f, 4.0f, 2.0f)
	[
		Button(NSLOCTEXT("HutongPresets", "Reload", "Reload"),
			NSLOCTEXT("HutongPresets", "ReloadTip", "Reapplies the selected preset, discarding changes made since."),
			&UHutongPresetProperties::LoadSelectedPreset)
	];
	if (bAdvanced)
	{
		Buttons->AddSlot().Padding(0.0f, 2.0f, 4.0f, 2.0f)
		[
			Button(NSLOCTEXT("HutongPresets", "Save", "Save"),
				NSLOCTEXT("HutongPresets", "SaveTip", "Saves the current settings under the Save As name."),
				&UHutongPresetProperties::SaveCurrentAsPreset)
		];
		Buttons->AddSlot().Padding(0.0f, 2.0f, 4.0f, 2.0f)
		[
			Button(NSLOCTEXT("HutongPresets", "Delete", "Delete"),
				NSLOCTEXT("HutongPresets", "DeleteTip", "Deletes the selected user preset."),
				&UHutongPresetProperties::DeleteSelectedPreset)
		];
	}

	DetailBuilder.EditCategory(TEXT("Preset")).AddCustomRow(NSLOCTEXT("HutongPresets", "Buttons", "Preset Buttons"))
	.WholeRowContent()
	[
		Buttons
	];
}

namespace
{
	struct FCollapsed
	{
		const TCHAR* Class;
		TArray<FName> Categories;
		TArray<FName> HiddenParamFields;
	};

	// **Register against the class declaring the properties, never a subclass.** A details view asks
	// once per class it found properties on and walks upward; a class declaring nothing is never asked.
	// `Params` is on UHutongWallToolProperties, so registering on the wall tools' subclasses hid nothing.
	const TArray<FCollapsed>& Collapsed()
	{
		static const TArray<FName> WallCategories = {
			TEXT("Cap"), TEXT("Window Details"), TEXT("Garden Doorway Details"), TEXT("Gate Details") };
		static const TArray<FCollapsed> Table = {
			{ TEXT("HutongSnapProperties"), { TEXT("Snapping") } },
			{ TEXT("HutongAppearanceProperties"), { TEXT("Appearance") } },
			// Evidence notes for tracing; a student placing freely need not open them.
			{ TEXT("HutongMetadataProperties"), { TEXT("Metadata") } },
			// Both wall tools force their role; no picker.
			{ TEXT("HutongWallToolProperties"), WallCategories, { TEXT("Role") } },
			// Placed wall keeps Role editable: retyping a run is a Details edit, not a redraw.
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
	PropertyEditor.RegisterCustomClassLayout(TEXT("HutongPresetProperties"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHutongPresetCustomization::Make));
}

void HutongPanelCustomizations::Unregister()
{
	FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (!PropertyEditor) return;
	for (const FCollapsed& C : Collapsed())
	{
		PropertyEditor->UnregisterCustomClassLayout(C.Class);
	}
	PropertyEditor->UnregisterCustomClassLayout(TEXT("HutongPresetProperties"));
}
