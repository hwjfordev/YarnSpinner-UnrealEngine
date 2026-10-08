using UnrealBuildTool;

public class YarnSpinnerGameData : ModuleRules
{
    public YarnSpinnerGameData(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "DeveloperSettings", "YarnSpinner" });
        PrecompileForTargets = PrecompileTargetsType.Any;
    }
}
