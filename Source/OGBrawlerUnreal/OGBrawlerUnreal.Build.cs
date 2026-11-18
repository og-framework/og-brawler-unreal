// SPDX-License-Identifier: BUSL-1.1

using UnrealBuildTool;

public class OGBrawlerUnreal : ModuleRules
{
	private const int PhysicsBackendChaos = 0;

	public OGBrawlerUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		FPSemantics = FPSemanticsMode.Precise;
		PrivatePCHHeaderFile = EngineDirectory + "/Source/Runtime/Engine/Public/EngineSharedPCH.h";

		PublicDependencyModuleNames.AddRange(new string[] { "Chaos", "Core", "PhysicsCore", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "DVolumeModule", "OGSimulation", "OGSimulationUnreal", "OGSimulationJoltUnreal", "OGSimulationJolt", "OGBrawler", "ProceduralMeshComponent", "OGNet", "OGNetUnreal", "OGSimulationNet" });
		PrivateDependencyModuleNames.Add("ApplicationCore");
		PublicDefinitions.Add("OG_PHYSICS_BACKEND_CHAOS=" + PhysicsBackendChaos);
		SetupIrisSupport(Target);
	}
}
