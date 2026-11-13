<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: EncroachingBlockingGeometry -- Unreal Engine UWorld method (Engine/World.h), outside every scan root -->
<!-- lint-external-ref: FindTeleportSpot -- Unreal Engine UWorld method (Engine/World.h), outside every scan root -->
<!-- lint-external-ref: APlayerStartPIE -- Unreal Engine class (Engine/PlayerStartPIE.h), outside every scan root -->
<!-- lint-external-ref: AGameModeBase::ChoosePlayerStart_Implementation -- Unreal Engine method (GameModeBase.cpp), outside every scan root -->
<!-- lint-external-ref: GameModeBase.cpp -- Unreal Engine source file (Runtime/Engine/Private), outside every scan root -->
# `BrawlerPlayerStartUImpl` — rationale

`BrawlerPlayerStartUImpl.h` and `.cpp` (BUSL-1.1, `Source/OGBrawlerUnreal/`), og-input-latency task 2d, 2026-11-11. The
files state no prohibition, so they carry a one-path docs pointer and there is no guards doc.

## 1. What it does

`brawlerPlayerStartUImpl::choosePlayerStart(gameMode, player)` is the engine's PlayerStart pick with one filter added. It
skips every `APlayerStart` whose height is at or below the ring-out kill plane plus
`brawlerRingout::kSpawnPointKillPlaneMargin`, using the same predicate as the ring-out seed,
`brawlerRingout::spawnPointClearsKillPlane`. `AOGBrawlerUEGameMode::ChoosePlayerStart_Implementation` calls it and falls back
to the engine's own choice when it returns `nullptr`.

The rest is `AGameModeBase::ChoosePlayerStart_Implementation` (UE 5.6, `GameModeBase.cpp`), step for step:
- the first `APlayerStartPIE` ("Play from Here") wins at once, unfiltered, as in the engine;
- a start where the default pawn does not encroach blocking geometry (`EncroachingBlockingGeometry`) is *unoccupied*;
  otherwise, if `FindTeleportSpot` finds room near it, it is *occupied*;
- a random unoccupied start, else a random occupied one (`FMath::RandRange`).

## 2. Why the engine's spawn needs the filter too

The seed's rejection (`BrawlerRosterUImpl-rationale.md` §1a) keeps a below-plane start out of the ring-out respawn table.
It does nothing for the pawn's first spawn, which is the engine's random pick over every `APlayerStart` in the level. A
pawn spawned on a rejected start falls below the plane, dies one tick after its first integrate, and respawns two seconds
later. That is the idle `[Ringout.score.client]` change task 2b saw, and in task 2c it hit a third of all joins. So the
filter has to be in both places, and both use one predicate.

## 3. The kill plane it reads

`brawlerRingout::StaticData{}.killPlaneHeight`: the shipped default, -5 m. The game mode has no reference to the session's
`simulatableBrawler::StaticData`. Its `m_ringoutStaticData` is default-constructed and nothing writes `killPlaneHeight`
(`SimulatableBrawlerTypes.h`; the seed writes only `spawnPoints`), so the two values are the same. If a level ever gets its
own kill plane, this read must move to the session's value.

## 4. When every start is rejected

The helper logs one `LogOGMgmt` Warning, `[Ringout.playerStart] no APlayerStart clears the kill plane ...`, with the
rejected count, and returns `nullptr`. The game mode then takes the engine's choice, which is today's behaviour: a level
with no usable start spawns as it did before this task rather than not at all.

## 5. Determinism

Only the authority runs a game mode, and the pawn's spawn location reaches the simulation through the registration
teleport seed (`BrawlerRosterUImpl-rationale.md` §2), which every peer reads from the replicated capsule. The random pick
is the engine's and was never peer-identical; nothing here changes that.
