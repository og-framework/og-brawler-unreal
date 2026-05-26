// SPDX-License-Identifier: BUSL-1.1

using UnrealBuildTool;

// Dummy TargetRules required by UBT — it expects every .Target.cs file to
// declare a class named <Filename>Target : TargetRules.
// This target is never built; it exists only so GenerateProjectFiles succeeds.
// The real purpose of this file is the OGBrawlerUnrealTargetCommon static helper below.
[SupportedPlatforms()]
public class OGBrawlerUnrealTargetCommonTarget : TargetRules
{
	public OGBrawlerUnrealTargetCommonTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		OGBrawlerUnrealTargetCommon.ConfigureTarget(this);
	}
}

public static class OGBrawlerUnrealTargetCommon
{
	public static void ConfigureTarget(TargetRules Rules)
	{
		Rules.DefaultBuildSettings = BuildSettingsVersion.V4;
		Rules.IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;

		Rules.ExtraModuleNames.Add("OGBrawlerUnreal");
		Rules.ExtraModuleNames.Add("OGSimulationUnreal");
		Rules.ExtraModuleNames.Add("DVolumeModule");
		Rules.ExtraModuleNames.Add("JoltPhysicsModule");
		Rules.ExtraModuleNames.Add("ProceduralMountainSide");
	}
}
