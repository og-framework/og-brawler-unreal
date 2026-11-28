// SPDX-License-Identifier: MPL-2.0
// Plugins/OGSimulation/Source/OGNet/og-net/OGNet/docs/OGNet-rationale.md
using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class OGNetTestsTarget : TestTargetRules
{
	public OGNetTestsTarget(TargetInfo Target) : base(Target)
	{
		bWithLowLevelTestsOverride = true;

		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = false;
		bCompileAgainstApplicationCore = false;

		LaunchModuleName = "OGNetTests";
	}
}
