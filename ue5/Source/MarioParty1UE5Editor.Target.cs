using UnrealBuildTool;

public class MarioParty1UE5EditorTarget : TargetRules
{
    public MarioParty1UE5EditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("MarioParty1UE5");
    }
}
