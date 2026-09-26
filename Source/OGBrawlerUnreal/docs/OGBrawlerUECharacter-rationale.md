<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: Character.cpp -- the engine's walking-pawn source file, read through the GitHub API; the engine is not vendored into this repository -->
<!-- lint-external-ref: impl_notes_ringout_5.md -- a private workspace document of the brawler-ringout-gamemode initiative, quoted verbatim; not in this repository -->
<!-- lint-external-ref: UActorChannel::ReplicateActor -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: DataChannel.cpp -- Unreal Engine source file, outside every scan root -->
<!-- lint-external-ref: bIsPushBased -- Unreal Engine lifetime-property flag; named because this module does NOT use it -->
<!-- lint-external-ref: InputCollection::onMove -- quoted verbatim: the pawn's member InputCollection, whose class is UOGBrawlerInputCollectionComponent; the real member is UOGBrawlerInputCollectionComponent::onMove -->
<!-- lint-external-ref: SkeletalMeshComponent -- quoted verbatim as an ABSENCE claim: no such component exists in this module -->
<!-- lint-external-ref: AnimInstance -- quoted verbatim as an ABSENCE claim: no such class is used in this module -->
<!-- lint-external-ref: m_weaponVis -- deleted commented-out code, recorded as history; it must NOT resolve -->
<!-- lint-external-ref: AddCustomPhysics -- Unreal Engine body-instance method; named because this module never calls it -->
# `AOGBrawlerUECharacter` — rationale

Companion to `Source/OGBrawlerUnreal/OGBrawlerUECharacter.h` and
`Source/OGBrawlerUnreal/OGBrawlerUECharacter.cpp`. The source files carry the code and one-line
guard tags; the prohibitions are in `OGBrawlerUECharacter-guards.md`, and everything else that
used to be a comment in those two files is here. Text quoted in `>` blocks is the shipped bytes
of the comment it replaced; where a quoted claim was found false before the move (R0), the
correction sits directly under the quote and in §13.

Related documents, which remain authoritative for their own subjects:
`ScoreboardDisplay-rationale.md` (how the scoreboard joins this pawn's replicated score and tint
to simulation state), `SimmableUpdateComponent-rationale.md` (registration),
`SimulationManagerUImpl-rationale.md` (the score push and the id allocator's owner).

---

## 1. The root collision capsule — created and owned by this class

> [movement-sim task 19] ⭐ THE ROOT COLLISION CAPSULE — CREATED AND OWNED BY THIS CLASS.
> Until this task the pawn derived from the engine's stock walking-pawn base, which created
> this component, named it, made it the root, and handed its movement component the very
> same component to drive. That movement component was retired in task 15 — the movement
> sub-simulation owns locomotion — so the base class was doing exactly ONE useful thing for
> us: a handful of constructor lines. They live in our own constructor now, and the base is
> `APawn`.

> [movement-sim task 19] ⭐ THE ROOT CAPSULE, CREATED HERE. These are the lines the engine's
> stock walking-pawn base used to run for us — read off `Character.cpp`'s constructor at
> `ref=5.6` through the GitHub API (the local engine tree is out of bounds on this
> initiative), kept in the same order, and given this project's capsule size. Nothing else
> that base class provided was still in use by the time this task ran, so the whole of the
> inheritance is replaced by what you can read below.

The 42 / 96 contract, the subobject name, and the removed stock movement component are guards
G-01, G-02 and G-03.

> ⚠ THE FOUR SETTINGS AFTER THE SIZE ARE CARRIED OVER FOR BEHAVIOUR NEUTRALITY, not because
> anything in this project reads them today: the collision profile is what the capsule
> answers with between spawn and registration (`ChaosPhysicsFactory::applyDescriptor` then
> sets object type, responses, simulate-physics and gravity from the descriptor and owns it
> from that point on), `SetShouldUpdatePhysicsVolume` keeps physics-volume tracking on, and
> the two navigation/step-up bits are inert with no other stock-movement pawn in the game.
> They are kept so this migration moves nothing observable; drop them in a task that says so.

Verified 2026-09-26: `ChaosPhysicsFactory::applyDescriptor` sets simulate-physics, gravity and the
collision object type from the descriptor (`Source/OGSimulationUnreal/ChaosPhysicsFactory.cpp`).
The block counts "four settings" and then lists five calls after the size (profile, step-up,
physics volume, affect-navigation, dynamic obstacle); the reasoning covers all five.

`GetCapsuleComponent()`:

> Returns the root collision capsule.
> ⭐ DELIBERATELY THE SAME NAME the engine base class used to provide, so all seven
> pre-existing call sites across three files are unchanged by the migration. This is also the
> component the movement sub-simulation ADOPTS as its body (`PhysicsSetup::body.isRoot`),
> which is why its authored size is a contract rather than a tuning value — see the
> capsule member above.

"Seven call sites across three files" is a count at migration time; on 2026-09-26 the tree has
14 `GetCapsuleComponent()` calls in four files, 11 of them in this pawn's own two files.

### 1.1 The stock movement component, and what keeps the capsule single-authority

> [movement-sim task 15] Every tuning knob that used to live on it (JumpZVelocity,
> AirControl, the walk-speed cap, the analog-walk floor, the braking decelerations) is now
> authored ONCE in `simulatableBrawler::StaticData::m_movementStaticData`
> (`OGBrawler/SimulatableBrawlerTypes.h`), which is the sub-simulation's StaticData and
> the only place any of them is spelled. `Move()`, its engine movement-input call, and the
> per-Tick walk-speed-cap resync were all deleted in the same change.

> ⭐ [movement-sim task 19] TASK 15 COULD ONLY SWITCH IT OFF; THIS TASK REMOVED IT. What
> stood here were three belt-and-braces disable calls, plus one include kept alive on
> purpose to serve them. The calls went with the base class that declared their accessor,
> the include went with the calls, and there is no component left to disable.

> ⛔ WHAT KEEPS THE CAPSULE SINGLE-AUTHORITY IS UNCHANGED, AND IT IS A PAIR:
> `brawlerMovementSimulation::PhysicsSetup::body.simulatePhysics = true` makes the physics
> factory's adopt-root pass simulate THIS capsule, and `StaticData::drivesBody = true` makes
> the sub-simulation the one thing that writes it. `simulatePhysics` alone leaves a free
> rigid body nobody drives; `drivesBody` alone would put the sim back in a fight with
> whatever else moved the component. See the paired comments on `StaticData::drivesBody` and
> `PhysicsSetup::body` in `BrawlerMovementSimulation.h`.

**Correction (R0).** The "paired comments" no longer exist. `BrawlerMovementSimulation.h` was
converted, and the `simulatePhysics` half of the pair is now a `static_assert` on
`kCharacterCapsuleBody` whose message names both halves. `StaticData::drivesBody` defaults to
`true` at its declaration.

## 2. The capsule's physical material

> [movement-sim task 15] Friction 0 / restitution 0, assigned to the capsule as a
> primitive phys-material OVERRIDE in the constructor. Held as a UPROPERTY so the
> default subobject is rooted for the lifetime of the CDO/instance and cannot be
> collected out from under `SetPhysMaterialOverride`. Restitution 0 is load-bearing:
> step 6' of the movement sub-simulation adopts the solver's positional push-out, so a
> bouncy capsule would feed a rebound straight back into the movement state.

> ⭐ FRICTION 0 / RESTITUTION 0 ON THE CAPSULE, and the mechanism is named: a
> `UPhysicalMaterial` default subobject assigned as the primitive's override, so it needs
> no content asset and cannot be un-set by a Blueprint that forgets to inherit one.
> RESTITUTION 0 IS LOAD-BEARING (revision 6): step 6' measures the solver's positional
> push-out and adopts it, so a bouncy capsule would feed a rebound back into the movement
> state. FRICTION 0 is recorded rather than relied on — the sim re-writes
> {position, velocity} every tick, so a tangential friction impulse has nothing to
> accumulate into. Mass is deliberately NOT asserted (revision 6: mass plays no part in
> the movement path).

The restitution prohibition is guard G-04.

## 3. Constructor settings: camera, networking, cosmetic components

Trailing and one-line comments removed from the constructor, with their status:

| removed comment | status |
|---|---|
| `// Don't rotate when the controller rotates. Let that just affect the camera.` | true (the three `bUseControllerRotation*` are `false`) |
| `// Create a camera boom (pulls in towards the player if there is a collision)` | describes `USpringArmComponent`'s default collision test; kept as history |
| `// The camera follows at this distance behind the character` (on `TargetArmLength = 900`) | true |
| `// Rotate the arm based on the controller` (on `bUsePawnControlRotation = false`) | **false** — the value is `false`, so the arm does NOT follow the controller; the camera is driven by `dAttackCameraBehaviour` in `Tick` (R0, §13) |
| `// Create a follow camera`, `// Attach the camera to the end of the boom …`, `// Camera does not rotate relative to arm` | true |
| `// Create a PhysicsComponent` | **false** — no physics component is created (R0, §13) |
| `//a Camera settings` (above `m_cameraAxis`) | label only |
| `// Higher = more updates per second`, `// Always relevant for network updates`, `// Distance at which the character is culled from the network updates` | true, restate the identifiers |

> Tracks the Chaos async-physics tick rate (60 Hz; Config/DefaultEngine.ini
> AsyncFixedTimeStepSize). Replicating faster than the sim produces new states
> just wastes bandwidth, so this moves in lockstep with the tick-rate flip (T15).

Verified: `Config/DefaultEngine.ini` sets `AsyncFixedTimeStepSize=0.016667`. Since task 25 this
frequency also bounds how soon a client learns its pawn's `SimCharacterIdValue` after the
authority assigns it (§9).

> [movement-sim task 15] ⛔ ACTOR MOVEMENT REPLICATION OFF (R6). The capsule's pose is
> carried by the simulation's own state wire and reproduced on every peer by
> `brawlerMovementSimulation::integrate` step 5, then corrected through the resim path.
> Leaving UE's actor-movement replication on would put a SECOND, unsynchronised copy of
> the same pose on the wire and fight the correction with visible rubber-banding.

Guard G-05.

> [movement-sim task 19] THE INHERITED SKELETAL-MESH COMPONENT IS GONE with the base class.
> It was `SetVisibility(false)` at construction from its first commit, and nothing in this
> module ever animated off it: the visible body is `HumanoidMesh` below, a
> `UProceduralMeshComponent` built in C++ from `HumanoidVisualization`.
> ⚠ STATED PRECISELY: the SPAWNED pawn uses no skeletal mesh — `OGBrawlerUEGameMode` sets
> `DefaultPawnClass` to this C++ class DIRECTLY. `Content/ThirdPerson/Blueprints/`
> `BP_ThirdPersonCharacter.uasset` (last touched 2024-02-08, referenced by no other asset
> and by no map) does still assign a mannequin and an AnimBP to the component this task
> removed — which is why the honest claim is about the spawned pawn, not about the project.

Verified 2026-09-26: `AOGBrawlerUEGameMode`'s constructor assigns
`DefaultPawnClass = AOGBrawlerUECharacter::StaticClass()`, and the `.uasset` still exists. The
asset-reference claims were not re-checked (binary assets).

## 4. The humanoid mesh

> [movement-sim task 15] THE CAPSULE HOVERS NOW, so the feet no longer sit at
> `-CapsuleHalfHeight`. `brawlerMovementSimulation`'s hover servo holds the capsule
> CENTRE at `rideHeight` above the surface, so the mesh must drop by the half-height AND
> the ride height for the feet to touch the ground.
> ⚠ THE 10.f MIRRORS `m_movementStaticData.rideHeight` (`SimulatableBrawlerTypes.h`, the
> one place it is authored) exactly as the 42/96 capsule dimensions mirror
> `InitCapsuleSize` above — the sub-simulation is engine-free and cannot read this file,
> so the pairing is maintained by hand. CHANGE BOTH TOGETHER or the character walks with
> its feet 10 cm into, or 10 cm above, the floor.

Verified: `SimulatableBrawlerTypes.h` passes `/*rideHeight=*/ 10.f`. Guard G-06. A
`static_assert` was not possible: `rideHeight` is a constructor argument of a runtime-built
`StaticData`, not a `constexpr` this file can read.

> The one dynamic instance the tint is written to. Created lazily because
> RebuildHumanoidMesh() re-applies the BASE material to every section.

> ⛔ MUST BE LAST. Every createSection above sets the BASE material, so a rebuild
> silently reverts the brawler tint. This re-applies it. Do not move this above
> the sections, and do not drop it when adding a fourth section.

Dropping the call is guard G-07. **Moving it above the sections** is an ordering prohibition,
which a tag cannot stop (rule §9.3, `no (ordering)`), so it lives here with no tag: the call must
stay after the last `createSection`, including any section added later.

> `Color` is BasicShapeMaterial's vector parameter. A wrong name here fails
> SILENTLY — the mesh renders in the base tint and nothing logs — so it is
> named once, at this single write site, rather than being spread around.

Guard G-13.

> RebuildHumanoidMesh() re-applies the BASE material to every section, so the
> MID must be re-applied after any rebuild, not just once at possession.

## 5. Brawler colour — a distinct tint per brawler, server-assigned, replicated

> BRAWLER COLOUR — a distinct tint per brawler, server-assigned, replicated.
>
> ⛔ KEYED PER BRAWLER, NOT PER CONNECTION. This game supports couch co-op:
> AOGBrawlerPlayerController::JoinLocalPlayer calls CreatePlayer(ControllerId=-1),
> so several players share ONE connection. Anything keyed on the connection or its
> address gives every sibling on a couch the same colour. The counter below is
> bumped once per POSSESSION, which is per brawler.
>
> Stable for a whole run, deliberately NOT stable across runs: a
> leaver's slot is never reused, so a rejoining player gets the next colour rather
> than their old one.

Verified: `AOGBrawlerPlayerController::JoinLocalPlayer` calls
`UGameplayStatics::CreatePlayer(world, /*ControllerId=*/-1, …)`.

> Hand-picked so neighbours stay distinguishable for a colour-blind reader and
> against the level's grey; 10 entries for a 6-brawler target, so the wrap below
> is a safety net rather than an expected path.

The palette entries carried trailing colour names: red, blue, amber, green, violet, cyan,
orange, pink, lime, indigo. ⚠ "a 6-brawler target" disagrees with every other figure in the
tree (the advisory `kPreDietCharacterCap` and `brawlerRingout::kMaxSpawnPoints` are both 4, and
the block below says "≤ 4-player session"); recorded as stale (§13).

> ⭐⭐ [ringout task 10] THE SCOREBOARD NOW DRAWS THIS TINT INSTEAD OF A RAW CHARACTER
> ID, so two brawlers sharing one is no longer merely confusing in the world — it makes
> two ROWS OF THE BOARD indistinguishable, which is the one thing that column exists to
> prevent. The `%` wrap below is what could do it, and this is the gate that says it
> cannot for any board this project can draw.
>
> ⛔ THE BOUND IS THE SCOREBOARD'S OWN ROW CAP, NOT A FOURTH COPY OF THE 4. The board
> draws at most `kScoreboardMaxRows` rows, so it needs at least that many distinct tints;
> naming that constant rather than writing a literal is what stops this becoming yet
> another mirror of `kPreDietCharacterCap` / `brawlerRingout::kMaxSpawnPoints` — a
> duplication that OGBrawler/docs/BrawlerScoreboardVisualization-rationale.md section 8
> calls out by name. It is also the STRONGER bound: the advisory character cap is 4 and
> the row cap is 8.
>
> ⚠ WHAT THIS DOES *NOT* PROMISE, WRITTEN DOWN BECAUSE THE ASSERT LOOKS STRONGER THAN IT
> IS. The counter is bumped once per POSSESSION and never reclaims a leaver's index, so
> the guarantee is "the first `kBrawlerPaletteCount` possessions of a run get distinct
> tints", not "no two concurrent brawlers ever share one". A run with enough joins and
> rejoins to wrap past ten CAN hand a newcomer the tint of a fighter who never left. That
> is pre-existing behaviour of the palette, unchanged here, and it is far outside the
> ≤ 4-player session this mode is built for — but it is the honest bound.
>
> ⛔ THIS ASSERT IS ONE OF ONLY THREE MECHANISMS THAT REACH A `Source/OGBrawlerUnreal`
> FILE (finding F26: the compiler, a lint's file glob, and a pure header a Catch2 case
> can read). Shrinking the palette below the row cap is a compile error, not a review
> finding.

Verified: `kScoreboardMaxRows = 8u`, `kPreDietCharacterCap = 4`, `kMaxSpawnPoints = 4u`, and
`BrawlerScoreboardVisualization-rationale.md` §8 is the `kScoreboardMaxRows` section. The
`static_assert` enforces the bound, so it has no guard.

> ⛔ SERVER-ONLY. Never read or written on a client — a client's count would
> diverge from the authority's and hand two brawlers the same tint.

> Authority only: PossessedBy does not run on clients, but HasAuthority() is
> stated rather than assumed because the counter must never advance twice.

> The listen-server's own pawn gets no OnRep, so apply here too.

`gNextBrawlerColorIndex` has one reader and one writer, both inside `PossessedBy`'s
`HasAuthority()` branch; that branch is guard G-14 and the host's self-apply is guard G-15. The
counter is a file-scope variable, so it is per process and never reset: it keeps counting
across PIE sessions in one editor process, which only moves which colour comes first.

## 6. Replicated state on this class

Three properties replicate: `BrawlerColor`, `RingoutScore` and, since task 25,
`SimCharacterIdValue`. All three use a plain `DOREPLIFETIME` (guards G-08, G-09, G-10).

> ⛔ REPLICATED. Purely cosmetic: it never reaches the engine-free simulation
> core, so it cannot affect determinism or the correction path.
> ⚠ [ringout task 5] IT IS NO LONGER THE ONLY REPLICATED STATE ON THIS CLASS —
> `RingoutScore` below is the second, and it is safe for exactly this reason.

**Correction (R0, task 25).** "The second" is now "one of three", and the third is **not**
cosmetic: `SimCharacterIdValue` is the simulation character id. It never enters a composite or
the correction wire (it rides pawn replication only, so it costs the correction wire 0 B), but
it is the value every peer registers the character under, so a wrong or missing value
changes which storage entry the peer simulates. The "safe because cosmetic" argument applies to
`BrawlerColor` and `RingoutScore` only.

### 6.1 `RingoutScore` never reaches the simulation core

> [ringout task 5] ⭐ THE REPLICATED RING-OUT SCORE — THE SCOREBOARD'S FIRST INPUT.
>
> ⛔ COSMETIC ON THE CLIENT, AND THAT IS THE PROPERTY THAT MAKES REPLICATING IT SAFE.
> Exactly the claim `BrawlerColor` above makes about itself: this value never reaches
> the engine-free simulation core. It is not in `simulatableBrawler::State`, not in any
> composite, has no `SerializableFields` specialization, rides no correction buffer and
> is read by nothing that integrates, rewinds or reconciles. A client that receives a
> wrong, stale or missing score draws a wrong NUMBER; it cannot desynchronise, cannot
> change a prediction and cannot make a resim land differently. The authority-side
> ledger it mirrors — `brawlerRingout::ScoreSystem`'s table — is likewise outside sim
> state, which is what task 4 built it that way for.
> ⇒ THE GREP THAT PROVES IT, in two parts, re-runnable, over the two engine-free core
>   trees (`Plugins/OGBrawler/.../og-brawler/` and `Plugins/OGSimulation/.../og-simulation/`):
>     1. The four identifiers that carry this value — `RingoutScore`,
>        `OnRep_RingoutScore`, `GetRingoutScore`, `SetAuthoritativeRingoutScore` — have
>        ZERO occurrences in either tree.
>        ⚠ Grep for the bare word and you get three hits. They are task 4's TEST-CASE
>        NAMES (`RingoutScore.TheAwardCostsTheCompositeNothing` and two more) quoted in
>        comments — a substring collision with a different symbol, not a read. Match the
>        identifier, not the word: `RingoutScore[^.A-Za-z0-9_]`.
>     2. Structural, and it is the stronger half: those trees contain no engine code at
>        all. Their only mentions of `AOGBrawlerUECharacter`, `UPROPERTY` and
>        `DOREPLIFETIME` are in comments and markdown. There is nothing there that COULD
>        read a `UPROPERTY` even if it named one.
>   Both commands, with their output, are in `impl_notes_ringout_5.md` §3.

Re-run 2026-09-26 over the `.h`/`.cpp` files of both core trees: part 1 (identifier form) has 0
hits, and part 2 has 0 non-comment hits. `impl_notes_ringout_5.md` is in the private
`brawler-ringout-gamemode` workspace, not in this repository.

> ⚠ SIGNEDNESS: `brawlerRingout::ScoreSystem` holds the score as `uint32_t`. This is
> `int32` because that is the integer width UE replicates, reflects and exposes without
> qualification; the push casts once, at the single write site below. One point per
> death makes either width unreachable overflow.

Verified: `ScoreSystem::m_scores` is `std::unordered_map<unsigned int, uint32_t>`. This is the
correction `BrawlerRingoutScoreSystem-rationale.md` §9 pairs with.

### 6.2 Replication conditions, and couch co-op

> ⛔ NOT COND_OwnerOnly — every client must see every brawler's colour.

> ⛔⛔ [ringout task 5] NOT COND_OwnerOnly EITHER, AND HERE IT IS LOAD-BEARING RATHER
> THAN MERELY CORRECT. A scoreboard shows EVERYONE's score to EVERYONE: a viewer needs
> the other three rows far more than their own, which they could have read locally.
> ⚠ AND THIS PROJECT RUNS COUCH CO-OP — several brawlers share ONE connection, and only
> ONE of the pawns on that connection is that connection's owner. Under COND_OwnerOnly a
> four-player, two-machine session would replicate each machine's FIRST pawn and starve
> every sibling sitting next to it: player 2's scoreboard would show player 1's score,
> its own row frozen at whatever it last was, and both remote rows frozen at 0. The
> failure is silent, looks like "the score isn't updating", and reproduces only with
> more than one pawn per connection — which is the configuration the game is for.
> ⇒ The extra cost of the plain form is one int32 per character per CHANGE (the push
>   below writes nothing when the value is unchanged), not per tick.

> ⛔ PLAIN `DOREPLIFETIME`, NEVER `COND_OwnerOnly` — see `GetLifetimeReplicatedProps`.

**Correction (R0).** The couch co-op mechanism above is false. A split-screen player added with
`CreatePlayer` owns a `UChildConnection` whose parent is the machine's connection, and the engine
treats that parent as the owner: `UActorChannel::ReplicateActor` sets `RepFlags.bNetOwner` when
the owning connection is a child of the channel's connection (`DataChannel.cpp`, UE 5.6). So
`COND_OwnerOnly` would deliver every pawn on a machine to that machine, siblings included. What
it would starve is every **other** machine: the remote rows would freeze. The prohibition stands,
for the first reason the block gives (everyone's score is shown to everyone). The last line's
cost claim is also imprecise: without push-model replication the engine compares the property
against its shadow copy on every net update and sends only a change, whether or not the push
writes (§7).

**`SimCharacterIdValue`** (task 25) uses the plain form for two reasons, both guard G-10:
* not `COND_OwnerOnly`: every client must learn every character's id, because a client registers
  each remote proxy under that id. Under `COND_OwnerOnly` a proxy's id would never arrive, and
  the proxy's registration would wait until the retry budget's `checkf`;
* not `COND_InitialOnly`: the authority assigns the id inside
  `USimmableUpdateComponent::tryRegisterWithNewFramework`, at least two frames after the
  component's `BeginPlay` (`BeginPlay` schedules `tryInitializeWithManager` for the next tick,
  and that schedules `tryRegisterWithNewFramework` for the tick after). Assignment therefore
  normally lands after the pawn's first replication bunch has gone out. `COND_InitialOnly` would never send the id, and the
  client would wait until the 600-attempt `checkf`.

## 7. The score's two doors — one in, one out

> [ringout task 5] THE SCORE'S TWO DOORS — one in, one out.

`GetRingoutScore()`:

> The ring-out score as this peer last heard it. ⭐ THIS IS WHERE THE SCOREBOARD (task 6b)
> READS THE SCORE, on EVERY peer: on a client it is whatever `RingoutScore` last
> replicated in, on the authority it is whatever the push below last wrote. There is no
> second route and deliberately so — a HUD that reached into `ScoreSystem` on the
> authority and into this property on a client would be two code paths, one of which
> nobody ever looks at.
> ⚠ The scoreboard's OTHER two inputs are NOT here and must NOT be replicated again:
> dead and ticks-until-respawn are `brawlerRingout::State`, already on every peer via the
> correction wire. See `impl_notes_ringout_5.md` §4 for the exact accessors.

`GetBrawlerColor()`:

> ⭐ [ringout task 10] THE TINT THIS BRAWLER'S MESH IS WEARING — the SCOREBOARD'S SWATCH.
>
> ⛔ PRESENT AND CORRECT ON EVERY PEER, which is the property that lets the board draw a
> real colour on a client instead of a column of white. `BrawlerColor` is assigned
> server-side in `PossessedBy` from `kBrawlerPalette` and registered with a PLAIN
> `DOREPLIFETIME` — deliberately NOT `COND_OwnerOnly`; the reason is written at the
> registration and it is couch co-op. It therefore reaches a client's scoreboard by
> exactly the same route `RingoutScore` above does, and column one works on a client for
> exactly the reason column two does.
>
> ⚠ THE SCOREBOARD DROPS THE ALPHA. `brawlerScoreboardVisualization::ScoreboardInk` is
> three channels; the swatch is opaque on purpose, because a translucent swatch is a
> different colour to the eye over a bright scene than over a dark one, and "which
> fighter is this" must not depend on what is behind the board.
>
> ⚠ IT IS COSMETIC, LIKE THE PROPERTY ITSELF. Nothing here reaches the engine-free
> simulation core; see the note at `BrawlerColor`'s declaration.

"The reason is written at the registration and it is couch co-op" is imprecise: the
`BrawlerColor` registration gave "every client must see every brawler's colour"; the couch co-op
argument was at `RingoutScore`'s registration, and §6.2 corrects it. `ScoreboardInk` is three
floats (`r`, `g`, `b`): verified.

`SetAuthoritativeRingoutScore()`:

> ⛔ AUTHORITY ONLY, AND IT IS A NO-OP WHEN THE VALUE IS UNCHANGED.
> Called once per character per game-thread post-physics pass by
> `ASimulationManagerUImpl::OnPostPhysicsStep`. The unchanged-value early-out is the
> whole reason this is a method rather than a public field: replication marks a property
> dirty on ASSIGNMENT, not on change, so an unguarded per-pass write would put a
> never-changing score on the wire 60 times a second, for every character, forever.

> [ringout task 5] THE SINGLE WRITE SITE FOR THE REPLICATED SCORE.

> ⛔ HasAuthority() IS LEGITIMATE HERE, AND THAT IS WORTH STATING BECAUSE IT IS NOT
> LEGITIMATE ON THE PUSHER. `ASimulationManagerUImpl` sets `bReplicates = false`, which
> pins its Role to authority on every peer and makes `HasAuthority()` a CONSTANT there —
> that file warns about it twice and the push uses the world-level `GetNetMode()` test
> instead. This class is an `APawn`, whose constructor sets `bReplicates = true`, so its
> Role is genuinely assigned by the network and this test genuinely discriminates. It is
> a `checkf` rather than a silent early-out because a client reaching this is a routing
> defect, not a condition to tolerate: the value would be overwritten by the next
> correction from the server anyway, hiding the bug.

**Correction (R0).** Two parts are false. (1) The push is gated on `!runsPrediction()`, not on
`GetNetMode()`, and the manager's `HasAuthority()` warning is now one compile-time poison (a
`#define` in `SimulationManagerUImpl.cpp`), not two warnings. This was already routed by
`SimulationManagerUImpl-rationale.md` §13.10. (2) The value would be overwritten by the next
**replication** of `RingoutScore`, not by a correction: the score is not on the correction wire
(§6.1). The rest is true: `ASimulationManagerUImpl`'s constructor sets `bReplicates = false`, and
`APawn`'s constructor sets `bReplicates = true`.

> ⛔ THE UNCHANGED-VALUE GUARD. See the declaration: UE marks a replicated property dirty
> when it is ASSIGNED, and the comparison is against the last SENT value only for
> properties the replication system re-compares — which costs the comparison on the
> server for every character every net update. Returning here makes the common case (no
> death this pass, i.e. almost every pass) cost one int compare and nothing else.

**Correction (R0).** "Marks a property dirty on ASSIGNMENT" is false for this class. Push-model
replication is not used here (no `bIsPushBased` and no `MARK_PROPERTY_DIRTY` in the module), so
the replication layout compares every replicated property against its shadow copy on every net
update and sends only what changed. An unguarded per-pass write of an unchanged score costs one
assignment and puts nothing on the wire. The early-out is harmless and cheap, and it is **not**
load-bearing for bandwidth, so it carries no guard.

> ⚠ NO `OnRep_RingoutScore()` CALL HERE, and the asymmetry with `PossessedBy`'s
> `ApplyBrawlerColor()` is deliberate. That one self-calls because the listen-server
> host's own pawn gets no OnRep and the tint would otherwise never be APPLIED. Nothing
> is applied here — the scoreboard reads the property directly — so a self-call would
> only duplicate the diagnostic line on the host and make the log lie about which side
> received what.

Guard G-12 (an absence tag at the end of the function).

## 8. `OnRep_RingoutScore` — the client-side arrival line

> [ringout task 5] For OGBLOG_G — OnRep_RingoutScore's one diagnostic line. The sink it
> routes through is installed on BOTH roles by ASimulationManagerUImpl::BeginPlay.

Verified: `ASimulationManagerUImpl::BeginPlay` calls `ogblog::setGlobal` in both its authority and
its client branch. The sink is one process-global, so in a single-process PIE the last manager
to begin play owns it.

> [ringout task 5] ⭐ CLIENT-SIDE ARRIVAL. There is no client-side WORK to do — the
> scoreboard reads `GetRingoutScore()` at draw time and nothing is derived from the value —
> so this hook exists for the one thing prose cannot give task 8's PIE run: a per-peer,
> per-change record that the value CROSSED THE WIRE. Replication behaviour is not reachable
> from the low-level-test target, so this line is the live verification.
>
> CADENCE: one line per character per score CHANGE, which is at most one per death tick.
> ⛔ THE `[Warning]` PREFIX IS DELIBERATE AND IS NOT NOISE-BLINDNESS. `ogblog`'s sink routes
> every message to `LogOGBrawler`, which `Config/DefaultEngine.ini` ships at `=Warning`, so a
> bare `[Ringout.*]` tag is INVISIBLE in a default PIE run. The prefix raises this one line
> above that setting. (⚠ `ScoreSystem::postIntegrate`'s own `[Ringout.score]` award line has
> no prefix and IS silent by default — task 8 must raise `LogOGBrawler` to see the authority
> half. Reported in `impl_notes_ringout_5.md` §6.)
>
> ⚠ The sink is installed by `ASimulationManagerUImpl::BeginPlay`. A score arriving before
> that — which requires the manager to be missing, i.e. no simulation at all — is swallowed.

**Correction (R0).** `Config/DefaultEngine.ini` now ships `LogOGBrawler=Verbose`, not `Warning`.
Both the bare `[Ringout.*]` lines and this one print in a default PIE run, so the prefix is no
longer what makes the line visible. It is kept because it is harmless and it still survives a
lowered verbosity. It carries no guard. The routing half is true: the sink maps a `[Warning]`
prefix to `UE_LOG(LogOGBrawler, Warning, …)`.

> ⛔ THE SIM ID, NOT THE PAWN'S. See GetSimCharacterId() - every other `id=%u` in the
> simulation's logs is the SimmableUpdateComponent's, so printing the pawn's would make
> this line unjoinable with the [Ringout.death] and [Ringout.spawnSlot] lines beside it.

**Correction (R0, task 25).** Since task 25 every `id=%u` is the peer-stable `SimCharacterId`
carried on this pawn, not the component's `GetUniqueID()`. The prohibition stands and is guard
G-11: `%u` also accepts `GetUniqueID()`'s `uint32`, so the compiler does not catch that
substitution.

## 9. The simulation character id (task 25)

`SimCharacterIdValue` is a replicated `uint8`, the carrier of the `SimCharacterId` type declared
in `OGBrawler/SimCharacterId.h`. `GetSimCharacterId()` returns it as `SimCharacterId`, and
`SetAuthoritativeSimCharacterId()` is its one write site.

The declaration's pre-task-25 prose, kept as history:

> ⭐ THE JOIN KEY — the id this character is known by INSIDE the simulation, on every peer.
>
> ⛔ IT IS NOT `AActor::GetUniqueID()` ON THIS PAWN, AND THAT DISTINCTION IS THE WHOLE
> REASON THIS ACCESSOR EXISTS. Every id in the simulation — `SimulationObjectStorage`'s
> key, `brawlerRingout::ScoreSystem`'s roster key, `SpawnSlotAllocator`'s table, the
> input-history rings, every `id=%u` in every `[Ringout.*]` log line — is the
> `USimmableUpdateComponent`'s `GetUniqueID()`, because that is what
> `tryRegisterWithNewFramework` passes to `tryRegister`. The pawn's own id is a DIFFERENT
> number, and a scoreboard that joined on it would silently match nothing.
>
> ⭐ THIS IS HOW THE SCOREBOARD (task 6b) JOINS ITS THREE INPUTS. Iterate
> `AOGBrawlerUECharacter` actors for the SCORE (`GetRingoutScore()`, replicated, present
> on every peer), key each row by this id, and read DEAD and RESPAWN-AT-TICK for that same
> id out of the simulation — see the note on `GetRingoutScore` and
> `impl_notes_ringout_5.md` §4.
>
> Returns 0 if the component is missing, which cannot happen on a constructed pawn.

> [ringout task 5] The join key. See the declaration for why it is not GetUniqueID().

**Correction (R0, task 25).** The id is no longer the component's `GetUniqueID()`, and the
"component is missing" case no longer exists: the accessor reads `SimCharacterIdValue`. What is
true today:
* The authority assigns the id once, through `ASimulationManagerUImpl::allocateSimCharacterId`
  (count-up 1..255, never reused; see `SimCharacterId-guards.md` G-01), and
  `USimmableUpdateComponent::tryRegisterWithNewFramework` writes it here with
  `SetAuthoritativeSimCharacterId`. Its three `checkf`s enforce authority-only,
  never-`None` and assign-once.
* The value is `SimCharacterId::None` (0) until the authority assigns it, or on a client until it
  replicates in. A client's registration waits for a non-zero id.
* The id is **peer-stable**: the same character has the same id on every peer. The standing
  fact "ids are per process" is retired by this task.
* The pawn's own `AActor::GetUniqueID()` is still a different number, and a join on it still
  matches nothing. Since task 25 the accessor's return type is a distinct `enum class`, so passing
  it where `unsigned int` is expected, or `GetUniqueID()` where `SimCharacterId` is expected, is a
  compile error. A call site converts explicitly with `toStorageKey`. The scoreboard's join
  (`ScoreboardDisplay-rationale.md` §2.1) is unchanged in shape.
* `GetSimCharacterId()`'s `static_assert` pins `SimCharacterIdValue` to exactly the underlying
  byte of `SimCharacterId`.

## 10. Input mapping context — four add sites

> Client-side mirror of PossessedBy's IMC-add. Fires when the server-
> authoritative Controller pointer replicates to this client-side pawn.
> Required in PIE-as-client / dedicated-server-client setups where
> PossessedBy is server-only.

> Belt-and-suspenders IMC-add — works for LP0 at game start (BeginPlay
> deferred until after possession). For mid-game-spawned pawns (LP1+),
> PossessedBy and OnRep_Controller cover the cases this site misses;
> SetupPlayerInputComponent re-adds after the translator is initialized.

> Server-side hook for mid-game-spawned pawns: fires after Controller is
> wired, which BeginPlay's IMC-add misses because BeginPlay can run before
> Possess on the server.

> Client-side mirror: in PIE-as-client / dedicated-server-client setups
> PossessedBy fires only on the server. On the client, the Controller
> pointer replicates in and OnRep_Controller fires here.

> Initialize translator here — SetupPlayerInputComponent runs before BeginPlay (during possession).

> CRITICAL: for client-side mid-game-spawned pawns (LP1+ via CreatePlayer),
> the IMC-add calls in OnRep_Controller / PossessedBy / BeginPlay all fire
> BEFORE this initialize() because client-side possession/replication order
> is different from LP0's game-start order. Those earlier addToSubsystem
> calls are silent no-ops because m_inputTranslator's mapping context is
> still null. We re-add here so the joining LP's Enhanced Input subsystem
> actually receives the IMC and can match incoming gamepad events.

Verified: `InputMappingUETranslator::addToSubsystem` does nothing while its mapping context is
null. "this initialize()" means `InputCollection->initializeTranslator()`. The re-add is guard
G-16.

> Set up action bindings

> [movement-sim task 15] The character's own Move binding is GONE with the CMC path.
> The Move action now has exactly ONE consumer: `InputCollection::onMove`, which caches
> the stick for `buildPlayerInput()` to relay to the simulation on the physics tick.

## 11. `Tick`

> [movement-sim task 15] The per-frame walk-speed-cap resync is GONE with the CMC path.
> ⭐ [movement-sim task 17] AND THE WINDOW IT OPENED IS CLOSED. This block used to warn that
> `OGBrawler.MoveSpeed` had NO READER "until task 16" and that the console lever was inert.
> Task 16 has landed: `readMovementStaticDataCVars()` (MovementSchemeCVar.cpp) reads the cvar
> ONCE, when the manager builds its `StaticData`, into the movement sub-simulation's
> `maxWalkSpeed`. Mid-session changes are still ignored — that is the design, and each sink
> says so out loud — but the lever is no longer inert.

> rotate the character mesh

> [movement-sim task 19] The write to the inherited skeletal mesh's world rotation stood
> here and went with that component. It was a DUPLICATE: same `getRotationMatrix` call,
> same `(0, 1, 0)` default axis and same `tmpAimInput` as the `HumanoidMesh` write below,
> on an invisible component. Nothing read the result back — there is no other `GetMesh()`,
> and no `SkeletalMeshComponent` or `AnimInstance` anywhere in this module.

> [movement-sim task 19] Anchored on the CAPSULE, and that is a VALUE NO-OP. The inherited
> skeletal mesh this used to read was attached to the capsule at an identity relative
> transform — the base class attached it and never offset it, this constructor never moved
> it, and the spawned pawn is this C++ class rather than a Blueprint that could override it
> — so its world translation WAS the capsule's. Same sphere, same place, one fewer component.

## 12. History — removed code and orientation comments

* **Commented-out code, deleted by the task-25 conversion.** In the header's public section:
  `//UPROPERTY(Category = "Weapon", VisibleAnywhere, BlueprintReadWrite)`,
  `//UStaticMeshComponent* m_weaponVis;`, the same `UPROPERTY` line again, and
  `//TArray<USplineComponent*> splines;`. In the constructor: `//m_volume` and
  `//m_volumeAssets->emplace_back(DSphereAsset{ 1.f });`. None of them named a live symbol.
* **Orientation banners, deleted:** `// AOGBrawlerUECharacter` and `// Input` under
  `//////` rules, the bare `//` line above the net-frequency block, and the Doxygen one-liners
  `Camera boom positioning the camera behind the character`, `Follow camera`,
  `Returns CameraBoom subobject`, `Returns FollowCamera subobject`,
  `Returns InputCollection subobject`.
* **`PhysicsTick`.** Its declaration carried `Event called every physics tick and sub-step.` and
  `OnCalculateCustomPhysics` carried `Custom physics Delegate`. **Correction (R0):**
  `OnCalculateCustomPhysics` is bound in the constructor but never handed to a body
  (`AddCustomPhysics` is not called anywhere in the module), so `CustomPhysics`, and through it
  `PhysicsTick`, is never called. The pair is dormant.
* **The flinch-freeze predicate.**
  > [movement-sim task 15] The flinch-freeze predicate is DELETED. Its only caller was `Move()`,
  > the legacy CMC path, and the flinch freeze it implemented now lives in the simulation
  > itself: `brawlerMovementSimulation::integrate` step 1 gates on `machineFreezesMovement`,
  > which reads the machine sub-simulation's own state on the sim clock instead of a
  > game-thread viz snapshot. One authority, one clock.

  Verified: `machineFreezesMovement` exists in `BrawlerMovementSimulation.h` and gates the
  movement integrate.
* **The optimize pair.**
  > [og-netcode-v2-input-relay item 77] Closes the OGSIM_OPTIMIZE_OFF opened
  > above (AOGBrawlerUECharacter ctor). The file had no closing pragma since its
  > initial commit, so the whole rest of the TU compiled unoptimized in every
  > build — closing the pair here, at true end-of-file, changes nothing (there
  > was no code after this point either), it just makes the always-off scope an
  > explicit pair instead of an implicit "off to EOF".
* "To add mapping context" sat above `BeginPlay`'s declaration, and "APawn interface" above
  `SetupPlayerInputComponent`'s.

## 13. Corrections found before the move (R0), task-25 conversion, 2026-09-26

| # | claim as shipped | finding |
|---|---|---|
| C-1 | `GetSimCharacterId()` is the component's `GetUniqueID()`; "Returns 0 if the component is missing" | false since task 25 (§9) |
| C-2 | `OnRep_RingoutScore`: every `id=%u` is "the SimmableUpdateComponent's" | false since task 25 (§8) |
| C-3 | `BrawlerColor`/`RingoutScore` are "the only"/"the second" replicated state, all safe because cosmetic | three properties now; `SimCharacterIdValue` is not cosmetic (§6) |
| C-4 | couch co-op: `COND_OwnerOnly` replicates only each machine's first pawn | false: child connections count as owner; it starves other machines (§6.2) |
| C-5 | replication "marks a property dirty on ASSIGNMENT", so the unchanged-value early-out saves bandwidth | false without push model (§7) |
| C-6 | the pusher "warns about it twice and … uses the world-level `GetNetMode()` test"; the score is overwritten "by the next correction" | gated on `!runsPrediction()`; one compile-time poison; overwritten by replication, not correction (§7) |
| C-7 | `LogOGBrawler` ships at `=Warning`, so bare `[Ringout.*]` lines are invisible | the ini ships `Verbose` (§8) |
| C-8 | "See the paired comments on `StaticData::drivesBody` and `PhysicsSetup::body`" | comments converted to a `static_assert` (§1.1) |
| C-9 | `PhysicsTick`: "Event called every physics tick and sub-step" | never called: the custom-physics delegate is never registered (§12) |
| C-10 | `// Create a PhysicsComponent` | nothing is created (§3) |
| C-11 | `bUsePawnControlRotation = false; // Rotate the arm based on the controller` | the value says the opposite (§3) |
| C-12 | palette: "10 entries for a 6-brawler target" | every other figure is 4 (§5) |
| C-13 | "all seven pre-existing call sites across three files" | a migration-time count; 14 calls in four files today (§1) |
