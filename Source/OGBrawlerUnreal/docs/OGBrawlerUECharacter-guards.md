<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `OGBrawlerUECharacter.h` and `.cpp` — guards

Every prohibition that survived the task-25 conversion of the pawn's two files. Each entry has an
**opaque, stable id**. In the source, a single line `// ⛔G-nn  docs/OGBrawlerUECharacter-guards.md`
sits directly above the statement where the forbidden edit would be typed.

**If this file and the source disagree, the source is authoritative and this file is stale.**

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time
check, moves to §R and its number is spent forever.

⭐ The join is machine-checked in both directions by this repository's guard-tag lint. ⚠ It checks
that an entry EXISTS, never that its text is TRUE.

Orientation, provenance, history and the corrections found before the move live in
`OGBrawlerUECharacter-rationale.md`. Text in `>` blocks is the shipped bytes of the comment the
entry replaced.

---

## G-01 — the capsule is 42 / 96 because the movement descriptor says so; never change one side alone

**Site:** `GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);` in the constructor.

> ⛔⛔ 42 / 96 IS A CONTRACT. `brawlerMovementSimulation::PhysicsSetup::body` ships
> `CapsuleGeometry{42.f, 96.f}` with `isRoot`, and `ChaosPhysicsFactory`'s adopt-root branch
> `checkf`s the authored capsule AGAINST the descriptor instead of resizing it — a different
> number here asserts at the first character registration.

> ⛔⛔ 42 / 96 IS A CONTRACT, NOT A TUNING VALUE. […] A different size in the constructor is an
> immediate assert at the first character registration. Change both together or neither.

**Consequence.** A size that disagrees fails `ChaosPhysicsFactory::createPhysicalObject`'s
adopt-root `checkf` (radius and half-height compared with `FMath::IsNearlyEqual`) at the first
registration. ⚠ That `checkf` compiles out when `DO_CHECK == 0`, and a Shipping build would then
run with a body whose size disagrees with the simulation's.

**What breaks if the tag moves.** The mirror is also quoted by line in `SimulatableBrawlerTypes.h`
and `BrawlerMovementSimulation-rationale.md`. Neither is checked.

---

## G-02 — the capsule subobject keeps the engine's old name, `CollisionCylinder`

**Site:** `CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionCylinder"));`.

> ⛔ THE SUBOBJECT NAME IS DELIBERATELY THE ENGINE'S OLD ONE. `[PhysicsFactory.AdoptRoot]`
> logs the adopted component's NAME, and every PIE transcript on this initiative (task 15
> §GATE 1 onwards) records `CollisionCylinder`. Keeping it makes "PIE parity with task 15"
> a line-for-line comparison instead of a judgement call.

**Consequence.** A rename compiles and runs; it breaks the line-for-line comparison of
`[PhysicsFactory.AdoptRoot]` lines against the recorded transcripts, and it renames a default
subobject that serialized instances refer to by name.

---

## G-03 — the engine's stock movement component is gone; do not re-introduce a second authority over the capsule

**Site:** an absence tag in the constructor, after the three `bUseControllerRotation*` writes,
where a movement component would be created.

> ⛔⛔ THE ENGINE'S STOCK MOVEMENT COMPONENT IS GONE — not disabled, DELETED WITH THE BASE
> CLASS. Locomotion is `brawlerMovementSimulation`; do not re-introduce a second authority
> over this capsule.

**Consequence.** The capsule is simulated by the physics factory and written every tick by the
movement sub-simulation (`drivesBody`). A second writer (a movement component, or re-basing the
class on the engine's walking-pawn base) fights the simulation and the correction path.

---

## G-04 — the capsule's restitution stays 0

**Site:** `CapsulePhysicalMaterial->Restitution = 0.f;` in the constructor.

> RESTITUTION 0 IS LOAD-BEARING (revision 6): step 6' measures the solver's positional
> push-out and adopts it, so a bouncy capsule would feed a rebound back into the movement
> state.

**Consequence.** Any restitution above 0 turns contacts into a rebound that step 6' adopts as
movement. Friction 0 is recorded in the rationale but is not relied on, so it has no guard.

---

## G-05 — actor-movement replication stays OFF

**Site:** `this->SetReplicateMovement(false);` in the constructor.

> [movement-sim task 15] ⛔ ACTOR MOVEMENT REPLICATION OFF (R6). The capsule's pose is
> carried by the simulation's own state wire and reproduced on every peer by
> `brawlerMovementSimulation::integrate` step 5, then corrected through the resim path.
> Leaving UE's actor-movement replication on would put a SECOND, unsynchronised copy of
> the same pose on the wire and fight the correction with visible rubber-banding.

---

## G-06 — `kRideHeightMirror` mirrors `m_movementStaticData.rideHeight`; change both together

**Site:** `static constexpr float kRideHeightMirror = 10.f;` in the constructor.

> ⚠ THE 10.f MIRRORS `m_movementStaticData.rideHeight` (`SimulatableBrawlerTypes.h`, the
> one place it is authored) exactly as the 42/96 capsule dimensions mirror
> `InitCapsuleSize` above — the sub-simulation is engine-free and cannot read this file,
> so the pairing is maintained by hand. CHANGE BOTH TOGETHER or the character walks with
> its feet 10 cm into, or 10 cm above, the floor.

**What breaks if the tag moves.** Nothing checks the pair. `rideHeight` is a constructor argument
of a runtime-built `StaticData`, so a `static_assert` cannot reach it.

---

## G-07 — `RebuildHumanoidMesh` ends by re-applying the brawler tint; never drop the call

**Site:** `ApplyBrawlerColor();` at the end of `RebuildHumanoidMesh`.

> ⛔ MUST BE LAST. Every createSection above sets the BASE material, so a rebuild
> silently reverts the brawler tint. This re-applies it. Do not move this above
> the sections, and do not drop it when adding a fourth section.

**Consequence.** Without it every rebuild leaves the mesh in the base material, and the tint the
scoreboard's swatch column matches is gone from the world. The **ordering** half ("do not move
this above the sections") is an ordering prohibition a tag cannot stop; it is kept, untagged, in
the rationale §4.

---

## G-08 — `BrawlerColor` replicates with a plain `DOREPLIFETIME`, never `COND_OwnerOnly`

**Site:** `DOREPLIFETIME(AOGBrawlerUECharacter, BrawlerColor);`.

> ⛔ NOT COND_OwnerOnly — every client must see every brawler's colour.

**Consequence.** Under `COND_OwnerOnly` every other machine's brawlers stay white on this machine,
in the world and in the scoreboard's swatch column.

---

## G-09 — `RingoutScore` replicates with a plain `DOREPLIFETIME`, never `COND_OwnerOnly`

**Site:** `DOREPLIFETIME(AOGBrawlerUECharacter, RingoutScore);`.

> ⛔⛔ [ringout task 5] NOT COND_OwnerOnly EITHER, AND HERE IT IS LOAD-BEARING RATHER
> THAN MERELY CORRECT. A scoreboard shows EVERYONE's score to EVERYONE: a viewer needs
> the other three rows far more than their own, which they could have read locally.

**Consequence.** Under `COND_OwnerOnly` every row for another machine's brawlers freezes at its
last value (0 for a brawler that joined after). ⚠ The shipped block also claimed it would starve
split-screen siblings on the SAME machine; that is false (rationale §6.2, C-4). A split-screen
player's child connection counts as its parent's owner.

---

## G-10 — `SimCharacterIdValue` replicates with a plain `DOREPLIFETIME`: never `COND_OwnerOnly`, never `COND_InitialOnly`

**Site:** `DOREPLIFETIME(AOGBrawlerUECharacter, SimCharacterIdValue);`.

**Prohibition (task 25, new).** Keep the plain form.
* `COND_OwnerOnly`: a client registers every remote proxy under that proxy's replicated id, so
  it needs every character's id, not just its own. Under `COND_OwnerOnly` a proxy's id never
  arrives, and its registration waits at `SimCharacterId::None` until the component's
  600-attempt `checkf` fires.
* `COND_InitialOnly`: the authority assigns the id in
  `USimmableUpdateComponent::tryRegisterWithNewFramework`, one or more frames after the
  component's `BeginPlay`. That is normally after the pawn's first bunch, so the initial-only
  send carries 0 and the id is never sent. Every client then waits until the same `checkf`.

**What breaks if the tag moves.** Nothing mechanical checks the condition, and the LLT target
cannot reach replication. The failure shows only in a networked PIE run, as a client stuck
`Pending`.

---

## G-11 — `OnRep_RingoutScore`'s log line prints the SIMULATION id, never the pawn's

**Site:** the `OGBLOG_G("[Warning][Ringout.score.client] id=%u score=%d", …)` statement.

> ⛔ THE SIM ID, NOT THE PAWN'S. See GetSimCharacterId() - every other `id=%u` in the
> simulation's logs is the SimmableUpdateComponent's, so printing the pawn's would make
> this line unjoinable with the [Ringout.death] and [Ringout.spawnSlot] lines beside it.

**Correction (task 25).** "The SimmableUpdateComponent's" is now "the peer-stable
`SimCharacterId`", and the argument is `toStorageKey(GetSimCharacterId())`.

**Why a tag and not the type.** `%u` also accepts `AActor::GetUniqueID()`'s `uint32`, so
substituting the pawn's id here still compiles.

---

## G-12 — `SetAuthoritativeRingoutScore` does not self-call `OnRep_RingoutScore`

**Site:** an absence tag at the end of `SetAuthoritativeRingoutScore`.

> ⚠ NO `OnRep_RingoutScore()` CALL HERE, and the asymmetry with `PossessedBy`'s
> `ApplyBrawlerColor()` is deliberate. That one self-calls because the listen-server
> host's own pawn gets no OnRep and the tint would otherwise never be APPLIED. Nothing
> is applied here — the scoreboard reads the property directly — so a self-call would
> only duplicate the diagnostic line on the host and make the log lie about which side
> received what.

---

## G-13 — the material parameter is named `Color`, once, at this write site

**Site:** `HumanoidColorMID->SetVectorParameterValue(TEXT("Color"), BrawlerColor);` in
`ApplyBrawlerColor`.

> `Color` is BasicShapeMaterial's vector parameter. A wrong name here fails
> SILENTLY — the mesh renders in the base tint and nothing logs — so it is
> named once, at this single write site, rather than being spread around.

---

## G-14 — the colour counter advances on the authority only

**Site:** `if (HasAuthority())` in `PossessedBy`.

> ⛔ SERVER-ONLY. Never read or written on a client — a client's count would
> diverge from the authority's and hand two brawlers the same tint.

> Authority only: PossessedBy does not run on clients, but HasAuthority() is
> stated rather than assumed because the counter must never advance twice.

**What breaks if the tag moves.** The counter `gNextBrawlerColorIndex` is declared at file scope,
where a tag would not stand on the line a wrong read or write is typed. This branch holds its
only read and its only write.

---

## G-15 — `PossessedBy` applies the tint itself: the listen-server host's pawn gets no OnRep

**Site:** `ApplyBrawlerColor();` inside `PossessedBy`'s `HasAuthority()` branch.

> The listen-server's own pawn gets no OnRep, so apply here too.

**Consequence.** Without it the host's own brawlers render in the base tint on the host.

---

## G-16 — `SetupPlayerInputComponent` re-adds the input mapping context after initializing the translator

**Site:** the `InputCollection->addInputMappingContextForController(Controller);` statement in
`SetupPlayerInputComponent`.

> CRITICAL: for client-side mid-game-spawned pawns (LP1+ via CreatePlayer),
> the IMC-add calls in OnRep_Controller / PossessedBy / BeginPlay all fire
> BEFORE this initialize() because client-side possession/replication order
> is different from LP0's game-start order. Those earlier addToSubsystem
> calls are silent no-ops because m_inputTranslator's mapping context is
> still null. We re-add here so the joining LP's Enhanced Input subsystem
> actually receives the IMC and can match incoming gamepad events.

**Consequence.** Deleting it leaves a mid-game-joined local player with no mapping context and
no input. Nothing logs: `InputMappingUETranslator::addToSubsystem` returns silently on a null
context.

---

## §R — Retired ids

None.
