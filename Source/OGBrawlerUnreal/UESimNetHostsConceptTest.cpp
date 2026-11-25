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
using LanesServerHost = Lanes::ServerHost;
using ClientHost = Lanes::ClientHost;
using Server = ogsimnet::SimNetServer<Traits::Composition, LanesServerHost, Lanes::Transport>;

constexpr bool routesToNetAtLog(const TCHAR* body)
{
	const OGLogRouteEntry* const entry = ogLogRouteFor(body);
	return entry != nullptr && entry->route == OGLogRoute::Net && entry->plainLevel == ELogVerbosity::Log;
}
} // namespace ueSimNetHostsConceptTest

static_assert(ogsimnet::SimNetServerHost<ueSimNetHostsConceptTest::LanesServerHost>,
	"UESimNetLanesServerHost<BrawlerSessionTraitsUImpl>, the host the session's og-sim-net server runs on, must satisfy "
	"og-sim-net's SimNetServerHost");
static_assert(!ogsimnet::SimNetServerHost<ueSimNetHostsConceptTest::ServerHost>,
	"UESimNetServerHost<BrawlerSessionTraitsUImpl> keeps no latency stamps of its own: the authority runner's lanes host "
	"stamps through the diagnostics' mailbox. Was the in-frame server's game-thread stamps (og-session-runtime task 14)");
static_assert(ogsimnet::SimNetClientHost<ueSimNetHostsConceptTest::ClientHost>,
	"UESimNetClientHost<BrawlerSessionTraitsUImpl> must satisfy og-sim-net's SimNetClientHost");
static_assert(ogsimnet::SimNetDeliveryHook<ueSimNetHostsConceptTest::LanesServerHost>
		&& ogsimnet::SimNetDeliveryHook<ueSimNetHostsConceptTest::ClientHost>
		&& ogsimnet::SimNetDeliveryHookWellFormed<ueSimNetHostsConceptTest::LanesServerHost>
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
static_assert(simTrafficImmediateKnobRefused(std::optional<int32>{0}) && !simTrafficImmediateKnobRefused(std::optional<int32>{1})
		&& !simTrafficImmediateKnobRefused(std::nullopt),
	"A client's lanes remember [OGNetcode] SimTrafficImmediate=0 as a refused knob, which the session refuses at beginPlay "
	"([SimHost.Refused] knob=SimTrafficImmediate value=0 reason=retired); 1 and an absent key run the immediate client. Was the "
	"frame-locked client the 0 selected (og-session-runtime task 12)");
static_assert(simLanesIoMode(/*isServer=*/true, /*dedicatedServer=*/true) == ognetunreal::IoMode::HostServiced
		&& simLanesIoMode(/*isServer=*/true, /*dedicatedServer=*/false) == ognetunreal::IoMode::OwnThread
		&& simLanesIoMode(/*isServer=*/false, /*dedicatedServer=*/true) == ognetunreal::IoMode::OwnThread
		&& simLanesIoMode(/*isServer=*/false, /*dedicatedServer=*/false) == ognetunreal::IoMode::OwnThread,
	"A dedicated server's lanes are host-serviced (the authority runner services og-net); every other role's own thread "
	"services them, so a listen server's engine lane still runs while its world starts no session. Was the authority-loop "
	"decision og.Sim.AuthorityLoop selected (og-session-runtime task 14)");

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/CriticalSection.h"
#include "Misc/AutomationTest.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"
#include "OGSimulation/PCTimeManagement/TimeConfig.h"
#include "UObject/Package.h"

#include <atomic>

namespace ueSimNetHostsConceptTest
{
struct FDroppingEndpoint
{
	explicit FDroppingEndpoint(const ognet::EndpointConfig& InConfig) : Config(InConfig), Endpoint(this, Ops()) {}

	static void DropOne(void* Ctx) { static_cast<FDroppingEndpoint*>(Ctx)->Drops.fetch_add(1u, std::memory_order_relaxed); }

	static ognetunreal::SimLaneEndpointOps Ops()
	{
		ognetunreal::SimLaneEndpointOps Out;
		Out.send = [](void* Ctx, ognet::ConnectionId, ognet::Lane, std::span<const std::byte>) { DropOne(Ctx); };
		Out.broadcastExcept = [](void* Ctx, ognet::ConnectionId, ognet::Lane, std::span<const std::byte>) { DropOne(Ctx); };
		Out.setLaneHandler = [](void*, ognet::Lane, ognet::LaneHandler) {};
		Out.setConnectionHandler = [](void*, ognet::ConnectionHandler) {};
		Out.disconnect = [](void* Ctx, ognet::ConnectionId) { DropOne(Ctx); };
		Out.noteDelivered = [](void*, ognet::Lane, double, double) {};
		Out.config = [](const void* Ctx) -> const ognet::EndpointConfig& { return static_cast<const FDroppingEndpoint*>(Ctx)->Config; };
		Out.serviceUntil = [](void*, double) {
			ognet::ServiceResult Result;
			Result.stopped = true;
			return Result;
		};
		Out.hasHighResolutionWait = [](const void*) { return false; };
		return Out;
	}

	uint64 DropCount() const { return Drops.load(std::memory_order_relaxed); }

	ognet::EndpointConfig Config;
	std::atomic<uint64> Drops{0u};
	ognetunreal::SimLaneEndpoint Endpoint;
};

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

	ognet::EndpointConfig serverConfig;
	serverConfig.role = ognet::Role::Server;
	ognet::EndpointConfig clientConfig;
	clientConfig.role = ognet::Role::Client;
	FDroppingEndpoint serverEndpoint(serverConfig);
	FDroppingEndpoint clientEndpoint(clientConfig);
	TUniquePtr<Lanes> serverLanes = MakeUnique<Lanes>(nullptr, true);
	TUniquePtr<Lanes> clientLanes = MakeUnique<Lanes>(nullptr, false);
	serverLanes->beforeStart(serverEndpoint.Endpoint);
	serverLanes->afterStart();
	clientLanes->beforeStart(clientEndpoint.Endpoint);
	clientLanes->afterStart();
	TestTrue(TEXT("a server lanes object over a dropping endpoint is the server's"), serverLanes->isServer());
	TestTrue(TEXT("a server lanes object holds the transport only"), serverLanes->client() == nullptr);
	TestTrue(TEXT("a client lanes object over a dropping endpoint holds its client session"),
		!clientLanes->isServer() && clientLanes->client() != nullptr);
	TestTrue(TEXT("the server's transport is sized for the endpoint's connections"),
		serverLanes->transport().config().maxConnections == ognet::EndpointConfig{}.maxConnections);
	TestTrue(TEXT("the client's transport is sized for its one server"), clientLanes->transport().config().maxConnections == 1u);
	TestFalse(TEXT("lanes the driver never asked for an I/O mode are not host-serviced"), serverLanes->hostServiced());
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
	{
		FServerRttWarnings warnings;
		const bool firstNoConnection = warnings.first(EServerRttUnavailable::NoConnection);
		const bool secondNoConnection = warnings.first(EServerRttUnavailable::NoConnection);
		const bool firstNoNetPing = warnings.first(EServerRttUnavailable::NoNetPing);
		const bool secondNoNetPing = warnings.first(EServerRttUnavailable::NoNetPing);
		TestTrue(TEXT("the server's RTT warning fires once per cause: a later cause still warns after an earlier one"),
			firstNoConnection && !secondNoConnection && firstNoNetPing && !secondNoNetPing);
	}
	{
		FServerConnectionLookup lookup;
		FServerRttWarnings warnings;
		constexpr double joinedSeconds = 100.0;
		constexpr double leftSeconds = 100.1;
		lookup.noteFound(*driver, 5001u, joinedSeconds);
		const FServerConnectionFound leftMiss = lookup.find(driver, 5001u, leftSeconds);
		const FServerConnectionFound unknownMiss = lookup.find(driver, 5002u, leftSeconds);
		const bool leaveWarned = leftMiss.missingCause != EServerRttUnavailable::None && warnings.first(leftMiss.missingCause);
		const bool outageWarned = unknownMiss.missingCause != EServerRttUnavailable::None && warnings.first(unknownMiss.missingCause);
		TestTrue(TEXT("a graceful leave's miss spends no NoConnection warning, so a later unknown og-net id still warns"),
			leftMiss.connection == nullptr && unknownMiss.connection == nullptr && !leaveWarned && outageWarned);
		TestTrue(TEXT("the leaver's later bundles in its drained close are still a leave"),
			lookup.find(driver, 5001u, leftSeconds + ognetunreal::kDrainedCloseSeconds).missingCause == EServerRttUnavailable::None);
		lookup.noteFound(*driver, 5003u, joinedSeconds);
		lookup.noteFound(*driver, 5004u, leftSeconds);
		TestTrue(TEXT("a found og-net id that the driver dropped before any miss is a leave too"),
			lookup.find(driver, 5003u, leftSeconds + 1.0).missingCause == EServerRttUnavailable::None);
		const double forgetSeconds = leftSeconds + kServerGoneConnectionForgetSeconds + 1.0;
		lookup.noteFound(*driver, 5005u, forgetSeconds);
		TestTrue(TEXT("a closed og-net id is forgotten once its forget window has passed"),
			lookup.find(driver, 5001u, forgetSeconds).missingCause == EServerRttUnavailable::NoConnection);
		TestTrue(TEXT("the connection lookup with no driver reports NoDriver"),
			lookup.find(nullptr, 5001u, forgetSeconds).missingCause == EServerRttUnavailable::NoDriver);
	}
	{
		FClientRttWarnings warnings;
		for (uint32 causeIndex = 1u; causeIndex < static_cast<uint32>(EClientRttUnavailable::Count); ++causeIndex)
		{
			const EClientRttUnavailable cause = static_cast<EClientRttUnavailable>(causeIndex);
			AddExpectedMessagePlain(
				FString::Printf(TEXT("[RttSample] CLIENT prediction-offset RTT source unavailable (%s). NetworkTimeEstimator"),
					clientRttUnavailableText(cause)),
				ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			warnClientRttUnavailableOnce(warnings, cause);
			warnClientRttUnavailableOnce(warnings, cause);
		}
	}

	FRouteCapture capture;
	const ELogVerbosity::Type configuredNetVerbosity = LogOGNet.GetVerbosity();
	LogOGNet.SetVerbosity(ELogVerbosity::Log);
	GLog->AddOutputDevice(&capture);
	AddExpectedMessagePlain(TEXT("[SimNet.Window] role=client p54probe"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("[SimNet.Host] role=server p54probe"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("[Roster.Refused] role=server p54probe"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	AddExpectedMessagePlain(TEXT("[Roster.Refused] reason=Truncated p54probe"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
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
		TestTrue(TEXT("a token-less [Roster. line is routed with a [Warning] token"),
			lines[3].Text.Contains(TEXT("[Warning][Roster.Refused] reason=Truncated p54probe")));
	}

	LanesServerHost serverHost(nullptr, serverLanes->endpoint(), nullptr);
	Server server(serverHost, serverLanes->transport(), TimeConfig{}, 0u);
	const uint64 dropsBefore = serverEndpoint.DropCount();
	server.publishClock(120u);
	serverLanes->afterDispatch();
	serverLanes->onTickFlush();
	TestEqual(TEXT("a server lanes object's frame points send nothing: the in-frame server's dispatch and flush are retired"),
		serverEndpoint.DropCount(), dropsBefore);
	server.afterInlineBatch(120);
	TestEqual(TEXT("the runner's batch hook sent the staged clock once"), server.stats().clocksSent, uint64{1});
	TestEqual(TEXT("the clock reached the endpoint at once, in the batch hook"), serverEndpoint.DropCount(), dropsBefore + 1u);
	server.afterInlineBatch(121);
	TestEqual(TEXT("an unchanged clock is not sent again"), serverEndpoint.DropCount(), dropsBefore + 1u);
	serverLanes->beforeStop();

	clientLanes->afterDispatch();
	clientLanes->onTickFlush();
	TestEqual(TEXT("a client with nothing to send sends nothing"), clientEndpoint.DropCount(), uint64{0});
	clientLanes->beforeStop();
	return true;
}

#endif
