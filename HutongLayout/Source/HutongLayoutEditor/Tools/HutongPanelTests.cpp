#include "Misc/AutomationTest.h"
#include "Tools/WallTool.h"
#include "Tools/SiheyuanTool.h"
#include "Tools/StreetRowTool.h"
#include "Tools/HutongPresets.h"
#include "Tools/HutongPresetDefaults.h"
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

	// The rows a details panel would build for this object, customizations and all: the row
	// generator runs the same QueryCustomDetailLayout pass the mode panel's view does.
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

// The wall is two tools, not a picker, and a hidden row has no visible failure mode: registered
// against the wrong class the customization simply never runs and the dropdown is back, which is
// how the role sat on both panels through the split. Asked of the panel rather than of the table.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongWallRolePanelTest, "HutongLayout.Walls.RoleNotOffered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongWallRolePanelTest::RunTest(const FString& Parameters)
{
	const TSet<FName> Lane = RowsFor(NewObject<UHutongLaneWallToolProperties>());
	const TSet<FName> Court = RowsFor(NewObject<UHutongCourtWallToolProperties>());

	// The walk really reached the params, or "no Role row" would be true of an empty panel.
	TestTrue(TEXT("院牆 panel shows the wall's parameters"), Lane.Contains(TEXT("Height")));
	TestTrue(TEXT("隔牆 panel shows the wall's parameters"), Court.Contains(TEXT("Height")));

	TestFalse(TEXT("院牆 tool does not offer the role"), Lane.Contains(TEXT("Role")));
	TestFalse(TEXT("隔牆 tool does not offer the role"), Court.Contains(TEXT("Role")));

	// Retyping a placed run is a Details edit: the placed wall is where the role stays.
	const TSet<FName> Placed = RowsFor(NewObject<UHutongWallBuildingComponent>());
	TestTrue(TEXT("a placed wall keeps its role"), Placed.Contains(TEXT("Role")));
	return true;
}

// Setup and Shutdown are the whole of what a tool does with its property cache, and neither
// needs a tool manager, so the cache can be driven here as the editor drives it: one tool up,
// a preset picked, the tool closed, the next tool up.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongPresetPickerCacheTest, "HutongLayout.Panel.PresetPickerPerTool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongPresetPickerCacheTest::RunTest(const FString& Parameters)
{
	HutongPresets::RegisterBuiltInPresets();

	// The house opens on its default; the street row's picker, though it lists the same house
	// presets, opens on its own default rather than on the house's choice.
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

	RowPicker->Preset = TEXT("Front Row (倒座房)");
	Row->Shutdown(EToolShutdownType::Completed);

	// And the house comes back with its own.
	UHutongSiheyuanTool* HouseAgain = NewObject<UHutongSiheyuanTool>();
	HouseAgain->Setup();
	TestEqual(TEXT("house tool restores its own choice"), PickerOf(HouseAgain)->Preset, FString(TEXT("Main Hall (正房)")));
	HouseAgain->Shutdown(EToolShutdownType::Completed);
	return true;
}

#endif
