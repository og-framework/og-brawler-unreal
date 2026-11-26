// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerSessionHooksUImpl-rationale.md · docs/BrawlerSessionHooksUImpl-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Templates/Casts.h"

#include "OGBrawler/SimCharacterId.h"
#include "OGBrawlerUnreal/BrawlerSessionTraitsUImpl.h"

#include <cstdint>
#include <utility>

class AActor;
class UOGBrawlerSimulatableComponent;
class AOGBrawlerUECharacter;
struct BrawlerSessionTraitsUImpl;
template <typename TraitsT> class UESimulationSession;

#define HasAuthority HasAuthority_is_constant_true_on_AOGBrawlerSimulationManager_use_worldIsAuthority_or_runsPrediction

template <typename SessionT>
class BrawlerSessionHooksUImpl
{
public:
	using Manager        = BrawlerCompositionUImpl::Manager;
	using Storage        = BrawlerCompositionUImpl::Storage;
	using RenderSnapshot = BrawlerCompositionUImpl::RenderSnapshot;
	using Session        = SessionT;
	using Component      = UOGBrawlerSimulatableComponent;
	using Character      = AOGBrawlerUECharacter;
	using StaticData     = simulatableBrawler::StaticData;

	template <typename CharacterT = Character>
	static void sampleInput(SessionT& session)
	{
		for (const auto& entry : session.getRegisteredComponentsById())
		{
			auto* const registered = entry.second.Get();
			CharacterT* const character = registered != nullptr ? Cast<CharacterT>(registered->GetOwner()) : nullptr;
			if (character != nullptr)
				character->getInputCollection()->publishSample();
		}
	}

	static float gravity(const StaticData& staticData) { return staticData.m_movementStaticData.gravity; }

	template <typename PhysicsSlotF, typename RecordSlotF, typename SlotCallF, typename CharacterT = Character>
	static decltype(auto) callWithSlotRequest(AActor& owner, uint64_t key, bool isAuthority, PhysicsSlotF&& physicsSlotFor,
		RecordSlotF&& recordSlot, SlotCallF&& slotCall)
	{
		const CharacterT* const character = Cast<CharacterT>(&owner);
		checkf(character != nullptr,
			TEXT("callWithSlotRequest: '%s' is not an AOGBrawlerUECharacter. Only that class's pawns register: the session's ")
			TEXT("first registration pass builds the bodies from its root capsule, and the slot the authority chooses reaches ")
			TEXT("the clients on that character's og-sim-net roster entry. Was the class checkf of tryAcquireJoltSlot (the ")
			TEXT("slot rode a replicated property of that class until og-input-latency task P5-7)."),
			*owner.GetName());
		const FTCHARToUTF8 name(*owner.GetName());
		return slotCall(key, name.Get(), isAuthority,
			physicsSlotFor(toStorageKey(character != nullptr ? character->GetSimCharacterId() : SimCharacterId::None)),
			std::forward<RecordSlotF>(recordSlot));
	}
};

using BrawlerSessionHooks = BrawlerSessionHooksUImpl<UESimulationSession<BrawlerSessionTraitsUImpl>>;

#undef HasAuthority
