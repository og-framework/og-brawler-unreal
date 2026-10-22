// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerRosterUImpl-rationale.md · docs/BrawlerRosterUImpl-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include <set>
#include <type_traits>
#include <unordered_map>

#include "OGBrawler/SimCharacterId.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawler/SimulatableBrawler.h"
#include "OGBrawler/BrawlerRingoutSimulation.h"
#include "OGBrawler/BrawlerRingoutScoreSystem.h"

class AOGBrawlerUECharacter;
class UOGBrawlerSimulatableComponent;
class UWorld;

class BrawlerRosterUImpl
{
public:
    // ∴D-01  docs/BrawlerRosterUImpl-rationale.md
    static constexpr int32 kPreDietCharacterCap = 4;
    static_assert(static_cast<uint32>(kPreDietCharacterCap) <= brawlerRingout::kMaxSpawnPoints,
        "kPreDietCharacterCap must not exceed brawlerRingout::kMaxSpawnPoints. The cap only WARNS - "
        "it allocates nothing - while the ring-out spawn table has exactly kMaxSpawnPoints entries, so "
        "a character registered past the table gets kNoFreeSlot and respawns with NO teleport seed (one "
        "[Warning][Ringout.spawnSlot] per respawn). A cap above the table stops warning about exactly the "
        "characters that cannot be placed: raise both together. Was the 'COUPLED TO "
        "brawlerRingout::kMaxSpawnPoints' prose fence of OGBrawlerSimulationManager.h (task 11); moved from "
        "AOGBrawlerSimulationManager by og-ue-simulationcomponentandmanagersplit task 23.");

    SimCharacterId allocateSimCharacterId(ENetMode netMode);

    // ⛔G-01  docs/BrawlerRosterUImpl-guards.md
    void seedSpawnPointsFromLevel(UWorld& world, ENetMode netMode, bool compositionExists,
                                  simulatableBrawler::StaticData& staticData);

    void checkSeeded(ENetMode worldNetMode) const;

    void onBodiesCreated(SimulatableBrawler& simulatable, AOGBrawlerUECharacter& character,
                         unsigned int id, bool isAuthority, ENetMode netMode);

    void onRegistered(unsigned int id, bool isAuthority);

    void forgetCharacter(unsigned int id);

    static void pushRingoutScores(
        const brawlerRingout::ScoreSystem& scoreSystem,
        const std::unordered_map<unsigned int, TWeakObjectPtr<UOGBrawlerSimulatableComponent>>& routes);

private:
    std::set<unsigned int> m_authorityRegisteredIds;
    static_assert(std::is_same_v<decltype(m_authorityRegisteredIds), std::set<unsigned int>>,
        "m_authorityRegisteredIds is the pre-diet cap's denominator and must be a SET, not a counter: "
        "tryRegister inserts only on the Ready path, while unregisterFromNewFramework erases for any "
        "component ending play - one abandoned mid-Pending included - so a counter drifts downward and "
        "disarms the cap. Was the 'A SET, not a counter' prose fence of OGBrawlerSimulationManager.h "
        "(task 11); moved from AOGBrawlerSimulationManager::compositionContractsHold by "
        "og-ue-simulationcomponentandmanagersplit task 23.");

    brawlerRingout::SpawnSlotAllocator m_spawnSlots;

    SimCharacterIdAllocator m_simCharacterIds;

    bool m_ringoutSpawnPointsSeeded = false;
};
