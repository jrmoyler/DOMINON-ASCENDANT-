using UnrealBuildTool;

public class DominionGameplay : ModuleRules
{
    public DominionGameplay(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DominionCore",
            "DominionSimulation",
            "GameplayAbilities",
            "GameplayTags",
            "Json",
            "JsonUtilities",
            "EnhancedInput",
            "AIModule",
            "StateTreeModule"
        });
    }
}
