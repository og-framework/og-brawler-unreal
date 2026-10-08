// SPDX-License-Identifier: BUSL-1.1

using UnrealBuildTool;

// Dummy TargetRules required by UBT — it expects every .Target.cs file to
// declare a class named <Filename>Target : TargetRules.
// This target is never built; it exists only so GenerateProjectFiles succeeds.
// The real purpose of this file is the OGBrawlerUnrealTargetCommon static helper below.
[SupportedPlatforms()]
public class OGBrawlerUnrealTargetCommonTarget : TargetRules
{
	public OGBrawlerUnrealTargetCommonTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		OGBrawlerUnrealTargetCommon.ConfigureTarget(this);
	}
}

public static class OGBrawlerUnrealTargetCommon
{
	public static void ConfigureTarget(TargetRules Rules)
	{
		Rules.DefaultBuildSettings = BuildSettingsVersion.V4;
		Rules.IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;

		Rules.ExtraModuleNames.Add("OGBrawlerUnreal");
		Rules.ExtraModuleNames.Add("OGSimulationUnreal");
		Rules.ExtraModuleNames.Add("DVolumeModule");
		Rules.ExtraModuleNames.Add("ProceduralMountainSide");

		// [movement-sim T90] Chaos::IsInPhysicsThreadContext() is compiled out of Shipping and Test
		// (Chaos/Public/Framework/Threading.h: PHYSICS_THREAD_CONTEXT defaults to 0 there, behind an
		// #ifndef). ChaosSpatialQueryAdapter.cpp reads it on every query to choose the physics-thread vs
		// game-thread particle view, and the answer is load-bearing (MEASURED true inside the pre-simulate
		// callback; impl/arch_pt_static_query.md Q1). Keep the engine's own tracking in the two
		// configurations that would drop it. Scoped so Development/DebugGame action graphs are untouched.
		// Requires the Unique build environment (source engine, monolithic target): the tracking's TLS
		// singleton is defined inside the Chaos module under the same macro, so the define must reach the
		// engine modules compiled into this target. impl/design_shipping_thread_context.md.
		if (Rules.Configuration == UnrealTargetConfiguration.Shipping ||
		    Rules.Configuration == UnrealTargetConfiguration.Test)
		{
			Rules.GlobalDefinitions.Add("PHYSICS_THREAD_CONTEXT=1");
		}

		// [uploadtosteam T20] The attack circle / aim visualization is drawn with DrawDebug*, which
		// Engine/Public/EngineDefines.h compiles out of Shipping and Test (UE_ENABLE_DEBUG_DRAWING, behind
		// an #ifndef). Keep it for the Client target only: the dedicated server renders nothing, and
		// Development/DebugGame/Editor action graphs stay untouched. Temporary until gameplay visuals
		// move off the debug-draw API.
		if (Rules.Type == TargetType.Client &&
		    (Rules.Configuration == UnrealTargetConfiguration.Shipping ||
		     Rules.Configuration == UnrealTargetConfiguration.Test))
		{
			Rules.GlobalDefinitions.Add("UE_ENABLE_DEBUG_DRAWING=1");
		}
	}
}
