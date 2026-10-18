// SPDX-License-Identifier: MPL-2.0

using UnrealBuildTool;

public class OGSimulationJoltUnreal : ModuleRules
{
	public OGSimulationJoltUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "OGSimulation", "OGSimulationUnreal", "OGSimulationJolt" });
	}
}
