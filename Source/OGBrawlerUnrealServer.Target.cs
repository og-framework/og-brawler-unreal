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

		// Shipping server keeps logs (host launcher join lines, HostLogs) and check()s. Scoped to Shipping so
		// the Development server's build environment is unchanged.
		if (Configuration == UnrealTargetConfiguration.Shipping)
		{
			bUseLoggingInShipping = true;
			bUseChecksInShipping = true;
		}
	}
}
