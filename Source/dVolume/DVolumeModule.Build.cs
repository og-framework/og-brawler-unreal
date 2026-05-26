// SPDX-License-Identifier: MPL-2.0

using UnrealBuildTool;

public class DVolumeModule : ModuleRules
{
	public DVolumeModule(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "glm" });
	}
}
