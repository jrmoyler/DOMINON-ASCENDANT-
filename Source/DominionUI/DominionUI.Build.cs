using UnrealBuildTool;

public class DominionUI : ModuleRules
{
    public DominionUI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DominionCore",
            "DominionSimulation",
            "DominionGameplay",
            "DominionWorld",
            "CommonUI",
            "UMG",
            "Slate",
            "SlateCore",
            "InputCore",
            "EnhancedInput",
            "GameplayAbilities",
            "Json"
        });
    }
}
