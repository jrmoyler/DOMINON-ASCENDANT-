using UnrealBuildTool;

public class DominionCore : ModuleRules
{
    public DominionCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags",
            "Json",
            "JsonUtilities"
        });
    }
}
