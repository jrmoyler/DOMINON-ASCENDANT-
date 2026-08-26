using UnrealBuildTool;

public class DominionSimulation : ModuleRules
{
    public DominionSimulation(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DominionCore",
            "MassEntity",
            "MassCommon",
            "NavigationSystem"
        });

        PrivateDependencyModuleNames.Add("Json");
    }
}
