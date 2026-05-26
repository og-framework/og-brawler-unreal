// SPDX-License-Identifier: MPL-2.0
using System.IO;
using UnrealBuildTool;
using EpicGames.Core;

public class OGSimulationTests : TestModuleRules
{
	static OGSimulationTests()
	{
		if (InTestMode)
		{
			TestMetadata = new Metadata();
			TestMetadata.TestName = "OGSimulationTests";
			TestMetadata.TestShortName = "OGSimulation";
			TestMetadata.ReportType = "xml";
		}
	}

	public OGSimulationTests(ReadOnlyTargetRules Target) : base(Target, true)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "OGSimulation" });

		// Epic's bundled Catch2 v3.4.0 header (WITH_LOW_LEVEL_TESTS auto-defined by UBT for TestTargetRules)
		PublicSystemIncludePaths.Add(Path.Combine(
			Target.UEThirdPartySourceDirectory, "Catch2", "v3.4.0", "extras"));

		// Test source from og-simulation-tests submodule
		string TestSource = Path.Combine(ModuleDirectory, "extern", "og-simulation-tests", "Source");
		ConditionalAddModuleDirectory(new DirectoryReference(Path.Combine(TestSource, "OGSimulationTests")));
	}
}
