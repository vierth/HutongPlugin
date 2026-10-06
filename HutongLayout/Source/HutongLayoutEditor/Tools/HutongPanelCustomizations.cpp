#include "HutongPanelCustomizations.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "HutongLayoutEdMode.h"
#include "HutongLayoutModeSettings.h"
#include "HutongPresets.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "HutongPluginStamp.h"
#include "IDetailPropertyRow.h"
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

FText HutongPanelCustomizations::PluginUpdatedText()
{
	// The last commit to change the plugin, written into the source by .githooks/pre-commit: copying,
	// checking out or rebuilding cannot move it.
	const FDateTime Utc = FDateTime::FromUnixTimestamp(HUTONG_PLUGIN_UPDATED_UTC);
	const FText When = FText::FromString((Utc + (FDateTime::Now() - FDateTime::UtcNow())).ToString(TEXT("%Y-%m-%d %H:%M")));
	return FText::Format(NSLOCTEXT("HutongScene", "Version", "Plugin last updated {0}"), When);
}

void FHutongModeSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1) return;
	const TWeakObjectPtr<UHutongLayoutModeSettings> Settings = Cast<UHutongLayoutModeSettings>(Objects[0].Get());
	if (!Settings.IsValid()) return;

	using FCan = bool (UHutongLayoutModeSettings::*)() const;
	using FDo = void (UHutongLayoutModeSettings::*)();
	using S = UHutongLayoutModeSettings;

	auto Button = [Settings](const FText& Label, const FText& Tip, FDo Do, FCan Can)
	{
		return SNew(SButton)
			.Text(Label)
			.ToolTipText(Tip)
			.HAlign(HAlign_Center)
			.IsEnabled_Lambda([Settings, Can] { return Settings.IsValid() && (!Can || (Settings.Get()->*Can)()); })
			.OnClicked_Lambda([Settings, Do]
			{
				if (UHutongLayoutModeSettings* Obj = Settings.Get()) (Obj->*Do)();
				return FReply::Handled();
			});
	};
	// One action per line, its description on hover: written beside it, it wrapped a word a line in a
	// narrow panel.
	auto Action = [&](IDetailCategoryBuilder& Category, const FText& Label, const FText& Tip, FDo Do, FCan Can = nullptr)
	{
		Category.AddCustomRow(Label)
		.WholeRowContent()
		.HAlign(HAlign_Left)
		[
			SNew(SBox).WidthOverride(190.0f).Padding(FMargin(0.0f, 2.0f))[Button(Label, Tip, Do, Can)]
		];
	};
	// A switch: the box, then its whole label (in the name column the label was cut to leave the box half the row).
	auto Property = [&](IDetailCategoryBuilder& Category, FName Name) -> IDetailPropertyRow*
	{
		const TSharedRef<IPropertyHandle> Handle = DetailBuilder.GetProperty(Name);
		if (!Handle->IsValidHandle()) return nullptr;
		IDetailPropertyRow& Row = Category.AddProperty(Handle);
		if (CastField<FBoolProperty>(Handle->GetProperty()))
		{
			Row.CustomWidget()
			.WholeRowContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)[Handle->CreatePropertyValueWidget()]
				+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[Handle->CreatePropertyNameWidget()]
			];
		}
		return &Row;
	};
	// A setting with an Apply button, lit while applying it would change something.
	auto WithApply = [&](IDetailCategoryBuilder& Category, FName Name, const FText& Tip, FDo Do, FCan Can)
	{
		const TSharedRef<IPropertyHandle> Handle = DetailBuilder.GetProperty(Name);
		if (!Handle->IsValidHandle()) return;
		Category.AddProperty(Handle).CustomWidget()
		.NameContent()
		[
			Handle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(220.0f)
		.MaxDesiredWidth(400.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[Handle->CreatePropertyValueWidget()]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f).VAlign(VAlign_Center)
			[
				Button(NSLOCTEXT("HutongScene", "Apply", "Apply"), Tip, Do, Can)
			]
		];
	};

	int32 Order = 0;
	auto Category = [&](const TCHAR* Name) -> IDetailCategoryBuilder&
	{
		IDetailCategoryBuilder& C = DetailBuilder.EditCategory(Name);
		C.SetSortOrder(Order++);
		return C;
	};

	IDetailCategoryBuilder& Geometry = Category(TEXT("Geometry"));
	Property(Geometry, GET_MEMBER_NAME_CHECKED(S, bPlanOnly));
	Property(Geometry, GET_MEMBER_NAME_CHECKED(S, SizeStep));
	Property(Geometry, GET_MEMBER_NAME_CHECKED(S, bShowPlanOutlines));
	Property(Geometry, GET_MEMBER_NAME_CHECKED(S, bShowBuildingLabels));
	Property(Geometry, GET_MEMBER_NAME_CHECKED(S, bPlansOverBuildings));
	Action(Geometry, NSLOCTEXT("HutongScene", "GenLoaded", "Generate Loaded"),
		NSLOCTEXT("HutongScene", "GenLoadedWhat", "Build geometry for every loaded outline-only building."), &S::GenerateLoadedGeometry);
	Action(Geometry, NSLOCTEXT("HutongScene", "GenSelected", "Generate Selected"),
		NSLOCTEXT("HutongScene", "GenSelectedWhat", "Build geometry for the selected outline-only buildings."), &S::GenerateSelectedGeometry, &S::HasSelectedLayout);
	Action(Geometry, NSLOCTEXT("HutongScene", "RevLoaded", "Revert Loaded"),
		NSLOCTEXT("HutongScene", "RevLoadedWhat", "Remove geometry from every loaded building, keeping its outline."), &S::RevertLoadedToLayout);
	Action(Geometry, NSLOCTEXT("HutongScene", "RevSelected", "Revert Selected"),
		NSLOCTEXT("HutongScene", "RevSelectedWhat", "Remove geometry from the selected buildings, keeping their outlines."), &S::RevertSelectedToLayout, &S::HasSelectedBuilt);

	IDetailCategoryBuilder& Export = Category(TEXT("Export"));
	Action(Export, NSLOCTEXT("HutongScene", "ExportAll", "Export All"),
		NSLOCTEXT("HutongScene", "ExportAllWhat", "Save every building in the level to a new file."), &S::ExportAll);
	Action(Export, NSLOCTEXT("HutongScene", "ExportSel", "Export Selection"),
		NSLOCTEXT("HutongScene", "ExportSelWhat", "Save the selected buildings to a new file."), &S::ExportSelection, &S::HasSelection);

	IDetailCategoryBuilder& Import = Category(TEXT("Import"));
	Action(Import, NSLOCTEXT("HutongScene", "ImportLayout", "Import Layout"),
		NSLOCTEXT("HutongScene", "ImportLayoutWhat", "Place the buildings from a layout file."), &S::ImportLayout);
	Property(Import, GET_MEMBER_NAME_CHECKED(S, bCustomizePlacement));
	Property(Import, GET_MEMBER_NAME_CHECKED(S, bUpdateMatchingPlacements));
	Property(Import, GET_MEMBER_NAME_CHECKED(S, ImportFolder));

	IDetailCategoryBuilder& Housekeeping = Category(TEXT("Housekeeping"));
	Action(Housekeeping, NSLOCTEXT("HutongScene", "RebuildLoaded", "Rebuild Loaded"),
		NSLOCTEXT("HutongScene", "RebuildLoadedWhat", "Rebuild every loaded building from its settings."), &S::RebuildLoaded);
	Action(Housekeeping, NSLOCTEXT("HutongScene", "RebuildSel", "Rebuild Selected"),
		NSLOCTEXT("HutongScene", "RebuildSelWhat", "Rebuild the selected buildings from their settings."), &S::RebuildSelection, &S::HasSelection);
	Action(Housekeeping, NSLOCTEXT("HutongScene", "Overlaps", "Find Overlapping Buildings"),
		NSLOCTEXT("HutongScene", "OverlapsWhat", "List loaded buildings that sit mostly on another's footprint."), &S::FindOverlappingBuildings);
	Action(Housekeeping, NSLOCTEXT("HutongScene", "Starter", "Create Starter Materials"),
		NSLOCTEXT("HutongScene", "StarterWhat", "Create an editable material for each surface in /Game/HutongLayout/Materials."), &S::CreateStarterMaterials);
	Action(Housekeeping, NSLOCTEXT("HutongScene", "Unused", "Delete Unused Meshes"),
		NSLOCTEXT("HutongScene", "UnusedWhat", "Delete generated meshes no building uses."), &S::DeleteUnusedGeneratedMeshes);

	// Only with something selected: it acts on nothing else.
	IDetailCategoryBuilder& Selection = Category(TEXT("Selection"));
	if (Settings->HasSelection())
	{
		WithApply(Selection, GET_MEMBER_NAME_CHECKED(S, TargetLevel),
			NSLOCTEXT("HutongScene", "ApplyLevelTip", "Rebuild the selected buildings at this detail level."),
			&S::ApplyDetailLevel, &S::CanApplyDetailLevel);
		// One Apply for the pair, on the second: it takes both.
		Property(Selection, GET_MEMBER_NAME_CHECKED(S, ConvertTo));
		WithApply(Selection, GET_MEMBER_NAME_CHECKED(S, ConvertPreset),
			NSLOCTEXT("HutongScene", "ApplyConvertTip", "Change the selected buildings to this type and preset."),
			&S::ApplyConvert, &S::CanApplyConvert);
		WithApply(Selection, GET_MEMBER_NAME_CHECKED(S, DivideAtBayLine),
			NSLOCTEXT("HutongScene", "ApplyDivideTip", "Divide each selected building at this bay line."),
			&S::ApplyDivide, &S::CanApplyDivide);
		Action(Selection, NSLOCTEXT("HutongScene", "Fuse", "Fuse Two Selected"),
			NSLOCTEXT("HutongScene", "FuseWhat", "Join two selected buildings standing end to end."), &S::FuseSelection, &S::CanFuse);
	}
	else
	{
		DetailBuilder.HideCategory(TEXT("Selection"));
	}

	IDetailCategoryBuilder& Information = Category(TEXT("Information"));
	Property(Information, GET_MEMBER_NAME_CHECKED(S, LoadedBuildings));
	Property(Information, GET_MEMBER_NAME_CHECKED(S, LoadedTriangles));
	Action(Information, NSLOCTEXT("HutongScene", "Count", "Count Loaded Buildings"),
		NSLOCTEXT("HutongScene", "CountWhat", "Count the loaded buildings and their triangles."), &S::RefreshCounts);
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
	PropertyEditor.RegisterCustomClassLayout(TEXT("HutongLayoutModeSettings"),
		FOnGetDetailCustomizationInstance::CreateStatic(&FHutongModeSettingsCustomization::Make));
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
	PropertyEditor->UnregisterCustomClassLayout(TEXT("HutongLayoutModeSettings"));
	PropertyEditor->UnregisterCustomClassLayout(TEXT("HutongBuildingComponent"));
}
