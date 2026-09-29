// SPDX-License-Identifier: BUSL-1.1
// docs/SessionLog-rationale.md · docs/SessionLog-guards.md

#include "OGBrawlerUnreal/SessionLogUImpl.h"

#include "Engine/ChildConnection.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "IPAddress.h"

#include "OGBrawler/SessionConstants.h"
#include "OGBrawlerUnreal/OGBuildIdentityUImpl.h"
#include "OGBrawlerUnreal/JoinScreenUImpl.h"

DEFINE_LOG_CATEGORY(LogOGSession);

namespace
{

const UNetConnection* rootConnectionOf(const APlayerController& controller)
{
	const UNetConnection* connection = controller.NetConnection;
	if (const UChildConnection* child = Cast<UChildConnection>(connection))
		return child->Parent;
	return connection;
}

bool logsSession(const AGameModeBase& gameMode)
{
	return gameMode.GetNetMode() == NM_DedicatedServer;
}

constexpr int32 kTestedPlayers = og::brawler::session::maxPlayersPerServer;

} // namespace

namespace sessionLogUImpl
{

PlayerCounts countPlayers(const UWorld& world, const APlayerController* connectionOf, const AController* exiting)
{
	const UNetConnection* connection = (connectionOf != nullptr) ? rootConnectionOf(*connectionOf) : nullptr;

	PlayerCounts counts;
	for (FConstPlayerControllerIterator it = world.GetPlayerControllerIterator(); it; ++it)
	{
		const APlayerController* controller = it->Get();
		// ⛔G-02  docs/SessionLog-guards.md
		if (controller == nullptr || controller == exiting || controller->PlayerState == nullptr
		    || controller->PlayerState->IsOnlyASpectator())
		{
			continue;
		}

		++counts.players;
		if (connection != nullptr && rootConnectionOf(*controller) == connection)
			++counts.onConnection;
	}
	return counts;
}

void SessionLog::noteListening(const AGameModeBase& gameMode)
{
	if (!logsSession(gameMode))
		return;

	checkf(!gameMode.bUseSeamlessTravel,
		TEXT("Was the T10 spike's Q6 caveat: the OGBrawlerSession join/leave lines are counted in ")
		TEXT("PostLogin and Logout only, and a seamless travel carries players over without PostLogin ")
		TEXT("(AGameModeBase::HandleSeamlessTravelPlayer). Extend SessionLogUImpl before turning ")
		TEXT("seamless travel on. See docs/SessionLog-rationale.md section 4."));

	UE_LOG(LogOGSession, Display, TEXT("%s"),
		*FString::Printf(kSessionLineFormats.buildLabel, *joinScreenUImpl::toFString(buildIdentityUImpl::buildLabel())));

	const UWorld*     world     = gameMode.GetWorld();
	const UNetDriver* netDriver = (world != nullptr) ? world->GetNetDriver() : nullptr;
	if (netDriver == nullptr)
		return;

	const TSharedPtr<const FInternetAddr> localAddress = const_cast<UNetDriver*>(netDriver)->GetLocalAddr();
	const int32 port = localAddress.IsValid() ? localAddress->GetPort() : world->URL.Port;

	UE_LOG(LogOGSession, Display, TEXT("%s"), *FString::Printf(kSessionLineFormats.listening, port));
}

void SessionLog::noteJoined(const AGameModeBase& gameMode, const APlayerController& joined)
{
	const UWorld* world = gameMode.GetWorld();
	if (!logsSession(gameMode) || world == nullptr)
		return;

	const PlayerCounts counts = countPlayers(*world, &joined, nullptr);

	UE_LOG(LogOGSession, Display, TEXT("%s"),
		*FString::Printf(kSessionLineFormats.joined, counts.players, kTestedPlayers, counts.onConnection));

	const AboveTestedStep step = aboveTestedAfter(m_aboveTestedSize, counts.players, kTestedPlayers);
	m_aboveTestedSize = step.nowAbove;
	if (step.logCrossing)
	{
		UE_LOG(LogOGSession, Display, TEXT("%s"),
			*FString::Printf(kSessionLineFormats.aboveTestedSize, counts.players, kTestedPlayers));
	}
}

void SessionLog::noteLeft(const AGameModeBase& gameMode, const AController* exiting)
{
	const UWorld* world = gameMode.GetWorld();
	if (!logsSession(gameMode) || world == nullptr)
		return;

	const APlayerController* exitingPlayer = Cast<APlayerController>(exiting);
	if (exitingPlayer == nullptr || exitingPlayer->PlayerState == nullptr || exitingPlayer->PlayerState->IsOnlyASpectator())
		return;

	const PlayerCounts counts = countPlayers(*world, nullptr, exiting);

	UE_LOG(LogOGSession, Display, TEXT("%s"),
		*FString::Printf(kSessionLineFormats.left, counts.players, kTestedPlayers));

	m_aboveTestedSize = aboveTestedAfter(m_aboveTestedSize, counts.players, kTestedPlayers).nowAbove;
}

} // namespace sessionLogUImpl
