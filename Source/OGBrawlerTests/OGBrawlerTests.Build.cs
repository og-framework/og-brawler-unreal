// SPDX-License-Identifier: BUSL-1.1
using System.IO;
using UnrealBuildTool;
using EpicGames.Core;

public class OGBrawlerTests : TestModuleRules
{
	static OGBrawlerTests()
	{
		if (InTestMode)
		{
			TestMetadata = new Metadata();
			TestMetadata.TestName = "OGBrawlerTests";
			TestMetadata.TestShortName = "OGBrawler";
			TestMetadata.ReportType = "xml";
		}
	}

	public OGBrawlerTests(ReadOnlyTargetRules Target) : base(Target, true)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "OGSimulation", "OGBrawler" });

		// Epic's bundled Catch2 v3.4.0 header
		PublicSystemIncludePaths.Add(Path.Combine(
			Target.UEThirdPartySourceDirectory, "Catch2", "v3.4.0", "extras"));

		// Test source from og-brawler-tests submodule
		string TestSource = Path.Combine(ModuleDirectory, "extern", "og-brawler-tests", "Source");
		ConditionalAddModuleDirectory(new DirectoryReference(Path.Combine(TestSource, "OGBrawlerTests")));
	}
}
