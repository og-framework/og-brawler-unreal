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

	template <typename CharacterT = Character>
	void sampleInput()
	{
		for (const auto& entry : m_session.getRegisteredComponentsById())
		{
			auto* const registered = entry.second.Get();
			CharacterT* const character = registered != nullptr ? Cast<CharacterT>(registered->GetOwner()) : nullptr;
			if (character != nullptr)
				character->getInputCollection()->publishSample();
		}
	}

	void beforePump(double hostNowSeconds, TickRateDoor& rate) { m_session.beforePump_Owner(hostNowSeconds, rate); }

	IdleWake idleUntil(double deadlineSeconds) { return m_session.idleUntil_Runner(deadlineSeconds); }

	void onFrameStepsDue(int32_t firstUpcomingSimTick, int32_t numSteps)
	{
		m_session.onFrameStepsDue_Step(firstUpcomingSimTick, numSteps);
	}

	void afterInlineBatch(int32_t firstUpcomingSimTick) { m_session.afterAuthorityBatch(firstUpcomingSimTick); }

	void releaseInputsForAuthorityTick(SimTick authorityTick)
	{
		m_session.noteAuthorityStepTime_Step();
		// ⛔G-01  docs/BrawlerTickDriverGameUImpl-guards.md
		m_session.releaseDelayedInputsForStep(static_cast<int32>(authorityTick), 1);
	}

	void receiveForStep() { m_session.receiveForClientStep_Step(); }

	void onStepStarted(double stepStartSeconds)
	{
		m_session.editLatencyBudgetDiagnostics().stampAfterStep_Internal(stepStartSeconds);
		m_session.afterClientCapture_Step();
	}

	void onStepEnded()
	{
		m_session.editLatencyBudgetDiagnostics().stampStepEnd_Internal();
		m_session.afterStep_Step();
	}

	void publishForDisplay() { m_session.publishDisplaySnapshot_Step(); }

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

static_assert(TickDriverSampleGame<BrawlerTickDriverGame>,
	"BrawlerTickDriverGame must keep a void sampleInput(): SimulationTickDriver calls it only under if constexpr, so "
	"without it nothing publishes the input sample and every client buildPlayerInput returns the neutral input. Added by "
	"og-input-latency task P5-15.");

static_assert(TickDriverReceiveGame<BrawlerTickDriverGame>,
	"BrawlerTickDriverGame must keep a void receiveForStep(): TickDriverStepHooks calls it only under if constexpr, so "
	"without it an immediate client's step never drains og-sim-net and receives nothing once its session is bound. Was the "
	"drain in onStepStarted, which fed the next step (og-input-latency task P5-30).");

static_assert(TickDriverDisplayGame<BrawlerTickDriverGame>,
	"BrawlerTickDriverGame must keep a void publishForDisplay(): SimulationTickDriver calls it only under if constexpr, after a "
	"client's step batch on the step thread, so without it a Jolt client publishes no display snapshot and its HUD, input "
	"history and visualization show nothing. Was the game-thread fill in onPostPhysicsStep, which raced the worker step "
	"(og-input-latency task P5-36).");

static_assert(TickDriverIdleGame<BrawlerTickDriverGame>,
	"BrawlerTickDriverGame must keep IdleWake idleUntil(double): SimulationTickBackend::runAuthorityLoop requires it, and the "
	"authority runner services og-net only through it, so without it og.Sim.AuthorityLoop=1 cannot build. Added by "
	"og-input-latency task P5-19 (P11 design section 4).");

#undef HasAuthority
