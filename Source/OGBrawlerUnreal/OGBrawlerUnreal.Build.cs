// SPDX-License-Identifier: BUSL-1.1

using UnrealBuildTool;

public class OGBrawlerUnreal : ModuleRules
{
	private const int PhysicsBackendChaos = 1;

	public OGBrawlerUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Chaos", "Core", "PhysicsCore", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "DVolumeModule", "OGSimulation", "OGSimulationUnreal", "OGSimulationJolt", "OGBrawler", "ProceduralMeshComponent" });
		PrivateDependencyModuleNames.Add("ApplicationCore");
		PublicDefinitions.Add("OG_PHYSICS_BACKEND_CHAOS=" + PhysicsBackendChaos);
		SetupIrisSupport(Target);
	}
}
