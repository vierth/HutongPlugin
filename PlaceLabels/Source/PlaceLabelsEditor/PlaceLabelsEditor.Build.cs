using UnrealBuildTool;

public class PlaceLabelsEditor : ModuleRules
{
	public PlaceLabelsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"PlaceLabels"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UnrealEd",
			"EditorFramework",
			"EditorStyle",
			"EditorSubsystem",
			"ToolMenus",
			// IPluginManager, for locating the plugin's Resources/Icons folder.
			"Projects",
			"LevelEditor",
			"InteractiveToolsFramework",
			"EditorInteractiveToolsFramework",
			// Starter type assets; resolving imported type ids.
			"AssetRegistry",
			// GeoJSON import and export.
			"Json",
			// Import/export file dialogs.
			"DesktopPlatform",
			// SListView and the rest of the region browser.
			"ApplicationCore",
			// The dockable Place Labels tab's entry in the Window menu.
			"WorkspaceMenuStructure"
		});
	}
}
