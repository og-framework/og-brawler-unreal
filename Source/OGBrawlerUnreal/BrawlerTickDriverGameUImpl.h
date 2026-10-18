// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerTickDriverGameUImpl-rationale.md · docs/BrawlerTickDriverGameUImpl-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Templates/Casts.h"

#include "OGSimulation/TickDriverConcepts.h"
#include "OGBrawlerUnreal/BrawlerCompositionUImpl.h"

#include <cstdint>

class AActor;
class ASimulationManagerUImpl;
class USimmableUpdateComponent;
class AOGBrawlerUECharacter;

#define HasAuthority HasAuthority_is_constant_true_on_ASimulationManagerUImpl_use_worldIsAuthority_or_runsPrediction

template <typename SessionT>
class BrawlerTickDriverGameUImpl
{
public:
	using Manager        = BrawlerCompositionUImpl::Manager;
	using Storage        = BrawlerCompositionUImpl::Storage;
	using RenderSnapshot = BrawlerCompositionUImpl::RenderSnapshot;
	using Session        = SessionT;
	using Component      = USimmableUpdateComponent;
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

	void afterInlineBatch(int32_t firstUpcomingSimTick)
	{
		if (typename SessionT::BrawlerReceptionCoordinator* coordinator = m_session.getReceptionCoordinator())
			coordinator->reapConnections(firstUpcomingSimTick);
	}

	void releaseInputsForAuthorityTick(SimTick authorityTick)
	{
		// ⛔G-01  docs/BrawlerTickDriverGameUImpl-guards.md
		m_session.releaseDelayedInputsForStep(static_cast<int32>(authorityTick), 1);
	}

	void onStepStarted(double stepStartSeconds) { m_session.stampLatencyAfterStep_Internal(stepStartSeconds); }

	void onStepEnded() { m_session.stampLatencyStepEnd_Internal(); }

	void onPostPhysics() { m_session.OnPostPhysicsStep(); }

	static float gravity(const SessionT& session) { return session.m_staticData.m_movementStaticData.gravity; }

	template <typename SlotCallF, typename CharacterT = Character>
	static decltype(auto) callWithSlotRequest(SessionT& session, AActor& owner, uint64_t key, SlotCallF&& slotCall)
	{
		CharacterT* const character = Cast<CharacterT>(&owner);
		checkf(character != nullptr,
			TEXT("callWithSlotRequest: '%s' is not an AOGBrawlerUECharacter. The slot the authority chooses reaches the ")
			TEXT("clients on that class's replicated PhysicsSlotValue, so only that class can hold a Jolt slot. Was the ")
			TEXT("class checkf of tryAcquireJoltSlot."),
			*owner.GetName());
		const FTCHARToUTF8 name(*owner.GetName());
		return slotCall(key, name.Get(), session.GetNetMode() != NM_Client, character->GetPhysicsSlot(),
			[character](uint32_t slot) { character->SetAuthoritativePhysicsSlot(slot); });
	}

private:
	SessionT& m_session;
};

using BrawlerTickDriverGame = BrawlerTickDriverGameUImpl<ASimulationManagerUImpl>;

#undef HasAuthority
