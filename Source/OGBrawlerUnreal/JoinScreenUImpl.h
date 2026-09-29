// SPDX-License-Identifier: BUSL-1.1
// docs/JoinScreen-rationale.md · docs/JoinScreen-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "InputCoreTypes.h"

#include <string>
#include <string_view>

#include "OGBrawler/BrawlerJoinScreen.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOGJoinScreen, Log, All);

namespace joinScreenUImpl
{

float scale();

FString toFString(std::string_view text);

std::string toUtf8(const FString& text);

bool editorLaunched();

FString commandLineAddress();

std::string_view ownBuildLabel();

brawlerJoinScreen::RecentAddressList loadRecentAddresses();

bool saveRecentAddresses(const brawlerJoinScreen::RecentAddressList& recents);

FKey localCoopAddPlayerKey();

FKey localCoopRemovePlayerKey();

constexpr brawlerJoinScreen::JoinFailureReason networkFailureReason(ENetworkFailure::Type failure,
                                                                    bool onPendingNetDriver)
{
	using brawlerJoinScreen::JoinFailureReason;

	switch (failure)
	{
	case ENetworkFailure::OutdatedClient:
	case ENetworkFailure::OutdatedServer:
		return JoinFailureReason::DifferentBuild;
	case ENetworkFailure::FailureReceived:
	case ENetworkFailure::PendingConnectionFailure:
		return JoinFailureReason::ServerRefused;
	case ENetworkFailure::ConnectionTimeout:
	case ENetworkFailure::ConnectionLost:
		return onPendingNetDriver ? JoinFailureReason::CannotReachServer : JoinFailureReason::ConnectionLost;
	case ENetworkFailure::NetDriverAlreadyExists:
	case ENetworkFailure::NetDriverCreateFailure:
	case ENetworkFailure::NetDriverListenFailure:
		return JoinFailureReason::CannotReachServer;
	case ENetworkFailure::NetGuidMismatch:
	case ENetworkFailure::NetChecksumMismatch:
		return JoinFailureReason::Unknown;
	}
	return JoinFailureReason::Unknown;
}

constexpr brawlerJoinScreen::JoinFailureReason travelFailureReason(ETravelFailure::Type failure)
{
	using brawlerJoinScreen::JoinFailureReason;

	switch (failure)
	{
	case ETravelFailure::PackageVersion:
		return JoinFailureReason::DifferentBuild;
	case ETravelFailure::PendingNetGameCreateFailure:
	case ETravelFailure::InvalidURL:
		return JoinFailureReason::CannotReachServer;
	case ETravelFailure::NoLevel:
	case ETravelFailure::LoadMapFailure:
	case ETravelFailure::PackageMissing:
	case ETravelFailure::NoDownload:
	case ETravelFailure::TravelFailure:
	case ETravelFailure::CheatCommands:
	case ETravelFailure::CloudSaveFailure:
	case ETravelFailure::ServerTravelFailure:
	case ETravelFailure::ClientTravelFailure:
		return JoinFailureReason::Unknown;
	}
	return JoinFailureReason::Unknown;
}

} // namespace joinScreenUImpl

static_assert(joinScreenUImpl::networkFailureReason(ENetworkFailure::OutdatedClient, true)
                      == brawlerJoinScreen::JoinFailureReason::DifferentBuild
                  && joinScreenUImpl::networkFailureReason(ENetworkFailure::OutdatedServer, true)
                      == brawlerJoinScreen::JoinFailureReason::DifferentBuild
                  && joinScreenUImpl::travelFailureReason(ETravelFailure::PackageVersion)
                      == brawlerJoinScreen::JoinFailureReason::DifferentBuild,
              "Was the T10 spike's Q3 table, row 'different build': a network version mismatch "
              "reaches the client as OutdatedClient/OutdatedServer. See docs/JoinScreen-rationale.md section 13.");
static_assert(joinScreenUImpl::networkFailureReason(ENetworkFailure::ConnectionTimeout, true)
                      == brawlerJoinScreen::JoinFailureReason::CannotReachServer
                  && joinScreenUImpl::networkFailureReason(ENetworkFailure::ConnectionLost, true)
                      == brawlerJoinScreen::JoinFailureReason::CannotReachServer
                  && joinScreenUImpl::networkFailureReason(ENetworkFailure::ConnectionTimeout, false)
                      == brawlerJoinScreen::JoinFailureReason::ConnectionLost
                  && joinScreenUImpl::networkFailureReason(ENetworkFailure::ConnectionLost, false)
                      == brawlerJoinScreen::JoinFailureReason::ConnectionLost,
              "Was the T10 spike's Q3 measurement r6/r7: a timeout while connecting and a timeout "
              "while playing are the SAME failure type and differ only by net driver (PendingNetDriver "
              "vs GameNetDriver). See docs/JoinScreen-rationale.md section 13.");
static_assert(joinScreenUImpl::networkFailureReason(ENetworkFailure::FailureReceived, false)
                      == brawlerJoinScreen::JoinFailureReason::ServerRefused
                  && joinScreenUImpl::networkFailureReason(ENetworkFailure::PendingConnectionFailure, true)
                      == brawlerJoinScreen::JoinFailureReason::ServerRefused,
              "Was the lead's ruling on T10 flag 5 (server refused: <server text>). A refusal during "
              "the login (NMT_Failure, e.g. 'Server full.') arrives as PendingConnectionFailure "
              "carrying the server's text; after the login it arrives as FailureReceived. "
              "See docs/JoinScreen-rationale.md section 13.");
static_assert(joinScreenUImpl::travelFailureReason(ETravelFailure::PendingNetGameCreateFailure)
                      == brawlerJoinScreen::JoinFailureReason::CannotReachServer
                  && joinScreenUImpl::travelFailureReason(ETravelFailure::InvalidURL)
                      == brawlerJoinScreen::JoinFailureReason::CannotReachServer
                  && joinScreenUImpl::networkFailureReason(ENetworkFailure::NetChecksumMismatch, false)
                      == brawlerJoinScreen::JoinFailureReason::Unknown
                  && joinScreenUImpl::travelFailureReason(ETravelFailure::PackageMissing)
                      == brawlerJoinScreen::JoinFailureReason::Unknown,
              "Was the T10 spike's Q3 table, rows 'can't reach server' (an unresolvable host) and "
              "'unknown'. See docs/JoinScreen-rationale.md section 13.");
