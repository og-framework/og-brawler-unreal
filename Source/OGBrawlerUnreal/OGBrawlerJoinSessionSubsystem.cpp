// SPDX-License-Identifier: BUSL-1.1
// docs/JoinScreen-rationale.md · docs/JoinScreen-guards.md

#include "OGBrawlerUnreal/OGBrawlerJoinSessionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformTime.h"
#include "UObject/UObjectGlobals.h"

#include "OGBrawlerUnreal/JoinScreenUImpl.h"
#include "OGBrawlerUnreal/OGBrawlerFrontEndGameMode.h"

UOGBrawlerJoinSessionSubsystem* UOGBrawlerJoinSessionSubsystem::forShownFrontEnd(const UWorld* world)
{
	if (world == nullptr)
		return nullptr;

	const AOGBrawlerFrontEndGameMode* frontEnd = world->GetAuthGameMode<AOGBrawlerFrontEndGameMode>();
	if (frontEnd == nullptr || !frontEnd->isFrontEndActive())
		return nullptr;

	const UGameInstance* gameInstance = world->GetGameInstance();
	return (gameInstance != nullptr) ? gameInstance->GetSubsystem<UOGBrawlerJoinSessionSubsystem>() : nullptr;
}

bool UOGBrawlerJoinSessionSubsystem::ShouldCreateSubsystem(UObject* outer) const
{
	return Super::ShouldCreateSubsystem(outer) && !IsRunningDedicatedServer();
}

void UOGBrawlerJoinSessionSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);

	const FString commandLineAddress = joinScreenUImpl::commandLineAddress();

	m_model = brawlerJoinScreen::JoinScreenModel::start(joinScreenUImpl::loadRecentAddresses(),
		joinScreenUImpl::toUtf8(commandLineAddress), /*reachedViaFailureReturn=*/false);

	m_autoJoinPending = m_model.phase() == brawlerJoinScreen::JoinPhase::Connecting;

	UE_LOG(LogOGJoinScreen, Log,
		TEXT("OGJoinScreen: session start recents=%d commandLineAddress='%s' connecting=%d editorLaunched=%d"),
		static_cast<int32>(m_model.recents().entries().size()), *commandLineAddress,
		m_autoJoinPending ? 1 : 0, joinScreenUImpl::editorLaunched() ? 1 : 0);

	if (GEngine != nullptr)
	{
		// ⛔G-04  docs/JoinScreen-guards.md
		m_networkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::handleNetworkFailure);
		m_travelFailureHandle  = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::handleTravelFailure);
	}

	m_postLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::handlePostLoadMap);
}

void UOGBrawlerJoinSessionSubsystem::Deinitialize()
{
	if (GEngine != nullptr)
	{
		GEngine->OnNetworkFailure().Remove(m_networkFailureHandle);
		GEngine->OnTravelFailure().Remove(m_travelFailureHandle);
	}

	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(m_postLoadMapHandle);

	Super::Deinitialize();
}

void UOGBrawlerJoinSessionSubsystem::noteFrontEndShown(UWorld& world, bool reachedViaFailureReturn)
{
	UE_LOG(LogOGJoinScreen, Log, TEXT("OGJoinScreen: front-end shown failureReturn=%d autoJoin=%d"),
		reachedViaFailureReturn ? 1 : 0, (m_autoJoinPending && !reachedViaFailureReturn) ? 1 : 0);

	// ⛔G-02  docs/JoinScreen-guards.md
	if (reachedViaFailureReturn)
	{
		m_autoJoinPending = false;
		noteFailure(brawlerJoinScreen::JoinFailureReason::Unknown, TEXT("failure return"), FString());
		return;
	}

	if (m_autoJoinPending)
	{
		m_autoJoinPending = false;
		travelTo(world, m_model.targetAddress());
	}
}

bool UOGBrawlerJoinSessionSubsystem::handleCharacter(TCHAR character)
{
	return m_model.typeCharacter(character);
}

bool UOGBrawlerJoinSessionSubsystem::handleKey(const FKey& key, bool controlDown, UWorld& world)
{
	using brawlerJoinScreen::JoinNavigation;

	if (key == EKeys::V && controlDown)
	{
		FString clipboard;
		FPlatformApplicationMisc::ClipboardPaste(clipboard);
		m_model.paste(std::basic_string_view<TCHAR>(*clipboard, static_cast<std::size_t>(clipboard.Len())));
		return true;
	}

	if (key == EKeys::BackSpace)
		m_model.backspace();
	else if (key == EKeys::Delete)
		m_model.deleteForward();
	else if (key == EKeys::Left)
		m_model.moveCursorLeft();
	else if (key == EKeys::Right)
		m_model.moveCursorRight();
	else if (key == EKeys::Home)
		m_model.moveCursorToStart();
	else if (key == EKeys::End)
		m_model.moveCursorToEnd();
	else if (key == EKeys::Up || key == EKeys::Gamepad_DPad_Up || key == EKeys::Gamepad_LeftStick_Up)
		m_model.navigate(JoinNavigation::Up);
	else if (key == EKeys::Down || key == EKeys::Gamepad_DPad_Down || key == EKeys::Gamepad_LeftStick_Down)
		m_model.navigate(JoinNavigation::Down);
	else if (key == EKeys::Enter || key == EKeys::Gamepad_FaceButton_Bottom)
		join(world);
	else if (key == EKeys::Escape || key == EKeys::Gamepad_FaceButton_Right)
		return cancelJoin(world);
	else
		return false;

	return true;
}

void UOGBrawlerJoinSessionSubsystem::noteLocalPlayerLimitReached(int32 maxLocalPlayers)
{
	m_localPlayerLimitReachedAt = FPlatformTime::Seconds();
	m_localPlayerLimit          = maxLocalPlayers;

	UE_LOG(LogOGJoinScreen, Log, TEXT("OGJoinScreen: local player refused, this PC already has %d"), maxLocalPlayers);
}

TOptional<FString> UOGBrawlerJoinSessionSubsystem::localPlayerLimitNotice() const
{
	if (m_localPlayerLimit <= 0)
		return {};

	const float secondsSinceRefused = static_cast<float>(FPlatformTime::Seconds() - m_localPlayerLimitReachedAt);
	if (!brawlerJoinScreen::localPlayerLimitNoticeVisible(secondsSinceRefused))
		return {};

	return joinScreenUImpl::toFString(brawlerJoinScreen::localPlayerLimitNoticeText(m_localPlayerLimit));
}

void UOGBrawlerJoinSessionSubsystem::join(UWorld& world)
{
	const std::optional<std::string> address = m_model.activate();
	if (!address.has_value())
		return;

	travelTo(world, *address);
}

bool UOGBrawlerJoinSessionSubsystem::cancelJoin(UWorld& world)
{
	if (!m_model.cancel())
		return false;

	m_autoJoinPending = false;

	if (GEngine != nullptr)
	{
		GEngine->GetWorldContextFromWorldChecked(&world).TravelURL.Empty();
		GEngine->CancelPending(&world);
	}

	UE_LOG(LogOGJoinScreen, Log, TEXT("OGJoinScreen: cancelled %s"), *joinScreenUImpl::toFString(m_model.targetAddress()));
	return true;
}

void UOGBrawlerJoinSessionSubsystem::travelTo(UWorld& world, const std::string& address)
{
	// ⛔G-06  docs/JoinScreen-guards.md
	const FString travelUrl = FString(TEXT("unreal://")) + joinScreenUImpl::toFString(address);

	UE_LOG(LogOGJoinScreen, Log, TEXT("OGJoinScreen: joining %s"), *travelUrl);

	if (GEngine != nullptr)
		GEngine->SetClientTravel(&world, *travelUrl, TRAVEL_Absolute);
}

bool UOGBrawlerJoinSessionSubsystem::ownsWorld(const UWorld* world) const
{
	return world == nullptr || world->GetGameInstance() == GetGameInstance();
}

void UOGBrawlerJoinSessionSubsystem::handleNetworkFailure(UWorld* world, UNetDriver* netDriver,
                                                          ENetworkFailure::Type failure, const FString& errorText)
{
	if (!ownsWorld(world))
		return;

	const bool onPendingNetDriver = netDriver != nullptr && netDriver->NetDriverName == NAME_PendingNetDriver;

	noteFailure(joinScreenUImpl::networkFailureReason(failure, onPendingNetDriver), ENetworkFailure::ToString(failure),
		errorText);
}

void UOGBrawlerJoinSessionSubsystem::handleTravelFailure(UWorld* world, ETravelFailure::Type failure,
                                                         const FString& errorText)
{
	if (!ownsWorld(world))
		return;

	noteFailure(joinScreenUImpl::travelFailureReason(failure), ETravelFailure::ToString(failure), errorText);
}

void UOGBrawlerJoinSessionSubsystem::noteFailure(brawlerJoinScreen::JoinFailureReason reason, const TCHAR* source,
                                                 const FString& errorText)
{
	m_autoJoinPending = false;

	[[maybe_unused]] const bool latched = m_model.noteJoinFailed(reason, joinScreenUImpl::toUtf8(errorText));

	UE_LOG(LogOGJoinScreen, Log, TEXT("OGJoinScreen: failure %s reason=%d latched=%d text='%s'"),
		source, static_cast<int32>(reason), latched ? 1 : 0, *errorText);
}

void UOGBrawlerJoinSessionSubsystem::handlePostLoadMap(UWorld* world)
{
	if (world == nullptr || !ownsWorld(world) || world->GetNetMode() != NM_Client)
		return;

	m_autoJoinPending = false;

	const bool remember = !joinScreenUImpl::editorLaunched();
	const bool listChanged = m_model.noteJoinSucceeded(remember);
	[[maybe_unused]] const bool saved = listChanged && joinScreenUImpl::saveRecentAddresses(m_model.recents());

	UE_LOG(LogOGJoinScreen, Log, TEXT("OGJoinScreen: joined %s remember=%d recentListSaved=%d recents='%s'"),
		*joinScreenUImpl::toFString(m_model.targetAddress()), remember ? 1 : 0, saved ? 1 : 0,
		*joinScreenUImpl::toFString(m_model.recents().serialize()));
}
