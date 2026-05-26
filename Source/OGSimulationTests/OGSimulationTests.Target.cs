// SPDX-License-Identifier: MPL-2.0

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class OGSimulationTestsTarget : TestTargetRules
{
    public OGSimulationTestsTarget(TargetInfo Target) : base(Target)
    {
        bWithLowLevelTestsOverride = true;

        // Pure C++ tests, no engine / no UObject system.
        bCompileAgainstEngine = false;
        bCompileAgainstCoreUObject = false;
        bCompileAgainstApplicationCore = false;

        // Launch module is the OGSimulationTests plugin module.
        LaunchModuleName = "OGSimulationTests";
    }
}
