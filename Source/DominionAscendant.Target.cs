using UnrealBuildTool;
using System.Collections.Generic;

public class DominionAscendantTarget : TargetRules
{
    public DominionAscendantTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

        ExtraModuleNames.AddRange(new string[]
        {
            "DominionCore",
            "DominionSimulation",
            "DominionGameplay",
            "DominionWorld",
            "DominionUI"
        });
    }
}
