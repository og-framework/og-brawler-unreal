// SPDX-License-Identifier: BUSL-1.1

using UnrealBuildTool;

public class OGBrawlerUnrealClientTarget : TargetRules
{
	public OGBrawlerUnrealClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Client;
		OGBrawlerUnrealTargetCommon.ConfigureTarget(this);
		bUseIris = true;
	}
}
