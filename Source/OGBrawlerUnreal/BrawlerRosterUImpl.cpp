// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerRosterUImpl-rationale.md · docs/BrawlerRosterUImpl-guards.md

#include "OGBrawlerUnreal/BrawlerRosterUImpl.h"

#include "OGSimulation/CompilerControl.h"
#include "OGSimulation/OGAssert.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGBrawlerUnreal/SimmableUpdateComponent.h"

#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

#include <array>
#include <utility>
#include <vector>

OGSIM_OPTIMIZE_OFF

SimCharacterId BrawlerRosterUImpl::allocateSimCharacterId(ENetMode netMode)
{
    checkf(netMode != NM_Client,
           TEXT("allocateSimCharacterId called on a client world's manager. Only the authority assigns ")
           TEXT("simulation character ids; a client learns each one through the pawn's replicated ")
           TEXT("SimCharacterIdValue."));

    const SimCharacterId simId = m_simCharacterIds.allocate();
    // ⛔G-02  docs/BrawlerRosterUImpl-guards.md
    if (simId == SimCharacterId::None)
    {
        UE_LOG(LogOGMgmt, Error,
            TEXT("allocateSimCharacterId: registration REFUSED. This authority has already assigned all %u ")
            TEXT("simulation character ids and never reuses one, so this character does not register. ")
            TEXT("Widening SimCharacterId or reusing ids is a user ruling (R2), not a fix to make here."),
            static_cast<unsigned int>(SimCharacterIdAllocator::kLastAssignable));
        OG_CHECK(false, "allocateSimCharacterId: all SimCharacterIds are spent; registration refused (R2)");
        return SimCharacterId::None;
    }

    UE_LOG(LogOGMgmt, Log, TEXT("allocateSimCharacterId: assigned id=%u"), toStorageKey(simId));
    return simId;
}

void BrawlerRosterUImpl::seedSpawnPointsFromLevel(UWorld& world, ENetMode netMode, bool compositionExists,
                                                  simulatableBrawler::StaticData& staticData)
{
	checkf(!m_ringoutSpawnPointsSeeded,
		TEXT("seedSpawnPointsFromLevel: the ring-out spawn table has already been ")
		TEXT("seeded. It is a ONE-TIME init, and a second pass would merge the level over its ")
		TEXT("own previous answer rather than over the authored defaults."));

	checkf(!compositionExists,
		TEXT("seedSpawnPointsFromLevel: the manager or integration layer already ")
		TEXT("exists, so a simulation tick may already have read StaticData::spawnPoints. ")
		TEXT("This seed must run BEFORE the first integrate - move it back above the role ")
		TEXT("branches in BeginPlay rather than relaxing this check."));

	std::vector<brawlerRingout::LevelSpawnPoint> placements;
	for (TActorIterator<APlayerStart> it(&world); it; ++it)
	{
		const APlayerStart* playerStart = *it;
		if (playerStart == nullptr)
			continue;

		brawlerRingout::LevelSpawnPoint placement;
		placement.name     = TCHAR_TO_UTF8(*playerStart->GetName());
		placement.position = uglm::toGLMVec3(playerStart->GetActorLocation());
		placements.push_back(std::move(placement));
	}

	const std::array<glm::vec3, brawlerRingout::kMaxSpawnPoints> authoredFallback =
		staticData.m_ringoutStaticData.spawnPoints;

	staticData.m_ringoutStaticData.spawnPoints =
		brawlerRingout::spawnPointsFromLevelPlacements(placements, authoredFallback);

	m_ringoutSpawnPointsSeeded = true;

	UE_LOG(LogOGMgmt, Warning,
		TEXT("[Ringout.spawnPoints] seeded from the level: %d APlayerStart actor(s) found, ")
		TEXT("%d slot(s) filled, %d left authored (netMode=%d)"),
		static_cast<int32>(placements.size()),
		FMath::Min(static_cast<int32>(placements.size()),
			static_cast<int32>(brawlerRingout::kMaxSpawnPoints)),
		static_cast<int32>(brawlerRingout::kMaxSpawnPoints)
			- FMath::Min(static_cast<int32>(placements.size()),
				static_cast<int32>(brawlerRingout::kMaxSpawnPoints)),
		static_cast<int32>(netMode));

	for (uint32 slot = 0u; slot < brawlerRingout::kMaxSpawnPoints; ++slot)
	{
		const glm::vec3& point = staticData.m_ringoutStaticData.spawnPoints[slot];
		UE_LOG(LogOGMgmt, Warning,
			TEXT("[Ringout.spawnPoints]   slot %u -> (%.2f, %.2f, %.2f) [%s]"),
			slot, point.x, point.y, point.z,
			(slot < placements.size()) ? TEXT("level") : TEXT("authored"));
	}
}

void BrawlerRosterUImpl::checkSeeded(ENetMode worldNetMode) const
{
	checkf(m_ringoutSpawnPointsSeeded,
		TEXT("SimulationManagerUImpl::BeginPlay: the ring-out spawn table was not seeded on this peer ")
		TEXT("(netMode=%d). Every peer reads StaticData::spawnPoints - a client predicts its own ")
		TEXT("respawn - so seedSpawnPointsFromLevel must not be role-gated. Was the NOT ")
		TEXT("ROLE-GATED fence at the seed call."),
		static_cast<int32>(worldNetMode));
}

void BrawlerRosterUImpl::onBodiesCreated(SimulatableBrawler& simulatable, AOGBrawlerUECharacter& character,
                                         unsigned int id, bool isAuthority, ENetMode netMode)
{
    const BodyId movementOwnBodyId =
        simulatable.getPhysicsComposite()
            .get<brawlerMovementSimulation::PhysicsDeclaration>().bindings.ownBodyId;
    const BodyId movementParentBodyId =
        simulatable.getPhysicsComposite()
            .get<brawlerMovementSimulation::PhysicsDeclaration>().bindings.parentBodyId;

    checkf(movementOwnBodyId == movementParentBodyId,
           TEXT("tryRegister: the movement declaration's own body (%u) is not the pawn's root ")
           TEXT("capsule (%u), id=%u. Either CharacterBindings is being stamped before the physics ")
           TEXT("fold populated bindings.ownBodyId, or the factory created a body instead of ")
           TEXT("adopting the capsule. Was the stamp-after-the-fold and isRoot fences."),
           movementOwnBodyId.value, movementParentBodyId.value, id);

    simulatable.setCharacterBindings({ /*.capsuleBodyId =*/ movementOwnBodyId });

    auto& movementIC = simulatable.editAllState().editState()
        .edit<brawlerMovementSimulation::InitialConditions>();
    movementIC.teleportPending = 1u;
    movementIC.teleportPos     =
        uglm::toGLMVec3(character.GetCapsuleComponent()->GetComponentLocation());

    if (isAuthority)
    {
        checkf(isAuthority == (netMode != NM_Client),
               TEXT("tryRegister: isAuthority disagrees with the world-level net mode; the ")
               TEXT("spawn-slot table must be populated on the authority world only"));

        auto& ringoutIC = simulatable.editAllState().editState()
            .edit<brawlerRingout::InitialConditions>();

        // ⛔G-03  docs/BrawlerRosterUImpl-guards.md
        ringoutIC.spawnSlot = m_spawnSlots.acquire(id);

        UE_LOG(LogOGMgmt, Log,
            TEXT("tryRegister: ring-out spawnSlot=%u assigned to id=%u"),
            ringoutIC.spawnSlot, id);
    }
}

void BrawlerRosterUImpl::onRegistered(unsigned int id, bool isAuthority)
{
    // ⛔G-04  docs/BrawlerRosterUImpl-guards.md
    if (isAuthority)
    {
        m_authorityRegisteredIds.insert(id);
        const int32 registered = static_cast<int32>(m_authorityRegisteredIds.size());
        if (registered > kPreDietCharacterCap)
        {
            UE_LOG(LogOGNet, Warning,
                TEXT("[PreDietCap] character %d exceeds pre-diet cap %d — input-loss margins "
                     "unsafe (T44/T38 §16); land item 40"),
                registered, kPreDietCharacterCap);
        }
    }
}

void BrawlerRosterUImpl::forgetCharacter(unsigned int id)
{
    m_authorityRegisteredIds.erase(id);

    // ⛔G-05  docs/BrawlerRosterUImpl-guards.md
    m_spawnSlots.release(id);
}

void BrawlerRosterUImpl::pushRingoutScores(
    const brawlerRingout::ScoreSystem& scoreSystem,
    const std::unordered_map<unsigned int, TWeakObjectPtr<USimmableUpdateComponent>>& routes)
{
    for (const auto& entry : routes)
    {
        const unsigned int id = entry.first;

        USimmableUpdateComponent* component = entry.second.Get();
        if (component == nullptr)
            continue;

        AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(component->GetOwner());
        if (character == nullptr)
            continue;

        checkf(scoreSystem.hasScoreEntry(id),
               TEXT("Ring-out score push: id=%u is in the authority route table but has no ")
               TEXT("ScoreSystem roster entry. The game-thread read of the score table is ")
               TEXT("safe only because the roster is seeded by onCharacterRegistered before ")
               TEXT("tryRegister returns Ready - an unseeded id lets postIntegrate's ")
               TEXT("operator[] INSERT, and rehash, on the physics thread under this walk."),
               id);

        character->SetAuthoritativeRingoutScore(
            static_cast<int32>(scoreSystem.scoreOf(id)));
    }
}

OGSIM_OPTIMIZE_ON
