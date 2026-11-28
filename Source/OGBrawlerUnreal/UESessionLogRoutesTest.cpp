// SPDX-License-Identifier: BUSL-1.1

#include "OGSimulationUnreal/UESimulationLogRoutes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"
#include "tests/SessionLogRoutesEmittedTags.h"
#include "tests/SessionLogRoutesGolden.h"

#include <cstddef>
#include <iterator>
#include <string>
#include <vector>

namespace ueSessionLogRoutesTest
{
enum class EOwner : uint8
{
	Simulation,
	SimNet,
	Session,
	Adapter,
};

constexpr EOwner ownerOf(std::size_t goldenIndex)
{
	switch (goldenIndex)
	{
	case 27: case 28: case 34: case 53:
		return EOwner::SimNet;
	case 38: case 40: case 41: case 43: case 44: case 45: case 46: case 47: case 48: case 49: case 50: case 51: case 52:
	case 54: case 55:
		return EOwner::Session;
	case 30: case 31: case 32: case 33: case 35: case 36: case 37: case 39: case 42:
		return EOwner::Adapter;
	default:
		return EOwner::Simulation;
	}
}

const TCHAR* nameOf(OGLogRoute route)
{
	switch (route)
	{
	case OGLogRoute::SimTick:         return TEXT("SimTick");
	case OGLogRoute::ResimProbe:      return TEXT("ResimProbe");
	case OGLogRoute::Sim:             return TEXT("Sim");
	case OGLogRoute::Net:             return TEXT("Net");
	case OGLogRoute::RelayProbe:      return TEXT("RelayProbe");
	case OGLogRoute::DivergenceProbe: return TEXT("DivergenceProbe");
	case OGLogRoute::Mgmt:            return TEXT("Mgmt");
	case OGLogRoute::SimHost:         return TEXT("SimHost");
	}
	return TEXT("<unnamed>");
}

const TCHAR* nameOf(ELogVerbosity::Type level)
{
	switch (level)
	{
	case ELogVerbosity::Verbose: return TEXT("Verbose");
	case ELogVerbosity::Log:     return TEXT("Log");
	case ELogVerbosity::Warning: return TEXT("Warning");
	default:                     return TEXT("<other>");
	}
}

using FGoldenRow = sessionLogRoutesGolden::Row;

std::vector<FGoldenRow> goldenReblocked()
{
	std::vector<FGoldenRow> rows;
	for (const EOwner owner : { EOwner::Simulation, EOwner::SimNet, EOwner::Session, EOwner::Adapter })
	{
		for (const FGoldenRow& row : sessionLogRoutesGolden::kRows)
		{
			if (ownerOf(row.index) == owner)
				rows.push_back(row);
		}
	}
	return rows;
}

const FGoldenRow* goldenFirstMatch(const char* line)
{
	const char* body = ogLevelLineBody(line);
	for (const FGoldenRow& row : sessionLogRoutesGolden::kRows)
	{
		if (simulationLogRoutes::startsWithIgnoringCase(body, row.prefix))
			return &row;
	}
	return nullptr;
}

FString describe(const OGLogRouteEntry* row)
{
	return row == nullptr ? FString(TEXT("<none>"))
	                      : FString::Printf(TEXT("%s %s %s"), row->prefix, nameOf(row->route), nameOf(row->plainLevel));
}

FString describe(const FGoldenRow* row)
{
	return row == nullptr ? FString(TEXT("<none>"))
	                      : FString::Printf(TEXT("%hs %hs %hs"), row->prefix, row->route, row->level);
}

struct FCapturedLine
{
	FString Text;
	ELogVerbosity::Type Verbosity = ELogVerbosity::NoLogging;
	FName Category;
};

const TCHAR* const kProbeMark = TEXT(" #t36probe");

class FProbeLineCapture final : public FOutputDevice
{
public:
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		if (FCString::Strstr(V, kProbeMark) == nullptr)
			return;
		FScopeLock lock(&m_lock);
		m_lines.Add(FCapturedLine{ FString(V), static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask),
			Category });
	}

	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }

	TArray<FCapturedLine> TakeLines()
	{
		FScopeLock lock(&m_lock);
		return MoveTemp(m_lines);
	}

private:
	FCriticalSection m_lock;
	TArray<FCapturedLine> m_lines;
};

inline void oldRouteOGMessage(const char* msg)
{
	FString fmsg(msg);

	ELogVerbosity::Type severity = ELogVerbosity::Log;
	FString body = fmsg;
	if (fmsg.StartsWith(TEXT("[Verbose]")))
	{
		severity = ELogVerbosity::Verbose;
		body = fmsg.RightChop(9);
	}
	else if (fmsg.StartsWith(TEXT("[Warning]")))
	{
		severity = ELogVerbosity::Warning;
		body = fmsg.RightChop(9);
	}

#define EMIT_OG(cat) \
	do { \
		switch (severity) { \
			case ELogVerbosity::Warning: UE_LOG(cat, Warning, TEXT("%s"), *fmsg); break; \
			case ELogVerbosity::Verbose: UE_LOG(cat, Verbose, TEXT("%s"), *fmsg); break; \
			default:                     UE_LOG(cat, Log,     TEXT("%s"), *fmsg); break; \
		} \
	} while (0)

	for (const OGLogRouteEntry& entry : kOGLogRoutes)
	{
		if (!body.StartsWith(entry.prefix))
			continue;

		if (severity == ELogVerbosity::Log)
			severity = entry.plainLevel;

		switch (entry.route)
		{
		case OGLogRoute::SimTick:          EMIT_OG(LogOGSimTick); break;
		case OGLogRoute::ResimProbe:       EMIT_OG(LogOGResimProbe); break;
		case OGLogRoute::Sim:              EMIT_OG(LogOGSim); break;
		case OGLogRoute::Net:              EMIT_OG(LogOGNet); break;
		case OGLogRoute::RelayProbe:       EMIT_OG(LogOGRelayProbe); break;
		case OGLogRoute::DivergenceProbe:  EMIT_OG(LogOGDivergenceProbe); break;
		case OGLogRoute::Mgmt:             EMIT_OG(LogOGMgmt); break;
		case OGLogRoute::SimHost:          EMIT_OG(LogOGSimHost); break;
		}
		return;
	}

	EMIT_OG(LogOG);

#undef EMIT_OG
}

inline void oldRouteOGLevelLine(const char* line)
{
	const FString text(UTF8_TO_TCHAR(line));

	ELogVerbosity::Type severity = ELogVerbosity::Log;
	FString body = text;
	if (text.StartsWith(TEXT("[Verbose]")))
	{
		severity = ELogVerbosity::Verbose;
		body = text.RightChop(9);
	}
	else if (text.StartsWith(TEXT("[Warning]")))
	{
		severity = ELogVerbosity::Warning;
		body = text.RightChop(9);
	}
	else if (text.StartsWith(TEXT("[Error]")))
	{
		severity = ELogVerbosity::Error;
		body = text.RightChop(7);
	}

	const OGLogRouteEntry* route = nullptr;
	for (const OGLogRouteEntry& entry : kOGLogRoutes)
	{
		if (body.StartsWith(entry.prefix))
		{
			route = &entry;
			break;
		}
	}
	if (route != nullptr && severity == ELogVerbosity::Log)
		severity = route->plainLevel;

#define EMIT_OG_BODY(cat) \
	do { \
		switch (severity) { \
			case ELogVerbosity::Error:   UE_LOG(cat, Error,   TEXT("%s"), *body); break; \
			case ELogVerbosity::Warning: UE_LOG(cat, Warning, TEXT("%s"), *body); break; \
			case ELogVerbosity::Verbose: UE_LOG(cat, Verbose, TEXT("%s"), *body); break; \
			default:                     UE_LOG(cat, Log,     TEXT("%s"), *body); break; \
		} \
	} while (0)

	if (route == nullptr)
	{
		EMIT_OG_BODY(LogOG);
		return;
	}
	switch (route->route)
	{
	case OGLogRoute::SimTick:          EMIT_OG_BODY(LogOGSimTick); break;
	case OGLogRoute::ResimProbe:       EMIT_OG_BODY(LogOGResimProbe); break;
	case OGLogRoute::Sim:              EMIT_OG_BODY(LogOGSim); break;
	case OGLogRoute::Net:              EMIT_OG_BODY(LogOGNet); break;
	case OGLogRoute::RelayProbe:       EMIT_OG_BODY(LogOGRelayProbe); break;
	case OGLogRoute::DivergenceProbe:  EMIT_OG_BODY(LogOGDivergenceProbe); break;
	case OGLogRoute::Mgmt:             EMIT_OG_BODY(LogOGMgmt); break;
	case OGLogRoute::SimHost:          EMIT_OG_BODY(LogOGSimHost); break;
	}

#undef EMIT_OG_BODY
}

std::vector<std::string> probeLines()
{
	std::vector<std::string> lines;
	for (const char* emitted : sessionLogRoutesGolden::kEmittedLines)
		lines.emplace_back(emitted);
	for (const FGoldenRow& row : sessionLogRoutesGolden::kRows)
	{
		lines.emplace_back(row.prefix);
		lines.emplace_back(std::string("[Verbose]") + row.prefix);
		lines.emplace_back(std::string("[Warning]") + row.prefix);
		lines.emplace_back(std::string("[Error]") + row.prefix);
	}
	lines.emplace_back("[Error] JoltSpatialQueryAdapter: [SpatialQuery.EmptyObjectQuery] volume=1");
	lines.emplace_back("[RelayDelayFloor] floor=2 \xE2\x80\x94 the one");
	lines.emplace_back("[resim.input] lower case");
	lines.emplace_back("no tag at all");
	lines.emplace_back("");
	return lines;
}

struct FCategoryVerbosity
{
	FLogCategoryBase* Category;
	ELogVerbosity::Type Before;
};

class FAllOGCategoriesVerbose
{
public:
	FAllOGCategoriesVerbose()
	{
		for (FLogCategoryBase* category : { static_cast<FLogCategoryBase*>(&LogOG), static_cast<FLogCategoryBase*>(&LogOGSim),
				 static_cast<FLogCategoryBase*>(&LogOGSimTick), static_cast<FLogCategoryBase*>(&LogOGMgmt),
				 static_cast<FLogCategoryBase*>(&LogOGNet), static_cast<FLogCategoryBase*>(&LogOGRelayProbe),
				 static_cast<FLogCategoryBase*>(&LogOGDivergenceProbe), static_cast<FLogCategoryBase*>(&LogOGResimProbe),
				 static_cast<FLogCategoryBase*>(&LogOGSimHost) })
		{
			m_before.Add(FCategoryVerbosity{ category, category->GetVerbosity() });
			category->SetVerbosity(ELogVerbosity::Verbose);
		}
	}

	~FAllOGCategoriesVerbose()
	{
		for (const FCategoryVerbosity& entry : m_before)
			entry.Category->SetVerbosity(entry.Before);
	}

private:
	TArray<FCategoryVerbosity> m_before;
};

class FWarningLinesTestBase : public FAutomationTestBase
{
public:
	FWarningLinesTestBase(const FString& InName, const bool bInComplexTask) : FAutomationTestBase(InName, bInComplexTask) {}

	virtual bool SuppressLogWarnings() override { return true; }
	virtual bool SuppressLogErrors() override { return true; }
};
} // namespace ueSessionLogRoutesTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUESessionLogRoutesPinTest, "OGBrawler.Session.LogRoutes.Pin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUESessionLogRoutesPinTest::RunTest(const FString& Parameters)
{
	using namespace ueSessionLogRoutesTest;

	const std::vector<FGoldenRow> expected = goldenReblocked();
	TestEqual(TEXT("the golden holds today's 56 rows"), static_cast<int32>(std::size(sessionLogRoutesGolden::kRows)), 56);
	TestEqual(TEXT("kOGLogRoutes has as many rows as the golden"), static_cast<int32>(kOGLogRoutes.size()),
		static_cast<int32>(expected.size()));
	TestEqual(TEXT("the engine-free block ends at row 47"), static_cast<int32>(kOGEngineFreeLogRouteCount), 47);
	const std::size_t rows = FMath::Min(kOGLogRoutes.size(), expected.size());
	for (std::size_t i = 0; i < rows; ++i)
	{
		TestEqual(FString::Printf(TEXT("row %d (golden %d): prefix, route and level"), static_cast<int32>(i),
					  static_cast<int32>(expected[i].index)),
			describe(&kOGLogRoutes[i]), describe(&expected[i]));
	}

	int32 prefixes = 0;
	for (const FGoldenRow& row : sessionLogRoutesGolden::kRows)
	{
		const FString prefix(row.prefix);
		TestEqual(FString::Printf(TEXT("ogLogRouteFor(%s)"), *prefix), describe(ogLogRouteFor(*prefix)),
			describe(goldenFirstMatch(row.prefix)));
		++prefixes;
	}
	TestEqual(TEXT("every golden prefix was routed"), prefixes, 56);

	int32 emitted = 0;
	for (const char* line : sessionLogRoutesGolden::kEmittedLines)
	{
		TestEqual(FString::Printf(TEXT("ogLogRouteForLevelLine(%hs)"), line), describe(ogLogRouteForLevelLine(line)),
			describe(goldenFirstMatch(line)));
		const FString wide(UTF8_TO_TCHAR(ogLevelLineBody(line)));
		TestEqual(FString::Printf(TEXT("ogLogRouteFor(%s)"), *wide), describe(ogLogRouteFor(*wide)),
			describe(goldenFirstMatch(line)));
		++emitted;
	}
	TestTrue(TEXT("every emitted line of the tree was routed (more than 300)"), emitted > 300);
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FUESessionLogRouterFoldTest, ueSessionLogRoutesTest::FWarningLinesTestBase,
	"OGBrawler.Session.LogRoutes.RouterFold", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUESessionLogRouterFoldTest::RunTest(const FString& Parameters)
{
	using namespace ueSessionLogRoutesTest;

	const std::vector<std::string> lines = probeLines();
	FProbeLineCapture capture;
	{
		FAllOGCategoriesVerbose verbose;
		GLog->AddOutputDevice(&capture);
		for (const std::string& line : lines)
		{
			const std::string probe = line + TCHAR_TO_UTF8(kProbeMark);
			oldRouteOGMessage(probe.c_str());
			RouteOGMessage(probe.c_str());
			oldRouteOGLevelLine(probe.c_str());
			RouteOGLevelLine(probe.c_str());
		}
		GLog->Flush();
		GLog->RemoveOutputDevice(&capture);
	}

	const TArray<FCapturedLine> captured = capture.TakeLines();
	TestEqual(TEXT("each probe line was printed four times: old and folded, printed token and read token"), captured.Num(),
		4 * static_cast<int32>(lines.size()));
	for (int32 index = 0; index + 1 < captured.Num(); index += 2)
	{
		const FCapturedLine& oldLine = captured[index];
		const FCapturedLine& newLine = captured[index + 1];
		TestEqual(TEXT("the folded router prints the old router's text"), newLine.Text, oldLine.Text);
		TestTrue(FString::Printf(TEXT("the folded router's category equals the old router's: %s"), *oldLine.Text),
			newLine.Category == oldLine.Category);
		TestEqual(FString::Printf(TEXT("the folded router's level equals the old router's: %s"), *oldLine.Text),
			static_cast<int32>(newLine.Verbosity), static_cast<int32>(oldLine.Verbosity));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUESessionLogWantsTest, "OGBrawler.Session.LogRoutes.Wants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUESessionLogWantsTest::RunTest(const FString& Parameters)
{
	const ELogVerbosity::Type before = LogOGNet.GetVerbosity();
	LogOGNet.SetVerbosity(ELogVerbosity::Log);
	const bool verboseAtLog = ogLogWants(SimulationLogRoute::Net, HostLogLevel::Verbose);
	const bool logAtLog = ogLogWants(SimulationLogRoute::Net, HostLogLevel::Log);
	const bool sinkVerboseAtLog = UESessionLogSink{}.wants(SimulationLogRoute::Net, HostLogLevel::Verbose);
	LogOGNet.SetVerbosity(ELogVerbosity::Verbose);
	const bool verboseAtVerbose = ogLogWants(SimulationLogRoute::Net, HostLogLevel::Verbose);
	LogOGNet.SetVerbosity(ELogVerbosity::Warning);
	const bool logAtWarning = ogLogWants(SimulationLogRoute::Net, HostLogLevel::Log);
	const bool warningAtWarning = ogLogWants(SimulationLogRoute::Net, HostLogLevel::Warning);
	LogOGNet.SetVerbosity(before);

	TestFalse(TEXT("LogOGNet at Log: a Net Verbose line is not wanted"), verboseAtLog);
	TestTrue(TEXT("LogOGNet at Log: a Net Log line is wanted"), logAtLog);
	TestFalse(TEXT("UESessionLogSink asks the same category"), sinkVerboseAtLog);
	TestTrue(TEXT("LogOGNet at Verbose: a Net Verbose line is wanted"), verboseAtVerbose);
	TestFalse(TEXT("LogOGNet at Warning: a Net Log line is not wanted"), logAtWarning);
	TestTrue(TEXT("LogOGNet at Warning: a Net Warning line is wanted"), warningAtWarning);
	return true;
}

#endif
