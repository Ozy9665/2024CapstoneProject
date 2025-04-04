using UnrealBuildTool;
using System.Collections.Generic;

public class CultServerTarget : TargetRules
{
    public CultServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Program;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        LaunchModuleName = "CultServer";
        ExtraModuleNames.Add("CultServer");
    }
}
