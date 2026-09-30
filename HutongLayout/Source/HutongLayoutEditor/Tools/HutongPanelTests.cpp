#include "Misc/AutomationTest.h"
#include "Tools/WallTool.h"
#include "Tools/SiheyuanTool.h"
#include "Tools/StreetRowTool.h"
#include "Tools/HutongPresets.h"
#include "Tools/HutongPresetDefaults.h"
#include "Tools/HutongPanelFilter.h"
#include "Tools/CompoundTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IPropertyRowGenerator.h"
#include "IDetailTreeNode.h"
#include "PropertyHandle.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	void GatherRows(const TSharedRef<IDetailTreeNode>& Node, TSet<FName>& Out)
	{
		if (const TSharedPtr<IPropertyHandle> Handle = Node->CreatePropertyHandle())
		{
			if (Handle->IsValidHandle() && Handle->GetProperty()) Out.Add(Handle->GetProperty()->GetFName());
		}
		TArray<TSharedRef<IDetailTreeNode>> Children;
		Node->GetChildren(Children);
		for (const TSharedRef<IDetailTreeNode>& Child : Children) GatherRows(Child, Out);
	}

	// Rows a details panel builds for this object, customizations included (same QueryCustomDetailLayout pass as the mode panel).
	UHutongPresetProperties* PickerOf(UInteractiveTool* Tool)
	{
		for (UObject* Set : Tool->GetToolProperties())
		{
			if (UHutongPresetProperties* Picker = Cast<UHutongPresetProperties>(Set)) return Picker;
		}
		return nullptr;
	}

	TSet<FName> RowsFor(UObject* Object)
	{
		FPropertyEditorModule& PropertyEditor =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FPropertyRowGeneratorArgs Args;
		Args.bAllowMultipleTopLevelObjects = true;
		const TSharedPtr<IPropertyRowGenerator> Generator = PropertyEditor.CreatePropertyRowGenerator(Args);
		Generator->SetObjects({ Object });

		TSet<FName> Rows;
		for (const TSharedRef<IDetailTreeNode>& Root : Generator->GetRootTreeNodes()) GatherRows(Root, Rows);
		return Rows;
	}
}

// Wall role is chosen by tool, not picker. A customization on the wrong class fails silently and the
// dropdown returns (as it did through the split), so this asks the panel, not the table.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallRolePanelTest, "HutongLayout.Walls.RoleNotOffered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallRolePanelTest::RunTest(const FString& Parameters)
{
	const TSet<FName> Lane = RowsFor(NewObject<UHutongLaneWallToolProperties>());
	const TSet<FName> Court = RowsFor(NewObject<UHutongCourtWallToolProperties>());

	// Params reached, else "no Role row" holds for an empty panel.
	TestTrue(TEXT("院牆 panel shows the wall's parameters"), Lane.Contains(TEXT("Height")));
	TestTrue(TEXT("隔牆 panel shows the wall's parameters"), Court.Contains(TEXT("Height")));

	TestFalse(TEXT("院牆 tool does not offer the role"), Lane.Contains(TEXT("Role")));
	TestFalse(TEXT("隔牆 tool does not offer the role"), Court.Contains(TEXT("Role")));

	// Placed wall keeps Role: retyping a run is a Details edit.
	const TSet<FName> Placed = RowsFor(NewObject<UHutongWallBuildingComponent>());
	TestTrue(TEXT("a placed wall keeps its role"), Placed.Contains(TEXT("Role")));
	return true;
}

// Setup/Shutdown are all a tool does with its property cache and need no tool manager, so the cache
// is driven as the editor does: tool up, preset picked, tool closed, next tool up.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongPresetPickerCacheTest, "HutongLayout.Panel.PresetPickerPerTool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongPresetPickerCacheTest::RunTest(const FString& Parameters)
{
	HutongPresets::RegisterBuiltInPresets();

	// Street row lists the house presets but opens on its own default, not the house's choice.
	UHutongSiheyuanTool* House = NewObject<UHutongSiheyuanTool>();
	House->Setup();
	UHutongPresetProperties* HousePicker = PickerOf(House);
	if (!TestNotNull(TEXT("house tool has a preset picker"), HousePicker)) return false;
	TestEqual(TEXT("house tool opens on the default preset"), HousePicker->Preset, HutongPresets::DefaultSiheyuanName());

	HousePicker->Preset = TEXT("Main Hall (正房)");
	House->Shutdown(EToolShutdownType::Completed);

	UHutongStreetRowTool* Row = NewObject<UHutongStreetRowTool>();
	Row->Setup();
	UHutongPresetProperties* RowPicker = PickerOf(Row);
	if (!TestNotNull(TEXT("street row has a preset picker"), RowPicker)) return false;
	TestEqual(TEXT("street row opens on its own default, not the house's choice"), RowPicker->Preset, HutongPresets::DefaultStreetRowHouseName());

	// Row Position picks the houses' preset: rear row loads 後罩房, front row 倒座房 again.
	UHutongStreetRowToolProperties* RowSettings = nullptr;
	for (UObject* Set : Row->GetToolProperties(false)) if (!RowSettings) RowSettings = Cast<UHutongStreetRowToolProperties>(Set);
	if (TestNotNull(TEXT("street row has its settings"), RowSettings))
	{
		FProperty* PositionProp = UHutongStreetRowToolProperties::StaticClass()->FindPropertyByName(
			GET_MEMBER_NAME_CHECKED(UHutongStreetRowToolProperties, Position));
		RowSettings->Position = EHutongStreetRowPosition::Rear;
		Row->OnPropertyModified(RowSettings, PositionProp);
		TestEqual(TEXT("rear row loads the rear-row preset"), RowPicker->Preset, FString(TEXT("Rear Row (後罩房)")));
		RowSettings->Position = EHutongStreetRowPosition::Front;
		Row->OnPropertyModified(RowSettings, PositionProp);
		TestEqual(TEXT("front row loads the front-row preset"), RowPicker->Preset, HutongPresets::DefaultStreetRowHouseName());
	}

	RowPicker->Preset = TEXT("Front Row (倒座房)");
	Row->Shutdown(EToolShutdownType::Completed);

	// House reopens with its own choice.
	UHutongSiheyuanTool* HouseAgain = NewObject<UHutongSiheyuanTool>();
	HouseAgain->Setup();
	TestEqual(TEXT("house tool restores its own choice"), PickerOf(HouseAgain)->Preset, FString(TEXT("Main Hall (正房)")));
	HouseAgain->Shutdown(EToolShutdownType::Completed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongPanelSimpleViewTest,
	"HutongLayout.Panel.SimpleView",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Simple view keeps a few params per tool, advanced keeps all; nested structs opt in per level, so
// the compound's twelve building sets stay shut.
bool FHutongPanelSimpleViewTest::RunTest(const FString& Parameters)
{
	auto Count = [](const UStruct* Type, bool bAdvanced, const FProperty* Parent = nullptr)
	{
		int32 N = 0;
		TArray<const FProperty*> Chain;
		if (Parent) Chain.Add(Parent);
		for (TFieldIterator<FProperty> It(Type, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_Edit) && HutongPanel::IsVisible(**It, Chain, bAdvanced)) ++N;
		}
		return N;
	};

	const UScriptStruct* Tools[] = {
		FHutongSiheyuanParams::StaticStruct(), FHutongCorridorParams::StaticStruct(), FHutongGateHouseParams::StaticStruct(),
		FHutongInnerGateParams::StaticStruct(), FHutongWallParams::StaticStruct(), FHutongHallParams::StaticStruct(),
		FHutongShopfrontParams::StaticStruct(), FHutongStoreyParams::StaticStruct(), FHutongPavilionParams::StaticStruct(),
		FHutongPaifangParams::StaticStruct(), FHutongScreenWallParams::StaticStruct(), FHutongEarPassageParams::StaticStruct(),
	};
	for (const UScriptStruct* T : Tools)
	{
		const int32 Simple = Count(T, false), All = Count(T, true);
		// The house carries two more: its 檻牆 face and 封護檐 cornice are a student's choice (user, 2026-09-27).
		const int32 Handful = (T == FHutongSiheyuanParams::StaticStruct()) ? 8 : 6;
		TestTrue(FString::Printf(TEXT("%s: a handful in the simple view (%d of %d, at most %d)"), *T->GetName(), Simple, All, Handful),
			Simple >= 1 && Simple <= Handful && All > Simple);
	}

	const UClass* Compound = UHutongCompoundToolProperties::StaticClass();
	const int32 CompoundSimple = Count(Compound, false);
	TestTrue(FString::Printf(TEXT("compound: plan choices only (%d of %d)"), CompoundSimple, Count(Compound, true)),
		CompoundSimple >= 4 && CompoundSimple <= 12);
	const FProperty* MainHall = Compound->FindPropertyByName(TEXT("MainHall"));
	if (TestNotNull(TEXT("compound has its main hall set"), MainHall))
	{
		TestEqual(TEXT("compound: the main hall's own basics stay hidden with its set"),
			Count(FHutongSiheyuanParams::StaticStruct(), false, MainHall), 0);
	}

	// A basic struct field opens only its basic fields.
	const FProperty* Stones = FHutongGateHouseParams::StaticStruct()->FindPropertyByName(TEXT("DoorStones"));
	if (TestNotNull(TEXT("gate has its door stones"), Stones))
	{
		const int32 Inner = Count(FHutongDoorStoneParams::StaticStruct(), false, Stones);
		TestTrue(FString::Printf(TEXT("door stones: on/off and style (%d)"), Inner), Inner == 2);
	}
	return true;
}

#endif
