#include "Misc/AutomationTest.h"
#include "Tools/WallTool.h"
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

#endif
