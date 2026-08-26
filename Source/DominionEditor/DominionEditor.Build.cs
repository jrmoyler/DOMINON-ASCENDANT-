using UnrealBuildTool;

public class DominionEditor : ModuleRules
{
    public DominionEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "AssetRegistry",
            "AssetTools",
            "UnrealEd",
            "Kismet",
            "UMG",
            "UMGEditor",
            "EnhancedInput",
            "LevelSequence",
            "CinematicCamera",
            "MovieScene",
            "MovieSceneTracks",
            "MaterialEditor",
            "Niagara",
            "NiagaraEditor",
            "DominionCore",
            "DominionGameplay",
            "DominionWorld",
            "DominionUI"
        });
    }
}
