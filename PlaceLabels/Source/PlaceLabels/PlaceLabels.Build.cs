using UnrealBuildTool;

public class PlaceLabels : ModuleRules
{
	public PlaceLabels(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(ModuleDirectory);

		// No UnrealEd anywhere in this module: it has to build in a packaged game.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UMG",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			// The region outline draws itself through a scene proxy.
			"RenderCore",
			"RHI"
		});
	}
}
