using UnrealBuildTool;

public class DominionTests : ModuleRules
{
    public DominionTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "EnhancedInput",
            "CommonUI",
            "UMG",
            "FunctionalTesting",
            "Niagara",
            "UnrealEd",
            "DominionCore",
            "DominionGameplay",
            "DominionSimulation",
            "DominionWorld",
            "DominionUI"
        });
    }
}
