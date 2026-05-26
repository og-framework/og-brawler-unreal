// SPDX-License-Identifier: BUSL-1.1

using UnrealBuildTool;

public class ProceduralMountainSide : ModuleRules
{
	public ProceduralMountainSide(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bEnableExceptions = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });

		PrivateDependencyModuleNames.AddRange(new string[] {
			"CoreUObject",
			"Engine",
			"GeometryCore",
			"GeometryFramework",
			"JoltPhysicsModule"
		});
	}
}
