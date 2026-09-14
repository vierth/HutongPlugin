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
			// Registering the generated starter type assets with the content browser, and
			// resolving imported type ids back to assets.
			"AssetRegistry",
			// GeoJSON import and export.
			"Json",
			// The file open/save dialogs the import and export buttons put up.
			"DesktopPlatform",
			// SListView and the rest of the region browser.
			"ApplicationCore",
			// The dockable Place Labels tab's entry in the Window menu.
			"WorkspaceMenuStructure"
		});
	}
}
