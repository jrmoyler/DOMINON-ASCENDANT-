using UnrealBuildTool;
using System.Collections.Generic;

public class DominionAscendantEditorTarget : TargetRules
{
    public DominionAscendantEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

        ExtraModuleNames.AddRange(new string[]
        {
            "DominionCore",
            "DominionSimulation",
            "DominionGameplay",
            "DominionWorld",
            "DominionUI",
            "DominionTests",
            "DominionEditor"
        });
    }
}
