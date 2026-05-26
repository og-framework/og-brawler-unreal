// SPDX-License-Identifier: BUSL-1.1

using UnrealBuildTool;

[SupportedPlatforms(UnrealPlatformClass.Server)]
public class OGBrawlerUnrealServerTarget : TargetRules
{
	public OGBrawlerUnrealServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		OGBrawlerUnrealTargetCommon.ConfigureTarget(this);
		bUseIris = true;
	}
}
