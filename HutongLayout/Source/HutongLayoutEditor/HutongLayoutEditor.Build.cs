using UnrealBuildTool;

public class HutongLayoutEditor : ModuleRules
{
	public HutongLayoutEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"GeometryCore",
			"GeometryFramework",
			"DynamicMesh",
			"MeshConversion",
			"MeshDescription",
			"StaticMeshDescription",
			"GeometryAlgorithms"
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
			"Projects",
			"Json",
			"JsonUtilities",
			"DesktopPlatform",
				"PropertyEditor",
				"RenderCore",
				"RHI",
			"LevelEditor",
			"InteractiveToolsFramework",
			"EditorInteractiveToolsFramework",
			"ModelingComponents",
			"ModelingComponentsEditorOnly",
			"GeometryScriptingCore",
			"MaterialEditor",
			"AssetRegistry"
		});
	}
}
