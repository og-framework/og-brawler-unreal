// SPDX-License-Identifier: BUSL-1.1

using UnrealBuildTool;

public class OGBrawlerUnreal : ModuleRules
{
	public OGBrawlerUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Chaos", "Core", "PhysicsCore", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "DVolumeModule", "JoltPhysicsModule", "OGSimulation", "OGSimulationUnreal", "OGBrawler", "ProceduralMeshComponent" });
		SetupIrisSupport(Target);
	}
}
