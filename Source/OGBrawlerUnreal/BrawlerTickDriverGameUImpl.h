// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerTickDriverGameUImpl-rationale.md · docs/BrawlerTickDriverGameUImpl-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Templates/Casts.h"

#include "OGSimulation/TickDriverConcepts.h"
#include "OGBrawler/SimCharacterId.h"
#include "OGBrawlerUnreal/BrawlerCompositionUImpl.h"

#include <cstdint>

class AActor;
class UOGBrawlerSimulatableComponent;
class AOGBrawlerUECharacter;
struct BrawlerSessionTraitsUImpl;
template <typename TraitsT> class UESimulationSession;

#define HasAuthority HasAuthority_is_constant_true_on_AOGBrawlerSimulationManager_use_worldIsAuthority_or_runsPrediction

template <typename SessionT>
class BrawlerTickDriverGameUImpl
{
public:
	using Manager        = BrawlerCompositionUImpl::Manager;
	using Storage        = BrawlerCompositionUImpl::Storage;
	using RenderSnapshot = BrawlerCompositionUImpl::RenderSnapshot;
	using Session        = SessionT;
	using Component      = UOGBrawlerSimulatableComponent;
	using Character      = AOGBrawlerUECharacter;
	using StaticData     = simulatableBrawler::StaticData;

	explicit BrawlerTickDriverGameUImpl(SessionT& session) : m_session(session) {}

	BrawlerTickDriverGameUImpl(const BrawlerTickDriverGameUImpl&) = delete;
	BrawlerTickDriverGameUImpl& operator=(const BrawlerTickDriverGameUImpl&) = delete;

	Manager& manager() { return *m_session.m_manager; }
	Storage& storage() { return m_session.m_storage; }

	void beforePump(double, TickRateDoor&) {}

	void onFrameStepsDue(int32_t firstUpcomingSimTick, int32_t numSteps)
	{
		m_session.onFrameStepsDue_GameThread(firstUpcomingSimTick, numSteps);
	}

	void afterInlineBatch(int32_t firstUpcomingSimTick) { m_session.afterAuthorityBatch(firstUpcomingSimTick); }

	void releaseInputsForAuthorityTick(SimTick authorityTick)
	{
		// ⛔G-01  docs/BrawlerTickDriverGameUImpl-guards.md
		m_session.releaseDelayedInputsForStep(static_cast<int32>(authorityTick), 1);
	}

	void onStepStarted(double stepStartSeconds) { m_session.editLatencyBudgetDiagnostics().stampAfterStep_Internal(stepStartSeconds); }

	void onStepEnded() { m_session.editLatencyBudgetDiagnostics().stampStepEnd_Internal(); }

	void onPostPhysics() { m_session.onPostPhysicsStep(); }

	static float gravity(const SessionT& session) { return session.m_staticData.m_movementStaticData.gravity; }

	template <typename SlotCallF, typename CharacterT = Character>
	static decltype(auto) callWithSlotRequest(SessionT& session, AActor& owner, uint64_t key, SlotCallF&& slotCall)
	{
		const CharacterT* const character = Cast<CharacterT>(&owner);
		checkf(character != nullptr,
			TEXT("callWithSlotRequest: '%s' is not an AOGBrawlerUECharacter. Only that class's pawns register: the session's ")
			TEXT("first registration pass builds the bodies from its root capsule, and the slot the authority chooses reaches ")
			TEXT("the clients on that character's og-sim-net roster entry. Was the class checkf of tryAcquireJoltSlot (the ")
			TEXT("slot rode a replicated property of that class until og-input-latency task P5-7)."),
			*owner.GetName());
		const FTCHARToUTF8 name(*owner.GetName());
		return slotCall(key, name.Get(), session.owner().GetNetMode() != NM_Client,
			session.physicsSlotFor(owner, key, toStorageKey(character != nullptr ? character->GetSimCharacterId() : SimCharacterId::None)),
			[&session, &owner, key](uint32_t slot) { session.recordAuthoritativeSlot(owner, key, slot); });
	}

private:
	SessionT& m_session;
};

using BrawlerTickDriverGame = BrawlerTickDriverGameUImpl<UESimulationSession<BrawlerSessionTraitsUImpl>>;

#undef HasAuthority
