// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerUnreal/OGBrawlerSimulationManager.h"
#include "OGBrawler/SimCharacterId.h"
#include "OGNet/Endpoint/EndpointConfig.h"
#include "OGNetUnreal/OGNetDriver.h"
#include "OGNetUnreal/OGNetSimLanes.h"
#include "OGSimNet/SimNetHost.h"
#include "OGSimNet/SimNetServer.h"
#include "OGSimulationUnreal/UESimNetHost.h"
#include "OGSimulationUnreal/UESimNetLanes.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"

#include <type_traits>

namespace ueSimNetHostsConceptTest
{
using Traits = BrawlerSessionTraitsUImpl;
using Lanes = UESimNetLanes<Traits>;
using ServerHost = UESimNetServerHost<Traits>;
using ClientHost = UESimNetClientHost<Traits>;
using Server = ogsimnet::SimNetServer<Traits::Composition, ServerHost, Lanes::Transport>;

constexpr bool routesToNetAtLog(const TCHAR* body)
{
	const OGLogRouteEntry* const entry = ogLogRouteFor(body);
	return entry != nullptr && entry->route == OGLogRoute::Net && entry->plainLevel == ELogVerbosity::Log;
}
} // namespace ueSimNetHostsConceptTest

static_assert(ogsimnet::SimNetServerHost<ueSimNetHostsConceptTest::ServerHost>,
	"UESimNetServerHost<BrawlerSessionTraitsUImpl> must satisfy og-sim-net's SimNetServerHost");
static_assert(ogsimnet::SimNetClientHost<ueSimNetHostsConceptTest::ClientHost>,
	"UESimNetClientHost<BrawlerSessionTraitsUImpl> must satisfy og-sim-net's SimNetClientHost");
static_assert(ogsimnet::SimNetDeliveryHook<ueSimNetHostsConceptTest::ServerHost>
		&& ogsimnet::SimNetDeliveryHook<ueSimNetHostsConceptTest::ClientHost>
		&& ogsimnet::SimNetDeliveryHookWellFormed<ueSimNetHostsConceptTest::ServerHost>
		&& ogsimnet::SimNetDeliveryHookWellFormed<ueSimNetHostsConceptTest::ClientHost>,
	"Both UE sim-net hosts have og-sim-net's delivery hook noteDelivered(SimLane, double), which writes the driver's "
	"[OGNet.Window] sim-lane rows");
static_assert(std::is_constructible_v<ueSimNetHostsConceptTest::Lanes, UOGNetDriver*, bool>
		&& std::is_base_of_v<IOGNetSimLanes, ueSimNetHostsConceptTest::Lanes>,
	"UESimNetLanes<BrawlerSessionTraitsUImpl> is the driver's IOGNetSimLanes, constructible from the driver and its role");
static_assert(std::is_same_v<ueSimNetHostsConceptTest::Lanes::Transport,
		ogsimnet::SimNetTransport<ognetunreal::SimLaneEndpoint, 2048u>>,
	"UESimNetLanes owns SimNetTransport<SimLaneEndpoint, 2048>");
static_assert(std::is_same_v<ueSimNetHostsConceptTest::Lanes::Client::Channel,
		ueSimNetHostsConceptTest::Server::Channel<SimulatableBrawler>>,
	"The client session over the lanes' transport and a server session over the same transport name one channel type");
static_assert(toStorageKey(static_cast<SimCharacterId>(7)) == 7u,
	"The hosts stamp the latency probe with the sim id as the lane; the brawler's storage key is its sim id");
static_assert(ueSimNetHostsConceptTest::routesToNetAtLog(TEXT("[SimNet.Window] role=client drains=1"))
		&& ueSimNetHostsConceptTest::routesToNetAtLog(TEXT("[SimNet.Host] role=server immediate=0"))
		&& ueSimNetHostsConceptTest::routesToNetAtLog(TEXT("[SimNet.Disconnect] role=server conn=1"))
		&& ueSimNetHostsConceptTest::routesToNetAtLog(TEXT("[Roster.Refused] role=server simId=1 slot=2 reason=SimIdLive"))
		&& ueSimNetHostsConceptTest::routesToNetAtLog(TEXT("[Roster.InputRefused] conn=1 simId=2 reason=NotOwner"))
		&& ueSimNetHostsConceptTest::routesToNetAtLog(TEXT("[Roster.BindMismatch] simId=1 roster=Local engine=Remote")),
	"The [SimNet. and [Roster. rows route og-sim-net's lines to the Net route at plain level Log");

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/CriticalSection.h"
#include "Misc/AutomationTest.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "UObject/Package.h"

namespace ueSimNetHostsConceptTest
{
class FRouteCapture final : public FOutputDevice
{
public:
	struct FLine
	{
		FName Category;
		ELogVerbosity::Type Verbosity = ELogVerbosity::NoLogging;
		FString Text;
	};

	virtual void Serialize(const TCHAR* Text, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		if (FCString::Strstr(Text, TEXT("p54probe")) == nullptr)
			return;
		FScopeLock Lock(&Mutex);
		Lines.Add(FLine{Category, Verbosity, FString(Text)});
	}

	TArray<FLine> Take()
	{
		FScopeLock Lock(&Mutex);
		return MoveTemp(Lines);
	}

private:
	FCriticalSection Mutex;
	TArray<FLine> Lines;
};
} // namespace ueSimNetHostsConceptTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESimNetHostsTest,
	"OGBrawler.SimNet.Hosts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUESimNetHostsTest::RunTest(const FString& Parameters)
{
	using namespace ueSimNetHostsConceptTest;

	TUniquePtr<Lanes> serverLanes = Lanes::makeDetached(ognet::Role::Server, ognet::EndpointConfig{});
	TUniquePtr<Lanes> clientLanes = Lanes::makeDetached(ognet::Role::Client, ognet::EndpointConfig{});
	TestTrue(TEXT("a detached server lanes object is the server's"), serverLanes->isServer() && serverLanes->isDetached());
	TestTrue(TEXT("a detached server lanes object holds the transport only"), serverLanes->client() == nullptr);
	TestTrue(TEXT("a detached client lanes object holds its client session"),
		!clientLanes->isServer() && clientLanes->isDetached() && clientLanes->client() != nullptr);
	TestTrue(TEXT("the server's transport is sized for the endpoint's connections"),
		serverLanes->transport().config().maxConnections == ognet::EndpointConfig{}.maxConnections);
	TestTrue(TEXT("the client's transport is sized for its one server"), clientLanes->transport().config().maxConnections == 1u);
	{
		Lanes unstartedServer(nullptr, true);
		Lanes unstartedClient(nullptr, false);
		TestTrue(TEXT("a lanes object made for either role holds no session before start"),
			unstartedServer.client() == nullptr && unstartedClient.client() == nullptr);
	}

	UOGNetDriver* const driver = NewObject<UOGNetDriver>(GetTransientPackage());
	ServerHost rttHost(driver, serverLanes->endpoint(), nullptr);
	AddExpectedMessagePlain(TEXT("[RttSample] SERVER tier RTT source unavailable"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	TestEqual(TEXT("rttMs of an unknown og-net id is the no-reading sentinel"), rttHost.rttMs(4242u), kNoRttReadingMs);
	TestEqual(TEXT("rttMs of a second unknown og-net id is the sentinel too"), rttHost.rttMs(4243u), kNoRttReadingMs);

	FRouteCapture capture;
	const ELogVerbosity::Type configuredNetVerbosity = LogOGNet.GetVerbosity();
	LogOGNet.SetVerbosity(ELogVerbosity::Log);
	GLog->AddOutputDevice(&capture);
	AddExpectedMessagePlain(TEXT("[SimNet.Window] role=client p54probe"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("[SimNet.Host] role=server p54probe"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("[Roster.Refused] role=server p54probe"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	rttHost.log("[Warning][SimNet.Window] role=client p54probe");
	rttHost.log("[Warning][SimNet.Host] role=server p54probe");
	rttHost.log("[Warning][Roster.Refused] role=server p54probe");
	rttHost.log("[Roster.Refused] reason=Truncated p54probe");
	GLog->Flush();
	GLog->RemoveOutputDevice(&capture);
	LogOGNet.SetVerbosity(configuredNetVerbosity);
	const TArray<FRouteCapture::FLine> lines = capture.Take();
	if (TestEqual(TEXT("every probe line reached the log once"), lines.Num(), 4))
	{
		for (const FRouteCapture::FLine& line : lines)
		{
			TestTrue(*FString::Printf(TEXT("'%s' is logged to LogOGNet"), *line.Text), line.Category == FName(TEXT("LogOGNet")));
		}
		TestTrue(TEXT("a token-less [Roster. line takes the row's Log"), lines[3].Verbosity == ELogVerbosity::Log);
	}

	ServerHost serverHost(nullptr, serverLanes->endpoint(), nullptr);
	Server server(serverHost, serverLanes->transport(), TimeConfig{}, 0u);
	serverLanes->bindServerSession([&server]() { server.dispatchFrame(); }, [&server]() { server.flushFrame(); });
	const uint64 dropsBefore = serverLanes->endpoint().detachedDrops();
	server.publishClock(120u);
	serverLanes->afterDispatch();
	serverLanes->onTickFlush();
	TestEqual(TEXT("the server sent the staged clock once"), server.stats().clocksSent, uint64{1});
	TestEqual(TEXT("the transport released the held clock at the flush"), serverLanes->transport().stats().releasedSends, uint64{1});
	TestEqual(TEXT("nothing is left held after the flush"), serverLanes->transport().heldBytes(), size_t{0});
	TestEqual(TEXT("the detached endpoint dropped and counted the clock"), serverLanes->endpoint().detachedDrops(), dropsBefore + 1u);
	serverLanes->onTickFlush();
	TestEqual(TEXT("an unchanged clock is not sent again"), serverLanes->endpoint().detachedDrops(), dropsBefore + 1u);
	serverLanes->beforeStop();
	TestFalse(TEXT("beforeStop unbinds the server session"), serverLanes->serverSessionBound());
	server.publishClock(121u);
	serverLanes->onTickFlush();
	TestEqual(TEXT("an unbound server lanes object flushes nothing"), serverLanes->endpoint().detachedDrops(), dropsBefore + 1u);

	clientLanes->afterDispatch();
	clientLanes->onTickFlush();
	TestEqual(TEXT("a detached client with nothing to send drops nothing"), clientLanes->endpoint().detachedDrops(), uint64{0});
	clientLanes->beforeStop();
	return true;
}

#endif
