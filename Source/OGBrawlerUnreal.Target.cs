// SPDX-License-Identifier: BUSL-1.1

using UnrealBuildTool;

public class OGBrawlerUnrealTarget : TargetRules
{
	public OGBrawlerUnrealTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		OGBrawlerUnrealTargetCommon.ConfigureTarget(this);
		bUseIris = true;
	}
}
