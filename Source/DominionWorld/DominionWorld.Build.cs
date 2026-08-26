using UnrealBuildTool;

public class DominionWorld : ModuleRules
{
    public DominionWorld(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Json",
            "DominionCore",
            "DominionSimulation",
            "DominionGameplay",
            "AIModule"
        });
    }
}
