#include "HutongPanelCustomizations.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "HutongLayoutEdMode.h"
#include "HutongLayoutModeSettings.h"
#include "HutongPresets.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Generation/HutongBuildingComponent.h"
#include "Tools/HutongPanelFilter.h"
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
	// The switch lives on the mode's settings; with the mode down, on their defaults (the same config).
	bool ShowAdvanced()
	{
		const UHutongLayoutModeSettings* Active = UHutongLayoutEdMode::GetActiveSettings();
		return (Active ? Active : GetDefault<UHutongLayoutModeSettings>())->bShowAdvancedSettings;
	}

	void SetShowAdvanced(bool bShow)
	{
		UHutongLayoutModeSettings* Defaults = GetMutableDefault<UHutongLayoutModeSettings>();
		Defaults->bShowAdvancedSettings = bShow;
		if (UHutongLayoutModeSettings* Active = UHutongLayoutEdMode::GetActiveSettings()) Active->bShowAdvancedSettings = bShow;
		Defaults->SaveConfig();
	}

	// Hidden where the simple view hides it: the property, or failing that its hidden descendants.
	void HideForSimpleView(IDetailLayoutBuilder& DetailBuilder, const TSharedPtr<IPropertyHandle>& Handle, TArray<const FProperty*>& Parents)
	{
		const FProperty* Prop = Handle.IsValid() && Handle->IsValidHandle() ? Handle->GetProperty() : nullptr;
		if (!Prop) return;
		if (!HutongPanel::IsVisible(*Prop, Parents, false))
		{
			DetailBuilder.HideProperty(Handle);
			return;
		}
		const FStructProperty* Struct = CastField<FStructProperty>(Prop);
		if (!Struct || !Struct->Struct->GetName().StartsWith(TEXT("Hutong"))) return;
		uint32 Count = 0;
		Handle->GetNumChildren(Count);
		Parents.Add(Prop);
		for (uint32 i = 0; i < Count; ++i) HideForSimpleView(DetailBuilder, Handle->GetChildHandle(i), Parents);
		Parents.Pop();
	}

	bool IsOurs(const FProperty* Prop)
	{
		const UStruct* Owner = Prop ? Prop->GetOwnerStruct() : nullptr;
		return Owner && (Owner->IsChildOf(UHutongBuildingComponent::StaticClass()) || Owner->GetName().StartsWith(TEXT("Hutong")));
	}

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

void FHutongBuildingComponentCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const bool bAdvanced = ShowAdvanced();
	IDetailLayoutBuilder* Builder = &DetailBuilder;

	static const FName ViewCategory(TEXT("HutongView"));
	DetailBuilder.EditCategory(ViewCategory, NSLOCTEXT("HutongPanel", "ViewCategory", "View"))
	.AddCustomRow(NSLOCTEXT("HutongPanel", "ShowAdvancedRow", "Show advanced settings"))
	.WholeRowContent()
	[
		SNew(SCheckBox)
		.IsChecked(bAdvanced ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
		.OnCheckStateChanged_Lambda([Builder](ECheckBoxState State)
		{
			SetShowAdvanced(State == ECheckBoxState::Checked);
			Builder->ForceRefreshDetails();
		})
		.ToolTipText(NSLOCTEXT("HutongPanel", "ShowAdvancedTip", "Shows proportions, structure and detailing as well as the main choices; the same switch as the mode panel's."))
		[
			SNew(STextBlock).Text(NSLOCTEXT("HutongPanel", "ShowAdvanced", "Show advanced settings"))
		]
	];

	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	const UHutongBuildingComponent* First = Objects.Num() > 0 ? Cast<UHutongBuildingComponent>(Objects[0].Get()) : nullptr;
	const UClass* Class = First ? First->GetClass() : nullptr;

	// A type with no preset (built in or saved) has nothing to pick: the dropdown's only entry is empty.
	if (First && First->GetPresetOptions().Num() <= 1) DetailBuilder.HideCategory(TEXT("Preset"));

	if (!bAdvanced)
	{
		// Rows: the simple view's rule, at every level of every Hutong struct.
		for (TFieldIterator<FProperty> It(Class); Class && It; ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_Edit) || !IsOurs(*It)) continue;
			TArray<const FProperty*> Parents;
			HideForSimpleView(DetailBuilder, DetailBuilder.GetProperty(It->GetFName(), It->GetOwnerClass()), Parents);
		}
		// Categories holding no Hutong field (the component's engine tags, activation, cooking…).
		TArray<FName> Categories;
		DetailBuilder.GetCategoryNames(Categories);
		for (const FName& Name : Categories)
		{
			if (Name == ViewCategory) continue;
			TArray<TSharedRef<IPropertyHandle>> Props;
			DetailBuilder.EditCategory(Name).GetDefaultProperties(Props);
			if (Props.Num() > 0 && !Props.ContainsByPredicate([](const TSharedRef<IPropertyHandle>& H) { return IsOurs(H->GetProperty()); }))
			{
				DetailBuilder.HideCategory(Name);
			}
		}
	}

	// The tool's order: who and what first, the type's own settings in declaration order, the
	// placement's leftovers last; engine categories after everything.
	// A wall's openings are what a student places it for: straight after Metadata.
	const bool bWall = Class && Class->IsChildOf(UHutongWallBuildingComponent::StaticClass());
	DetailBuilder.SortCategories([bWall](const TMap<FName, IDetailCategoryBuilder*>& All)
	{
		static const TArray<FName> Leading = { TEXT("HutongView"), TEXT("Metadata"), TEXT("Preset") };
		static const TArray<FName> WallLeading = { TEXT("HutongView"), TEXT("Metadata"), TEXT("Openings"), TEXT("Preset") };
		const TArray<FName>& Front = bWall ? WallLeading : Leading;
		static const TArray<FName> Last = { TEXT("Footprint"), TEXT("Detail"), TEXT("Appearance"), TEXT("Identity") };
		TArray<TPair<FName, IDetailCategoryBuilder*>> Middle, Engine;
		for (const TPair<FName, IDetailCategoryBuilder*>& It : All)
		{
			if (Front.Contains(It.Key) || Last.Contains(It.Key)) continue;
			TArray<TSharedRef<IPropertyHandle>> Props;
			It.Value->GetDefaultProperties(Props);
			(Props.ContainsByPredicate([](const TSharedRef<IPropertyHandle>& H) { return IsOurs(H->GetProperty()); }) ? Middle : Engine).Add(It);
		}
		auto ByOrder = [](const TPair<FName, IDetailCategoryBuilder*>& A, const TPair<FName, IDetailCategoryBuilder*>& B)
		{
			return A.Value->GetSortOrder() < B.Value->GetSortOrder();
		};
		Middle.Sort(ByOrder);
		Engine.Sort(ByOrder);
		int32 Order = 0;
		for (const FName& Name : Front) if (IDetailCategoryBuilder* const* C = All.Find(Name)) (*C)->SetSortOrder(Order++);
		for (const TPair<FName, IDetailCategoryBuilder*>& It : Middle) It.Value->SetSortOrder(Order++);
		for (const FName& Name : Last) if (IDetailCategoryBuilder* const* C = All.Find(Name)) (*C)->SetSortOrder(Order++);
		for (const TPair<FName, IDetailCategoryBuilder*>& It : Engine) It.Value->SetSortOrder(Order++);
	});
}

void HutongPanelCustomizations::Register()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditor.RegisterCustomClassLayout(TEXT("HutongBuildingComponent"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHutongBuildingComponentCustomization::Make));
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
	PropertyEditor->UnregisterCustomClassLayout(TEXT("HutongBuildingComponent"));
}
