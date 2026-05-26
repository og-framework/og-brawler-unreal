// SPDX-License-Identifier: BUSL-1.1

using UnrealBuildTool;

public class OGBrawlerUnrealEditorTarget : TargetRules
{
	public OGBrawlerUnrealEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		OGBrawlerUnrealTargetCommon.ConfigureTarget(this);
	}
}
