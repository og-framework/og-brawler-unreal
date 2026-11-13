// SPDX-License-Identifier: MPL-2.0
// Plugins/OGSimulation/Source/OGNet/og-net/OGNet/docs/OGNet-rationale.md
using System.IO;
using UnrealBuildTool;
using EpicGames.Core;

public class OGNetTests : TestModuleRules
{
	static OGNetTests()
	{
		if (InTestMode)
		{
			TestMetadata = new Metadata();
			TestMetadata.TestName = "OGNetTests";
			TestMetadata.TestShortName = "OGNet";
			TestMetadata.ReportType = "xml";
		}
	}

	public OGNetTests(ReadOnlyTargetRules Target) : base(Target, true)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "OGNet" });

		PublicSystemIncludePaths.Add(Path.Combine(
			Target.UEThirdPartySourceDirectory, "Catch2", "v3.4.0", "extras"));

		string TestSource = Path.Combine(ModuleDirectory, "..", "..", "Plugins", "OGSimulation", "Source", "OGNet", "og-net", "tests");
		ConditionalAddModuleDirectory(new DirectoryReference(TestSource));

		string EngineLaneUnreal = Path.Combine(ModuleDirectory, "..", "..", "Plugins", "OGSimulation", "Source", "OGNetUnreal");
		ConditionalAddModuleDirectory(new DirectoryReference(Path.Combine(EngineLaneUnreal, "Tests", "Catch2")));
		PrivateIncludePaths.Add(Path.Combine(EngineLaneUnreal, "Public"));
		PrivateIncludePaths.Add(TestSource);
	}
}
