// SPDX-License-Identifier: MPL-2.0

using UnrealBuildTool;

public class OGSimulationUnreal : ModuleRules
{
	public OGSimulationUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Chaos", "Core", "PhysicsCore", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "DVolumeModule", "OGSimulation" });
		SetupIrisSupport(Target);
	}
}
