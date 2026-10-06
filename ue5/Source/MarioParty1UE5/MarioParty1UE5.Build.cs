using UnrealBuildTool;

public class MarioParty1UE5 : ModuleRules
{
    public MarioParty1UE5(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects",
            "RenderCore",
            "RHI"
        });

        CppStandard = CppStandardVersion.Cpp20;
    }
}
