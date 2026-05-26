// SPDX-License-Identifier: MPL-2.0

using UnrealBuildTool;
using System.IO;

public class JoltPhysicsModule : ModuleRules
{
	public JoltPhysicsModule(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;
		bEnforceIWYU = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "glm", "OGSimulation" });
		PrivateDependencyModuleNames.AddRange(new string[] { "CoreUObject", "Engine", "Landscape", "DVolumeModule" });

		// Jolt include path: allows #include <Jolt/Jolt.h> from this module and dependents
		string joltRoot = Path.Combine(ModuleDirectory, "ThirdParty", "JoltPhysics");
		PublicIncludePaths.Add(joltRoot);

		// ABI-affecting defines (must match for all TUs that include Jolt headers)
		PublicDefinitions.Add("JPH_OBJECT_LAYER_BITS=16");

		// SSE4 only on x86/x64 platforms. ARM64 auto-detects NEON in Jolt/Core/Core.h.
		if (Target.Platform == UnrealTargetPlatform.Win64 ||
		    Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicDefinitions.Add("JPH_USE_SSE4_1=1");
			PublicDefinitions.Add("JPH_USE_SSE4_2=1");
		}

		// Disable features not needed for simulation
		PrivateDefinitions.AddRange(new string[]
		{
			"JPH_PROFILE_ENABLED=0",
			"JPH_DEBUG_RENDERER=0"
		});

		// Jolt compiles cleanly with these relaxed — anonymous namespaces + shadow variables
		UndefinedIdentifierWarningLevel = WarningLevel.Off;

		bEnableExceptions = false;
		bUseRTTI = false;
	}
}
