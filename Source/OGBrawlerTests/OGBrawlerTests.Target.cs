// SPDX-License-Identifier: BUSL-1.1

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class OGBrawlerTestsTarget : TestTargetRules
{
    public OGBrawlerTestsTarget(TargetInfo Target) : base(Target)
    {
        bWithLowLevelTestsOverride = true;

        // Pure C++ tests, no engine / no UObject system.
        bCompileAgainstEngine = false;
        bCompileAgainstCoreUObject = false;
        bCompileAgainstApplicationCore = false;

        LaunchModuleName = "OGBrawlerTests";
    }
}
