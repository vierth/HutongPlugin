#include "Tools/HutongContextMenu.h"
#include "Tools/HutongDetailOps.h"
#include "Tools/HutongPresets.h"
#include "Tools/HutongPanelFilter.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongMetadata.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "HutongContextMenu"

namespace HutongContextMenu
{
namespace
{
	using HutongDetailOps::FConvertTarget;

	const FName OwnerName("HutongLayoutContextMenu");

	FUIAction Disabled()
	{
		return FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() { return false; }));
	}

	FUIAction Radio(TFunction<void()> Execute, bool bChecked)
	{
		FUIAction Action(FExecuteAction::CreateLambda(MoveTemp(Execute)));
		Action.GetActionCheckState = FGetActionCheckState::CreateLambda([bChecked]() { return bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; });
		return Action;
	}

	// ---- A field reached from the component through nested structs ----

	struct FField
	{
		TArray<const FProperty*> Chain;	// from the component's own property down to the field
		FText Label;
	};

	void* ValueOf(UHutongBuildingComponent* B, const FField& F)
	{
		void* Data = B;
		for (const FProperty* P : F.Chain) Data = P->ContainerPtrToValuePtr<void>(Data);
		return Data;
	}

	// The marked basics (HutongBasic) that are switches or choices, nested params walked.
	void CollectOptions(const UStruct* Struct, const TArray<const FProperty*>& Chain, const FString& Prefix, int32 Depth, TArray<FField>& Out)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* P = *It;
			if (!P->HasAnyPropertyFlags(CPF_Edit)) continue;
			// Fields the enclosing struct says do not apply here (an ear room's verandas).
			if (Chain.Num() > 0 && HutongPanel::IsHiddenBy(*P, Chain.Last())) continue;
			TArray<const FProperty*> Next = Chain;
			Next.Add(P);
			const FString Name = Prefix + P->GetDisplayNameText().ToString();
			// Only what the panel's simple view shows: a nested group opens only when marked basic, so
			// an ear room's closing and outer walls do not offer a wall's choices.
			if (!HutongPanel::PassesSimple(*P)) continue;
			if (const FStructProperty* S = CastField<FStructProperty>(P))
			{
				if (Depth < 2) CollectOptions(S->Struct, Next, Name + TEXT(" › "), Depth + 1, Out);
				continue;
			}
			if (P->IsA<FBoolProperty>() || P->IsA<FEnumProperty>() || (CastField<FByteProperty>(P) && CastField<FByteProperty>(P)->Enum))
			{
				Out.Add({ Next, FText::FromString(Name) });
			}
		}
	}

	const UEnum* EnumOf(const FProperty* P)
	{
		if (const FEnumProperty* E = CastField<FEnumProperty>(P)) return E->GetEnum();
		if (const FByteProperty* Byte = CastField<FByteProperty>(P)) return Byte->Enum;
		return nullptr;
	}

	int64 GetEnumValue(const FProperty* P, const void* Value)
	{
		if (const FEnumProperty* E = CastField<FEnumProperty>(P)) return E->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value);
		return CastField<FByteProperty>(P)->GetPropertyValue(Value);
	}

	void SetEnumValue(const FProperty* P, void* Value, int64 New)
	{
		if (const FEnumProperty* E = CastField<FEnumProperty>(P)) E->GetUnderlyingProperty()->SetIntPropertyValue(Value, New);
		else CastField<FByteProperty>(P)->SetPropertyValue(Value, (uint8)New);
	}

	// Sets the field on every one of the class, through the component's own edit hook (clamps, rebuild).
	void SetField(const TArray<UHutongBuildingComponent*>& Buildings, const FField& F, TFunctionRef<void(const FProperty*, void*)> Write)
	{
		const FScopedTransaction Transaction(FText::Format(LOCTEXT("SetField", "Set {0}"), F.Label));
		// A field of the base (metadata) is on every building; a Params field only on its own class.
		const UClass* Owner = F.Chain[0]->GetOwnerClass();
		for (UHutongBuildingComponent* B : Buildings)
		{
			if (!Owner || !B->GetClass()->IsChildOf(Owner)) continue;
			if (B->GetOwner()) B->GetOwner()->Modify();
			B->Modify();
			Write(F.Chain.Last(), ValueOf(B, F));
			FPropertyChangedEvent Event(const_cast<FProperty*>(F.Chain[0]), EPropertyChangeType::ValueSet);
			B->PostEditChangeProperty(Event);
			B->ApplyPlacementAttachments();
		}
	}

	// A choice field as a submenu of its values, the first building's checked.
	void AddEnumMenu(FMenuBuilder& Menu, const TArray<UHutongBuildingComponent*>& Buildings, const FField& F)
	{
		const UEnum* Enum = EnumOf(F.Chain.Last());
		if (!Enum) return;
		const int64 Current = GetEnumValue(F.Chain.Last(), ValueOf(Buildings[0], F));
		Menu.AddSubMenu(F.Label, FText::GetEmpty(), FNewMenuDelegate::CreateLambda([Buildings, F, Enum, Current](FMenuBuilder& Sub)
		{
			for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
			{
				if (Enum->HasMetaData(TEXT("Hidden"), i)) continue;
				const int64 Value = Enum->GetValueByIndex(i);
				Sub.AddMenuEntry(Enum->GetDisplayNameTextByIndex(i), Enum->GetToolTipTextByIndex(i), FSlateIcon(),
					Radio([Buildings, F, Value]() { SetField(Buildings, F, [Value](const FProperty* P, void* V) { SetEnumValue(P, V, Value); }); }, Value == Current),
					NAME_None, EUserInterfaceActionType::RadioButton);
			}
		}));
	}

	TArray<FField> OptionsOf(const UHutongBuildingComponent* Building)
	{
		TArray<FField> Out;
		if (const FStructProperty* Params = CastField<FStructProperty>(Building->GetClass()->FindPropertyByName(TEXT("Params"))))
		{
			CollectOptions(Params->Struct, { Params }, FString(), 0, Out);
		}
		return Out;
	}

	FField ComponentField(FName Name)
	{
		const FProperty* P = UHutongBuildingComponent::StaticClass()->FindPropertyByName(Name);
		return { { P }, P ? P->GetDisplayNameText() : FText::FromName(Name) };
	}

	// ---- Notes, in a small window of their own ----

	void EditNotes(const TArray<UHutongBuildingComponent*>& Buildings)
	{
		TSharedPtr<SMultiLineEditableTextBox> Box;
		TSharedRef<SWindow> Window = SNew(SWindow)
			.Title(LOCTEXT("NotesTitle", "Notes"))
			.ClientSize(FVector2D(480.0, 240.0))
			.SupportsMinimize(false).SupportsMaximize(false);
		bool bSave = false;
		Window->SetContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f)
			[
				SAssignNew(Box, SMultiLineEditableTextBox).Text(FText::FromString(Buildings[0]->Notes)).AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f)
				[
					SNew(SButton).Text(LOCTEXT("NotesSave", "Save")).OnClicked_Lambda([&bSave, Window]() { bSave = true; Window->RequestDestroyWindow(); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(LOCTEXT("NotesCancel", "Cancel")).OnClicked_Lambda([Window]() { Window->RequestDestroyWindow(); return FReply::Handled(); })
				]
			]);
		FSlateApplication::Get().AddModalWindow(Window, FSlateApplication::Get().GetActiveTopLevelWindow());
		if (!bSave || !Box.IsValid()) return;

		const FString Text = Box->GetText().ToString();
		const FScopedTransaction Transaction(LOCTEXT("EditNotes", "Edit Building Notes"));
		for (UHutongBuildingComponent* B : Buildings)
		{
			B->Modify();
			B->Notes = Text;
		}
	}

	// ---- The section ----

	void FillSection(UToolMenu* Menu)
	{
		const TArray<UHutongBuildingComponent*> Buildings = HutongDetailOps::CollectSelected();
		if (Buildings.Num() == 0) return;
		const UHutongBuildingComponent* First = Buildings[0];
		const FText Heading = Buildings.Num() == 1
			? FText::Format(LOCTEXT("HeadingOne", "Hutong: {0}{1}"), First->GetTypeLabel(),
				First->Preset.IsEmpty() ? FText::GetEmpty() : FText::FromString(TEXT("  ·  ") + First->Preset))
			: FText::Format(LOCTEXT("HeadingMany", "Hutong: {0} buildings"), FText::AsNumber(Buildings.Num()));
		FToolMenuSection& Section = Menu->AddSection(TEXT("HutongBuilding"), Heading, FToolMenuInsert(NAME_None, EToolMenuInsertType::First));
		const bool bOneClass = !Buildings.ContainsByPredicate([First](const UHutongBuildingComponent* B) { return B->GetClass() != First->GetClass(); });

		auto Sub = [&Section](FName Name, const FText& Label, const FText& Tip, TFunction<void(FMenuBuilder&)> Fill)
		{
			Section.AddSubMenu(Name, Label, Tip, FNewToolMenuChoice(FNewMenuDelegate::CreateLambda([Fill](FMenuBuilder& M) { Fill(M); })));
		};
		auto Entry = [&Section](FName Name, const FText& Label, const FText& Tip, TFunction<void()> Execute, TFunction<bool()> CanExecute = nullptr)
		{
			Section.AddMenuEntry(Name, Label, Tip, FSlateIcon(), FUIAction(FExecuteAction::CreateLambda(MoveTemp(Execute)),
				CanExecute ? FCanExecuteAction::CreateLambda(MoveTemp(CanExecute)) : FCanExecuteAction()));
		};

		Sub(TEXT("HutongType"), LOCTEXT("Type", "Building Type"), LOCTEXT("TypeTip", "Turn the building into another type of its kind, as if drawn here with it."),
			[Buildings](FMenuBuilder& M) { FillTypeMenu(M, Buildings, &ChangeSelectionAsked); });
		Sub(TEXT("HutongPreset"), LOCTEXT("Preset", "Preset"), LOCTEXT("PresetTip", "Another preset of the building's type, as if drawn here with it."),
			[Buildings](FMenuBuilder& M) { FillPresetMenu(M, Buildings, &ChangeSelectionAsked); });

		// Switches and choices the panel marks as basic, for one type at a time.
		TArray<FField> Options;
		if (bOneClass) Options = OptionsOf(First);
		if (Options.Num() > 0)
		{
			Sub(TEXT("HutongOptions"), LOCTEXT("Options", "Options"), LOCTEXT("OptionsTip", "The building's main switches and choices."),
				[Buildings, Options](FMenuBuilder& M)
				{
					for (const FField& F : Options)
					{
						if (EnumOf(F.Chain.Last()))
						{
							AddEnumMenu(M, Buildings, F);
							continue;
						}
						const FBoolProperty* Bool = CastField<FBoolProperty>(F.Chain.Last());
						const bool bOn = Bool->GetPropertyValue(ValueOf(Buildings[0], F));
						FUIAction Action(FExecuteAction::CreateLambda([Buildings, F, bOn]()
						{
							SetField(Buildings, F, [bOn](const FProperty* P, void* V) { CastField<FBoolProperty>(P)->SetPropertyValue(V, !bOn); });
						}));
						Action.GetActionCheckState = FGetActionCheckState::CreateLambda([bOn]() { return bOn ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; });
						M.AddMenuEntry(F.Label, F.Chain.Last()->GetToolTipText(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::ToggleButton);
					}
				});
		}

		// Facade and bays: what F, Shift+[ ] and [ ] do in a tool.
		EHutongBaySide Side;
		if (Buildings.ContainsByPredicate([&Side](const UHutongBuildingComponent* B) { return B->GetFacade(Side); }))
		{
			Sub(TEXT("HutongFacade"), LOCTEXT("Facade", "Facade"), LOCTEXT("FacadeTip", "Which side faces the court or lane."),
				[Buildings](FMenuBuilder& M)
				{
					M.AddMenuEntry(LOCTEXT("Flip", "Flip To The Other Side (F)"), FText::GetEmpty(), FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([Buildings]() { HutongDetailOps::FlipFacing(Buildings); })));
					// A step on in −Y, +X, +Y, −X is clockwise seen from above.
					M.AddMenuEntry(LOCTEXT("TurnRight", "Rotate Clockwise (Shift+])"), FText::GetEmpty(), FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([Buildings]() { HutongDetailOps::TurnFacing(Buildings, 1); })));
					M.AddMenuEntry(LOCTEXT("TurnLeft", "Rotate Counterclockwise (Shift+[)"), FText::GetEmpty(), FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda([Buildings]() { HutongDetailOps::TurnFacing(Buildings, -1); })));
				});
		}
		if (Buildings.ContainsByPredicate([](const UHutongBuildingComponent* B) { return HutongDetailOps::GetBayCountOverride(B) != INDEX_NONE; }))
		{
			Entry(TEXT("HutongBayMore"), LOCTEXT("BayMore", "One Bay More (])"), FText::GetEmpty(), [Buildings]() { HutongDetailOps::AdjustBays(Buildings, 1); });
			Entry(TEXT("HutongBayFewer"), LOCTEXT("BayFewer", "One Bay Fewer ([)"), FText::GetEmpty(), [Buildings]() { HutongDetailOps::AdjustBays(Buildings, -1); });
		}
		if (Buildings.ContainsByPredicate([](const UHutongBuildingComponent* B) { return B->IsA<UHutongWallBuildingComponent>(); }))
		{
			Entry(TEXT("HutongGate"), LOCTEXT("Gate", "Gate In Or Out (G)"), LOCTEXT("GateTip", "Takes the wall's opening out, or puts a gate (牆垣門) in."),
				[Buildings]() { HutongDetailOps::ToggleGate(Buildings); });
		}

		// Metadata: what the map says and what the student thinks of it.
		Sub(TEXT("HutongMetadata"), LOCTEXT("Metadata", "Metadata"), LOCTEXT("MetadataTip", "Confidence and notes."),
			[Buildings](FMenuBuilder& M)
			{
				AddEnumMenu(M, Buildings, ComponentField(GET_MEMBER_NAME_CHECKED(UHutongBuildingComponent, Confidence)));
				M.AddMenuEntry(LOCTEXT("Notes", "Edit Notes…"), FText::GetEmpty(), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([Buildings]() { EditNotes(Buildings); })));
			});

		// How much is built.
		Sub(TEXT("HutongDetail"), LOCTEXT("Detail", "Detail Level"), LOCTEXT("DetailTip", "How much of the building's geometry is built, or only its plan."),
			[Buildings](FMenuBuilder& M)
			{
				const UEnum* Enum = StaticEnum<EHutongDetail>();
				for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
				{
					if (Enum->HasMetaData(TEXT("Hidden"), i)) continue;
					const EHutongDetail Level = (EHutongDetail)Enum->GetValueByIndex(i);
					M.AddMenuEntry(Enum->GetDisplayNameTextByIndex(i), FText::GetEmpty(), FSlateIcon(),
						Radio([Buildings, Level]() { HutongDetailOps::SetLevel(Buildings, Level); }, Buildings[0]->DetailLevel == Level),
						NAME_None, EUserInterfaceActionType::RadioButton);
				}
				M.AddSeparator();
				M.AddMenuEntry(LOCTEXT("Generate", "Generate Geometry"), FText::GetEmpty(), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([Buildings]() { HutongDetailOps::GeneratePlanned(Buildings); })));
				M.AddMenuEntry(LOCTEXT("Revert", "Revert To Plan Only"), FText::GetEmpty(), FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([Buildings]() { HutongDetailOps::RevertToPlan(Buildings); })));
			});

		Entry(TEXT("HutongAll"), LOCTEXT("All", "All Parameters…"), LOCTEXT("AllTip", "Every parameter of the building in a window of its own."),
			[Buildings]()
			{
				TArray<UObject*> Objects(Buildings);
				FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateFloatingDetailsView(Objects, /*bIsLockable*/ false);
			});
	}
}

void FillPresetMenu(FMenuBuilder& Menu, const TArray<UHutongBuildingComponent*>& Buildings, FOnPick OnPick)
{
	if (Buildings.Num() == 0) return;
	const FName Key = Buildings[0]->GetPresetKey();
	const bool bMixed = Buildings.ContainsByPredicate([Key](const UHutongBuildingComponent* B) { return B->GetPresetKey() != Key; });
	const FString Own = Buildings[0]->Preset;
	Menu.BeginSection(NAME_None, FText::Format(LOCTEXT("PresetsOf", "Presets: {0}"), Buildings[0]->GetTypeLabel()));
	const TArray<FString> Names = bMixed ? TArray<FString>() : UHutongPresetLibrary::Get()->GetPresetNames(Key);
	for (const FString& Name : Names)
	{
		Menu.AddMenuEntry(FText::FromString(Name), FText::GetEmpty(), FSlateIcon(),
			Radio([OnPick, Name]() { OnPick(INDEX_NONE, Name); }, Name == Own), NAME_None, EUserInterfaceActionType::RadioButton);
	}
	if (Names.Num() == 0)
	{
		Menu.AddMenuEntry(bMixed ? LOCTEXT("Mixed", "The selection mixes types: pick one type first") : LOCTEXT("NoPresets", "This type has no presets"),
			FText::GetEmpty(), FSlateIcon(), Disabled());
	}
	Menu.EndSection();
}

void FillTypeMenu(FMenuBuilder& Menu, const TArray<UHutongBuildingComponent*>& Buildings, FOnPick OnPick)
{
	if (Buildings.Num() == 0) return;
	const TArray<FConvertTarget>& Targets = HutongDetailOps::ConvertTargets();
	const int32 Own = HutongDetailOps::FindConvertTargetIndex(Buildings[0]);
	const FString OwnPreset = Buildings[0]->Preset;
	int32 Group = INDEX_NONE;
	for (int32 i = 0; i < Targets.Num(); ++i)
	{
		const FConvertTarget& T = Targets[i];
		if (!HutongDetailOps::CanAllBecome(Buildings, T)) continue;
		if (T.Group != Group)
		{
			if (Group != INDEX_NONE) Menu.EndSection();
			Group = T.Group;
			Menu.BeginSection(NAME_None, HutongDetailOps::ConvertGroupName(Group));
		}
		const TArray<FString> Names = UHutongPresetLibrary::Get()->GetPresetNames(HutongDetailOps::PresetKeyOf(T));
		const bool bOwnType = i == Own;
		if (Names.Num() == 0)
		{
			Menu.AddMenuEntry(FText::FromString(T.Label), FText::GetEmpty(), FSlateIcon(),
				Radio([OnPick, i]() { OnPick(i, FString()); }, bOwnType), NAME_None, EUserInterfaceActionType::RadioButton);
			continue;
		}
		// By value: a submenu is built after this returns.
		Menu.AddSubMenu(FText::FromString(T.Label), FText::GetEmpty(),
			FNewMenuDelegate::CreateLambda([Names, i, bOwnType, OwnPreset, OnPick](FMenuBuilder& Sub)
			{
				for (const FString& Name : Names)
				{
					Sub.AddMenuEntry(FText::FromString(Name), FText::GetEmpty(), FSlateIcon(),
						Radio([OnPick, i, Name]() { OnPick(i, Name); }, bOwnType && Name == OwnPreset), NAME_None, EUserInterfaceActionType::RadioButton);
				}
			}),
			Radio([]() {}, bOwnType), NAME_None, EUserInterfaceActionType::RadioButton);
	}
	if (Group != INDEX_NONE) Menu.EndSection();
	else Menu.AddMenuEntry(LOCTEXT("NoCommonType", "The selected buildings cannot become one type"), FText::GetEmpty(), FSlateIcon(), Disabled());
}

TArray<FString> OptionLabels(const UHutongBuildingComponent* Building)
{
	TArray<FString> Out;
	if (!Building) return Out;
	for (const FField& F : OptionsOf(Building)) Out.Add(F.Label.ToString());
	return Out;
}

void ChangeSelectionAsked(int32 TypeIndex, const FString& Preset)
{
	const TArray<UHutongBuildingComponent*> Work = HutongDetailOps::NeedingChange(HutongDetailOps::CollectSelected(), TypeIndex, Preset);
	if (Work.Num() == 0) return;
	const TArray<FString> Lost = HutongDetailOps::CustomizedAcross(Work);
	if (Lost.Num() > 0)
	{
		const FText Message = FText::Format(LOCTEXT("ConfirmLose",
			"This replaces {0} value(s) changed from what a drag here gives:\n\n{1}\n\nApply anyway?"),
			FText::AsNumber(Lost.Num()), FText::FromString(FString::Join(Lost, TEXT("\n"))));
		if (FMessageDialog::Open(EAppMsgType::OkCancel, Message, LOCTEXT("ConfirmTitle", "Replace Changed Values?")) != EAppReturnType::Ok) return;
	}
	HutongDetailOps::ChangeTypeOrPreset(Work, TypeIndex, Preset);
}

void Register()
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		FToolMenuOwnerScoped Owner(OwnerName);
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.ActorContextMenu");
		// First in the menu: the building's own choices before the editor's generic ones.
		Menu->AddDynamicSection(TEXT("HutongBuildingDynamic"), FNewToolMenuDelegate::CreateStatic(&FillSection),
			FToolMenuInsert(NAME_None, EToolMenuInsertType::First));
	}));
}

void Unregister()
{
	if (UToolMenus* Menus = UToolMenus::TryGet()) Menus->UnregisterOwnerByName(OwnerName);
}
}

#undef LOCTEXT_NAMESPACE
