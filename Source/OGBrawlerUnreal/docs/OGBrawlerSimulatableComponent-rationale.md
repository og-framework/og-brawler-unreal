<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: ASimulationManagerUImpl -- RENAMED by og-ue-simulationcomponentandmanagersplit task 30 to AOGBrawlerSimulationManager; kept in the §11 history and named in its marker -->
<!-- lint-external-ref: ASimulationManagerUImpl::BeginPlay -- RENAMED by og-ue-simulationcomponentandmanagersplit task 30 to AOGBrawlerSimulationManager::BeginPlay; kept in the §11 history -->
<!-- lint-external-ref: SimulationManagerUImpl-rationale.md -- RENAMED by og-ue-simulationcomponentandmanagersplit task 30 to OGBrawlerSimulationManager-rationale.md (ids unchanged); kept in the §11 history -->
# `UOGBrawlerSimulatableComponent` — rationale

New to this code? Start with [SimulationArchitecture.md](SimulationArchitecture.md).

Companion to `Source/OGBrawlerUnreal/OGBrawlerSimulatableComponent.h` and `OGBrawlerSimulatableComponent.cpp`.
Since the task-25 conversion both files carry only code and one-line tags. **The prohibitions are in
`OGBrawlerSimulatableComponent-guards.md`, one id per site; this file carries the reasoning, the provenance
and the worked derivations.** The old `§N` marks are gone from the source; §12 records where every
pre-conversion comment went.

Sibling document: `Source/OGBrawlerUnreal/docs/OGBrawlerSimulationManager-rationale.md`, which owns the
manager side of every boundary named below. Where a fact belongs to the manager it is cited there
rather than restated here.

**Since og-ue-simulationcomponentandmanagersplit task 20a the character's reflected wire surface is a
second component**, `USimulatableNetSyncComponent` (OGSimulationUnreal, MPL-2.0;
`SimulatableNetSyncComponent-rationale.md`). It owns the replicated correction-state buffer, its
notification and the input RPC; this component binds it and supplies the typed ends of both (§16).
Since task 20b it also holds the link to the character's relay host and sends the connection tier;
this component supplies the relayed ring's stamps and the ring-flush callback (§17).

**Since task 21 the typed half of this component is a CRTP base**, `UESimulatableOwnerBinding<UOGBrawlerSimulatableComponent,
BrawlerSessionTraitsUImpl>` (OGSimulationUnreal, MPL-2.0; `UESimulatableOwnerBinding-rationale.md`): the five binding
overrides, the forwarders, the typed owner-concept members, the sibling pointer, the owner-concept aliases, the five
latency stamp blocks and the stage are its members. This component keeps the registration, the visualization, the
teardown order and one hook, `storageKeyOfOwner()` (§18).

**Since task 29 the registration loop is the binding's too**: a generic skeleton (`UESimulatableOwnerBinding-rationale.md`
§9) with the two polling loops, the attempt budget, the session-owner wait, the local-prediction test, the session's
`tryRegister` and its `Pending` retry, the provider latch, the relay host's spawn or find and the delivery route. This
component keeps the brawler's statements of that loop in four private registration hooks — `initializeWithManager`,
`resolveRegistrationKey`, `makeInputProvider` and `makeSimulatable` — and calls `beginRegistration()` from its
`BeginPlay` (§19).

<!-- ================= DECLARED LINT ESCAPES =================================
     Every token below is CORRECT and cannot resolve. None is here to silence a
     name that is wrong: the wrong names this pass found were FIXED, not escaped
     (§11). Three classes — engine symbols outside every scan root, names that
     are RETIRED and must NOT resolve, and one private-workspace document.
     ========================================================================= -->
<!-- lint-external-ref: LiveLink -- ⛔ MUST NOT RESOLVE. F-34-5: the baseline claimed this component ticks behind a LiveLink component and takes a tick prerequisite on one. No such component exists in this project; the day this name resolves, that claim has become true and Sec 2 must be re-checked -->
<!-- lint-external-ref: LiveLinkComponent -- ⛔ MUST NOT RESOLVE. F-34-8: the argument of the commented-out prerequisite call, deleted with it -->
<!-- lint-external-ref: getLatestInput -- RETIRED with the correction-input column; zero non-comment occurrences in the tree. It must NOT resolve -->
<!-- lint-external-ref: SimulationNetSync::collectInputAll -- DEAD OWNER, quoted in Sec 11 as the defect F-34-3 corrected: collectInputAll moved to SimulationInputResolution. The qualified form must NOT resolve -->
<!-- lint-external-ref: tryRegisterWithManager -- ⛔ FALSE SYMBOL, quoted verbatim in Sec 11 (F-34-9): no such function exists anywhere in the tree. The real ones are tryInitializeWithManager and tryRegisterWithNewFramework -->
<!-- lint-external-ref: m_simulationStateSyncedBuffer -- ⛔ MUST NOT RESOLVE. F-34-8: a member named only by the deleted dead scratch code; it never existed -->
<!-- lint-external-ref: EpicGamesAssignment -- ⛔ MUST NOT RESOLVE. F-34-8: a namespace named only by the deleted dead scratch code; it exists nowhere -->
<!-- lint-external-ref: StructeredLog -- ⛔ MUST NOT RESOLVE. F-34-8: a misspelled engine header named by two deleted commented-out includes; no file of that name exists -->
<!-- lint-external-ref: getSyncedCorrectionInputBuffer -- RETIRED (Sec 12): the authority accessor for the retired correction-input buffer. It must NOT resolve -->
<!-- lint-external-ref: getMachineVizState -- RETIRED (Sec 12): deleted with the pawn flinch-freeze predicate that was its only caller. It must NOT resolve -->
<!-- lint-external-ref: m_onCorrectionInputReceivedCallback -- RETIRED (Sec 12): removed with the correction-input channel. It must NOT resolve -->
<!-- lint-external-ref: USimmableUpdateComponent -- RENAMED by og-ue-simulationcomponentandmanagersplit task 21a to UOGBrawlerSimulatableComponent (this component); named in the §12 history marker -->
<!-- lint-external-ref: SimmableUpdateComponent-guards.md -- RENAMED by og-ue-simulationcomponentandmanagersplit task 21a to OGBrawlerSimulatableComponent-guards.md (this file's guards doc, ids unchanged); kept in the §12 history -->

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes. It is deliberately not in
> the `og-simulation` docs tier, which is MPL-2.0 and travels with a different repository.

**Read §0 first.** It states the shape — one instance per character, the role test, the four
channels, the phase order, the concept surface. Everything below assumes it.

---

## §0 Orientation — `UOGBrawlerSimulatableComponent`

*Moved from the orientation banner at the top of the `.cpp` (task 25), with the phase order brought up to
date.*

One instance per simulated character, on the character actor. This is the **adapter**: it registers
the character with the simulation and supplies the typed ends of that character's wire surfaces — the
correction-state read and the input RPC's tail — whose reflected declarations live on the
`USimulatableNetSyncComponent` sibling since task 20a (§16). It holds no netcode policy of its own —
all of it is core, reached through the manager.

**Binding declaration — this is UE adapter code.** Every engine or project type these files name —
`UOGBrawlerSimulatableComponent`, `AOGBrawlerSimulationManager`, `GEngine`, `ASimulationInputRelay`,
`ASimulationConnectionRelay`, `AOGBrawlerUECharacter`, and equally the replication vocabulary
(`UChildConnection`, `FNetPing`, `COND_SkipOwner`, `COND_OwnerOnly`, `NetSerialize`, Iris, `UObject`,
`FSimulationStateSyncBuffer`) — is one adapter's binding for the role it
names, and another adapter substitutes its own. The engine-free core never sees any of them.

**Role.** One instance runs on each side and they are not the same object. The role test is
`GetNetMode() != NM_Client`, world-level, the same predicate the manager picks its own role with
(the binding's guard G-07 since task 29; this file's G-02 before).

**Phase order.** Every step polls, because every step can legitimately not be ready yet, and "not
yet" is never an error. Since task 29 the two loops are the binding's skeleton, and this component's
part of each is a hook (§19):

```
(the character's PostInitializeComponents: bindNetSync to the sibling, §16)
BeginPlay -> beginRegistration     (the binding) loop one on the next tick
  -> tryInitializeWithManager      (the binding) until the manager exists; then the hook
                                   initializeWithManager: the query volume and the visualization
                                   state (Retry while the Jolt slot waits)
  -> tryRegisterWithNewFramework   (the binding) until the manager exists; then the hook
                                   resolveRegistrationKey: (authority) allocate the pawn's
                                   SimCharacterId, or (client) wait for it to replicate; then the hooks
                                   makeInputProvider (locally predicted only) and makeSimulatable;
                                   then until the bodies resolve; then register, latch the provider
                                   decision, spawn (or find) the relay host, and -- authority only --
                                   register the id -> component route
TickComponent                      visualization only, both roles
EndPlay
  -> unregister, then the sibling's detachInputRelayHost (the authority destroys the relay host, a
     client unbinds it), then unbindNetSync
```

**The concept surface**, and it is why this object has forwarders (members of its binding base since task 21, §18):
the core reaches this object through an accessor pair and a callback pair, and nothing else. The relayed-input ring itself lives on
a separate per-character actor, and the core never learns that (§6). Since task 20a the
correction-state buffer and its callback pair live on the sibling component the same way, reached
through one-line forwarders (§16), and since task 20b so do the ring accessor pair, the relayed-ring
callback pair and the host link (§17).

**Provider presence is the identity test.** A locally-controlled character gets an input provider;
every other character gets none, and provider-*absence* is what gives it a relay store for the core
to predict from (§4).

---

## §1 What this component is, and the four channels

One `UOGBrawlerSimulatableComponent` per simulated character, living on the character actor. It is the
**adapter**: the object that introduces a character to the simulation, holds that character's wire
surfaces, and turns engine primitives into the arguments the engine-free core expects. It decides
nothing about netcode.

Both roles run the same class. In one PIE process the server's world and each client's world run
their own instance against their own manager; a listen server's single world runs the authority role
only. ⚠ **Correction (R0, task 20a).** This said "on a listen server both run in the same process
against two different manager instances"; a listen server fills only the authority slot of
`AOGBrawlerSimulationManager::instanceFor`. The role test is `GetNetMode() != NM_Client` — **world-level**, never
`HasAuthority()`, because that answers `true` on a non-replicated actor and would disagree with the
gate the manager picks its own role with (`AOGBrawlerSimulationManager::instanceFor`).

The four wire channels — the correction state and the input bundle on the character's components, the
relayed input and the connection tier on their own actors — are tabled in
`SimulatableNetSyncComponent-rationale.md` §1, which owns the first two since task 20a. The table moved
there with the replicated surface and was corrected on the way: the correction state reaches every
client the pawn replicates to, not only its owner. This component carries none of the four; it supplies
the typed ends of the first two, and the relayed ring's stamps, through the binding (§16, §17). The
sibling links the relay host and sends the tier (§6, §5 point to its account).

---

## §2 Construction, the tick group, and the optimize pair

Tick enabled, tick group `TG_DuringPhysics`. **The component does not replicate** since task 20a (ruling
Q11): `SetIsReplicatedByDefault(true)` and `GetLifetimeReplicatedProps` left with the replicated buffer
for `USimulatableNetSyncComponent` (§16).

⚠ **The tick group's stated reason was false and is corrected here** (§11, F-34-5). The baseline
said the component ticks after "the LiveLink component" and that *"we also use a tick prerequisite
on LiveLink components"*. Neither half holds: the prerequisite call is commented out on the line
below it, and **the string `LiveLink` occurs nowhere in this project's source outside that comment**
— no such component exists to be ordered behind. What survives is the choice itself: the tick group
is `TG_DuringPhysics`, and nothing in this file depends on a prerequisite.

**The attempt budget moved with the loops** (task 29): `kMaxRegistrationAttempts` and the two attempt counters are the
binding's, and its account is `UESimulatableOwnerBinding-rationale.md` §9.3, moved there game-neutral. For the brawler,
loop one's hook retries are the physics-slot wait (§3), and loop two's are the task-25 id wait (§13). ⚠ The shipped
comment said "~10 seconds at 60 Hz", which reads as the 60 Hz simulation tick; it is one **game frame** per attempt
(§11, F-25-1).

**Deleted commented-out code (task 25).** Two commented-out overrides stood under the constructor in
the header, `ShouldCreatePhysicsState() const override { return true; }` and `OnCreatePhysicsState()`.
Neither was restorable intent anyone had recorded; they were removed as commented-out code.

**The optimize pair.** The file opens `OGSIM_OPTIMIZE_OFF` and — since the pragma-gate pass — closes
`OGSIM_OPTIMIZE_ON` at true end-of-file. It carried no closing pragma from its first commit, so the
whole translation unit, `TickComponent` included, compiled unoptimized in every build. Closing the
pair at the end changed no scope (there was no code after that point either); it made an implicit
"off to EOF" explicit so the gate has a balanced pair to find.

---

## §3 Reaching the manager — two polling loops, and why both poll

**Code moved to `UESimulatableOwnerBinding.h` (task 29):** the two loops, as the binding's skeleton. Their account —
why `BeginPlay` cannot do the work, what each loop waits for, the four things a completed registration does in order,
and that "not yet" is never an error — moved to `UESimulatableOwnerBinding-rationale.md` §9.2, game-neutral. What stays
here is the brawler's part of each loop, in its hooks (§19).

**Loop one's hook, `initializeWithManager`.** Caches the owner's input-collection component, registers the
target-visualization query volume through the manager's session's `registerVizVolume` (§10) and constructs the three
visualization states; the skeleton then schedules loop two.

In the Jolt configuration the volume hangs off the character's Jolt slot, so `registerVizVolume` first
acquires the slot, and it can answer "not yet" (og-simulationscheduler-withjolt task 66): on a client the
slot is the one the authority chose, read from the pawn's replicated `PhysicsSlotValue`, and the client
waits until that value has arrived, and until no departed character still holds that slot on this client.
The session then returns an empty volume id, and the hook answers `Retry` **before** it builds the visualization
states, so the states are only ever built over a registered volume; the skeleton reschedules loop one for the next
frame. The wait counts against the binding's `kMaxRegistrationAttempts` like the manager wait, and its `checkf` names
the owner. The Chaos configuration never waits here.

**Loop two's hooks.** `resolveRegistrationKey` waits for the pawn's `SimCharacterId` (§13) and answers its storage key;
`makeInputProvider` builds the locally predicted character's provider (§4); `makeSimulatable` builds the
`SimulatableBrawler` from the component's static data. The session's `tryRegister` then answers `Pending` until the
character's physics bodies are resolvable, and in the Jolt configuration also while the character holds no slot, which
loop one normally rules out.

---

## §4 Provider presence, and what absence buys

**Moved to `UESimulatableOwnerBinding-rationale.md` §9.4 (task 29), game-neutral:** what a local input provider is, which
owner gets one (on a pure client, the locally controlled one, `ROLE_AutonomousProxy`), and why provider presence is the
core's identity test (`registerLocalCharacter` against `registerRemoteCharacter`, `SimulationInputResolution::collectInputAll`
predicting an absent provider's character from the relay store). The local-prediction test is the binding's skeleton's
(its G-09, this file's G-04 before). What stays here is the brawler's provider, built by the hook `makeInputProvider`.

**The provider lambda captures nothing but the input collection and the id.** It used to capture the
manager, and reached back through it for the motion matcher's input history; the core now hands the
character's own raw-capture delay line straight in as an argument, so the lambda reaches for nothing
outside its arguments. Capturing the manager again would re-create a lifetime edge the registration
ordering does not guarantee.

---

## §5 The connection tier — moved to the sibling (task 20b)

Code moved to `SimulatableNetSyncComponent.cpp` (task 20b): `sendConnectionTierToOwningClient` and its
`ConnectionTierSink` `static_assert`. This section's account — the tier is a wire property, one relay
actor per root connection, owner-only relevancy in place of the replication condition, pure transport,
why the sink is per character — moved to `SimulatableNetSyncComponent-rationale.md` §9, R0-checked on the way. This
component's RPC tail passes the sibling as `noteRttSample`'s sink (§7).

**Nothing tier-related is initialized per character** (the binding's G-08 since task 29, an absence tag above the
skeleton's call of this component's `initializeWithManager` hook; this file's G-03 before). The client-side tier
consumer used to be bound in this component, lazily, once per character. It is the session's now, one per world,
emplaced by the session's `beginPlay` (`beginTierConsumer`, on both roles). *R0 (task 29):* this said "it lives on the
manager now … emplaced where the manager creates its `TimeConfig`"; the consumer is the session's since task 25, and
its emplacement reads the `TimeConfig` of the session's manager.

---

## §6 The relay ring — forwarders, spawn and staging (the link moved to the sibling, task 20b)

Code moved to `SimulatableNetSyncComponent.cpp` (task 20b): the host link (`m_inputRelayHost`), the two
never-replicated fallback rings, the ring accessors, the relayed-ring callback pair, `attachInputRelayHost`,
the relay detach, the staging target and the owner-skip check. Their account — why the ring is on its
own actor, the link and its replay, the two fallbacks, who detaches and why twice is safe, the owner-skip
precondition — moved to `SimulatableNetSyncComponent-rationale.md` §10, R0-checked on the way. What stays here is the
brawler's side: the forwarders, the spawn order at registration, and the stage. Since task 21 the forwarders and the
stage are members of the binding base (§18); the stage's account moved with it (below). Since task 29 the spawn order
and the provider latch are the binding's skeleton's (below).

### The concept surface, and why this file has forwarders

`ASimulationInputRelay` lives in `OGSimulationUnreal`, which must not depend on `OGBrawlerUnreal`.
More importantly, the engine-free core must never name an engine type at all. So the core reaches
the relay through members of *this* object only — the ring accessor pair and the relayed-ring
callback pair — and never learns that the ring is carried by a different actor, or since task 20b
that the link to it is held by the sibling, or since task 21 that the members it calls are inherited from the binding
base. `OGBrawlerSimulatableComponentConceptTest.cpp` passing unchanged
is the proof that the engine-facing change did not leak into the core contract.

⚠ **Correction (R0, task 20b).** This said `stageRelayedInput` was part of that surface, and that each
member "resolves the host and degrades to a never-replicated local fallback". The core's owner concept
(`PredictionSyncedBufferOwnerConcept`) names the callback pair and the `const` accessor; the core also
reads the ring through the non-`const` overload at bind. `stageRelayedInput` is the manager's write
door (the session's `UESimulationSession::relayRemoteInput` since og-ue-simulationcomponentandmanagersplit task 26a), not the core's. Since task 20b the four core-facing
members are one-line forwarders to the sibling, which resolves the host and the fallbacks.

### The spawn ordering, and what it protects — moved to the binding (task 29)

Code moved to `UESimulatableOwnerBinding.h` (task 29): the relay host's spawn (authority) or find (client) and the
delivery-route registration below it, with their ordering guard (G-06 here, G-10 there). Their account — why the host is
spawned above the route, and why it is not spawned on demand from the write path — moved to
`UESimulatableOwnerBinding-rationale.md` §9.5, game-neutral.

⚠ **The fallback container's name was wrong in the baseline** (§11, F-34-6). The baseline said those
writes would land in `m_detachedRelayRing`. Since the write path became a *staged* write they land in
`m_detachedRelayStagingRing`, a different member. Both exist, on the sibling since task 20b; only the
staging one is on that path.

### Staging, not writing — moved to the binding (task 21)

Code moved to `UESimulatableOwnerBinding.h` (task 21): `stageRelayedInput`, its overflow count (G-13 here, G-01 there)
and its no-depth `static_assert`. Their account moved to `UESimulatableOwnerBinding-rationale.md` §6, R0-checked on the
way. The brawler's caller was unchanged by that move: the manager's `relayRemoteInput` called `stageRelayedInput` on this
component, now an inherited member. Since task 26a the caller is the session's `UESimulationSession::relayRemoteInput`.

**History kept here (brawler-specific).** Before staging, the sink wrote the replicated ring directly at a
session-configurable retention depth, so a second arrival in one server frame overwrote the first in memory before
replication ever compared the property — about 11.6 % of relayed inputs, measured at the time. Since task 25 the
signature `static_assert` was seen to fire (`C2338`) when a defaulted `uint32 depth` parameter was added to both the
declaration and the definition.

### The provider latch — moved to the binding (task 29)

The registration writes its provider decision to the sibling through `setHasLocalInputProvider` (reached through the
binding's `netSync()` since task 21), at the statement where it used to write its own member, and the sibling's
owner-skip check reads it (sibling G-15). Since task 29 that statement is the binding's skeleton's, and its account is
`UESimulatableOwnerBinding-rationale.md` §9.5.

---

## §7 The RPC boundary — moved to the binding (task 21)

Code moved to `UESimulatableOwnerBinding.h` (task 21): `receiveRemoteMoveBundle`, the typed tail of the sibling's
`ServerReceiveRemoteMove`, with its guards G-19 … G-23 (G-02 … G-06 there). Its account — primitives in, policy out;
the root connection resolved once; the adapter-side no-wire fallback; one RTT sample per bundle; the slot; the delay
applied once; the two sinks — moved to `UESimulatableOwnerBinding-rationale.md` §3, R0-checked and made game-neutral on
the way.

**The delivery route moved to the binding too (task 29).** The id → component route the session resolves delivered
and relayed input against is registered once, at registration time, on the authority only, by the binding's skeleton
(`noteDelayedInputComponent`, its G-11, this file's G-07 before); its account is
`UESimulatableOwnerBinding-rationale.md` §9.6. What stays here is the brawler's use of the same map: the ring-out score
push walks it as its list of characters (`BrawlerRosterUImpl-rationale.md` §3, and §19 here). In the RPC tail the
session owner is `AOGBrawlerSimulationManager`; since task 26a the tail passes the owner's session as both the delivery and
the relay sink. *R0 (task 29):* this said the owner "passes itself as both … sink", stale since task 26a.

---

## §8 The wire-format compat fences — moved to the sibling (task 20a)

Code moved to `SimulatableNetSyncComponent.cpp` (task 20a), and this section's text moved to
`SimulatableNetSyncComponent-rationale.md` §4, R0-checked on the way: the state channel's version is
**6** (`CorrectionStateBufferCodec.h`), not the 5 this section said. Both fences, the latch and the
toast run on the sibling, before it calls this component's `onCorrectionStateAccepted` or
`receiveRemoteMoveBundle`, so neither typed end ever sees a payload whose version failed.

---

## §9 Retired channels — moved to the sibling (task 20a)

The absence fences of the retired correction-input channel, and of the ring's and the tier's old
registrations on this component, moved with the code they sit in (`SimulatableNetSyncComponent.h` and
`.cpp`; guards G-01, G-02, G-04, G-05 and G-08 … G-10 there). The reasoning is
`SimulatableNetSyncComponent-rationale.md` §5. The ring's and the tier's own accounts stay here, in §6
and §5.

---

## §10 The render-side input echo, and the visualization sites

`TickComponent` draws only. It resolves the manager for its role, skips if the character is not in
storage, and runs six visualization passes — radial, aim, projectile, movement (cvar-gated), target and
block-prediction — then the input-history poll. (This line said "five" before task 25's re-count.)

**The fallbacks are og-simulation axes (og-sim-metres-yup task 8).** `buildAimDirection()` returns the aim in
og-simulation axes, which is what the visualizations take. With no input collection the aim falls back to
`simFrame::kForward`, and so does `tmpAimInput` before the input component is bound; the move direction falls back to
`simFrame::kRight`. They are the og-simulation spellings of the UE +X and +Y literals they replaced, so at the shipped
knobs they are the same vectors. (The `moveDirectionWorld` local has no reader in `TickComponent`.)

**One input source, resolved once per render frame.** `vizPlayerInput` is computed once and shared by
every input-carrying visualization site, so the aim indicator and the block-prediction wedge can
never render two different samples of the same frame.

**`hasInputComponent()` is the local-versus-remote discriminator.** It is true for a pure client's
autonomous proxy **and** for the listen-server host's own pawn, and false for every simulated proxy —
because the bindings it reports are installed by `setupBindings`, reached only from
`AOGBrawlerUECharacter::SetupPlayerInputComponent`, which the engine calls only for a
locally-controlled pawn. The registration-
time `ROLE_AutonomousProxy` test (§4) is deliberately **not** reused here: that one excludes the
host, and the host is exactly the character this echo was added to cover.

- **Local character:** sample live, at this render frame's rate.
- **Remote simulated proxy:** the relay store's **last-known** relayed input — the same character's
  real input as relayed by the authority, and the source the simulation's own proxy prediction
  resolves from. It is deliberately *not* the per-tick scheduled read: the visualization wants "what
  is this player doing", not "which input does tick N run on". This used to read the correction
  cache's input column through `editReconciliation().getLatestInput(...)`, roughly one round-trip
  behind; the relayed value is fresher by about the return leg, and both that column and
  `getLatestInput` itself have since been retired outright.

⚠ **The listen-server fallback holds, but the stated reason for it was wrong** (§11, F-34-1). The
accessor does answer nothing on the authority, so the host's pawn takes the live-sample path and
nothing depends on the remote read. But it is **not** because the authority allocates no relay store:
the authority registration facade calls `SimulationNetSync::registerPredictionOwner` with a null
provider, which takes the provider-absent branch and **does** allocate one for every character. The
store is simply never written there — the ring notification is client-only and the attach-time replay
sees a never-written ring — so `findLatest()` is invalid and the accessor answers nothing because the
store is **cold**, not because it is absent.

**The echoed value carries continuous fields only.** Every discrete field is pinned neutral by
`simulatableBrawler::makeVisualizationPlayerInput`, so an attack or a Hadouken edge structurally cannot render-echo. It is
cosmetic: never fed to the simulation, the RPC, or a cache.

**No tier consult.** Muting the echo on a degraded connection tier is a separate, optional change and
is not wired here.

**The block-prediction pass has no local-player gate on *whether* it draws** — it draws for every
character. Only the input source splits, and that decision was already made in `vizPlayerInput`. A
remote proxy with nothing relayed yet is skipped for that frame, which is the pre-existing behaviour
the nullopt contract preserves.

**The projectile pass** takes its current tick and delta from the **same** clock the simulation uses —
the prediction step on a client, the server simulation step on the authority — so the residual-
lifetime arithmetic lines up with the simulation's own pruning.


### The movement draw and the input-history poll

*Moved from `TickComponent` (task 25).*

**The movement draw reads a sim-tick snapshot on the render clock.** `attackSimAllState` is
`getVizState()` — the whole-`AllState` copy `updateVisualizationAll` takes in the session's
`UESimulationSession::onPostPhysicsStep` (the manager actor's `OnPostPhysicsStep` until
og-ue-simulationcomponentandmanagersplit task 27a), once per game frame at the end of physics, in both
physics backends. So every drawn `State` / `DerivedState` value is the newest tick the copy saw: a
frame that ran several ticks shows only the last, a client that steps on a worker can lag the frame,
and a render frame above 60 Hz redraws the same tick's values; `StaticData` is authored once per
session. (Until og-simulationscheduler-withjolt task 18 this paragraph said "once per completed sim
tick" and "stale by up to one 60 Hz tick"; neither held.) The State and DerivedState halves come from the **same** copy, so the probe reading
and the body pose describe one tick and can be combined — which the servo-error arithmetic in
`BrawlerMovementVisualization.h` rests on. The call is a pure reader: every argument is a `const&`.

**In the Jolt configuration the copy's attached bodies carry the rendered pose** (og-simulationscheduler-withjolt
task 19). Right after the copy, the presenter's `updateVisualization_GameThread` (og-simulation's `TickDriverPresenter`,
called through the manager's Jolt backend) writes the
render interpolation's pose (two steps behind the host time, the pose the capsule is moved to) into the
copy's body states of every declaration except the root: the radial weapon axis, the guard and the
projectile pool. So the weapon, guard and projectile passes draw where the rendered character is, not
one or two steps ahead of it. The root (movement) body is left at the newest tick on purpose: the visible
character is the capsule and its mesh, which the render sync already moved, and this movement draw's
outline is documented as the simulation's belief, built from a body pose and a probe reading of the same
tick. Every other `State` and `DerivedState` value in the copy stays the newest tick's. In the Chaos
configuration the copy is unchanged (`TickDriverBackendUImpl-rationale.md` §7).

**The movement cvar is read per frame, deliberately (guard G-26).** Task 16 made `StaticData` cvars
read once, at construction, so a tunable cannot move under a running session and put two peers on
different numbers. A viz cvar is the opposite case: it feeds nothing simulated, and its value is that a
tuner can type `OGBrawler.Viz.Movement 1` mid-session and see the next frame change. The draw is ~60
debug-line calls per character per frame, so off must cost one bool read. The same reasoning is written
at `brawlerMovementVisualization::visualize` and in `ScoreboardDisplay-rationale.md` §4.

**The input-history poll is the display's only feed.** The rings are keyed per character id, so a
couch sibling gets its own the moment anything polls it. The **lanes** are polled for every registered
character (each component runs the block for itself, exactly once, guard G-30), and the display picks
which to draw at draw time — binding the feed to the selection is what once made a remote proxy's
corrections invisible. The **row** poll keeps its gate (G-29): its source is this client's own capture
line, which a remote proxy does not have. The machine state comes off the same `attackSimState` the
block-prediction pass reads; there is no per-tick machine-state history, so a missed tick stays a hole
and is never back-filled. Each lanes ledger is bounded at `kTickLaneCapacity` (240 ticks). The poll is a
render-rate game-thread read of a physics-written capture line — the accepted tear argued in
`OGBrawlerSimulationManager-rationale.md`'s CROSSING table. Nothing decides on it.

**The aim and block-prediction passes** both consume the shared `vizPlayerInput`; a remote proxy with
nothing relayed yet is skipped for that frame. The block-prediction pass shares the target pass's
query-volume list (`m_targetVisualizationVolumeIds`). The legacy per-enemy range arcs
(`DAttackTargetVisualization.legacyEnemyRangeArcsEnabled`) are off by default; the block-prediction
pass is the primary view and the arcs remain for A/B comparison.

**`selectVisualizationInput`'s echo carries continuous fields only** (§10); **no tier consult** is wired
here — muting on a degraded tier is a separate, optional change.

### The visualization adapters come from the manager, through three backend-neutral members

The target and block-prediction passes query the world through `UESimulationSession::editVizQuery`
and read bodies through `UESimulationSession::getVizReader`; loop one registers the target
volume with `UESimulationSession::registerVizVolume`, handing it the descriptor and the owner
(og-simulationscheduler-withjolt task 51, on the manager; the manager's session's since
og-ue-simulationcomponentandmanagersplit task 24b, reached as `session()`). The other composition reads the
visualization makes (`editStorage`, `getLastRelayedInput`, `runsPrediction` and the two clocks) go through `session()`
too. The manager owns the choice of physics backend
(`OGBrawlerSimulationManager-rationale.md` §18), and these members exist in both configurations, so this
component contains no backend arm and names no adapter type: it includes `PhysicsBackendUImpl.h`
instead of the two Chaos adapter headers. In the Chaos configuration the members return the Chaos
query adapter and the game-thread reader, and `registerVizVolume` builds the query parameters this
file used to build itself (simple collision, the owner ignored), so the volume and the two passes
behave as before. `registerVizVolume` returns an optional volume id, which is empty only in the Jolt
configuration while the slot acquire waits (§3). The visualizers are templates over the adapter types, so the calls compile against
whichever pair the manager selects.

In the Jolt configuration the two accessors return the step world's adapters on the authority and on
a client stepping inline (`og.Sim.RunInline`), and the game-thread shadow world's on a client that
steps on a worker, where the step's own world may be queried only by the step or under the world
mutex (`TickDriverBackendUImpl-rationale.md` §6). Either
way the component gets a world it may query on the game thread, so both passes run on every peer, as
in the Chaos configuration. (og-simulationscheduler-withjolt task 53 returned null on a worker client
and the component skipped the two passes there; task 56 removed that skip.)

---

## §11 Corrections — claims this file carried that were not true

*Names here are as they stood then: task 30 renamed the manager actor `ASimulationManagerUImpl` to `AOGBrawlerSimulationManager`, its files to `OGBrawlerSimulationManager.h` and `OGBrawlerSimulationManager.cpp`, and its docs pair to `OGBrawlerSimulationManager-rationale.md` and `OGBrawlerSimulationManager-guards.md` (ids unchanged).*

*History. Code moved to `SimulatableNetSyncComponent.cpp` (task 20a): the rows below that name the
OnRep, the RPC's fences or the version byte describe code that now lives in the sibling component.
Code moved to `SimulatableNetSyncComponent.cpp` (task 20b): the rows that name `attachInputRelayHost`,
the detached rings or the host's dereferences (F-34-2, F-34-6, F-25-5, F-25-7) describe code that now
lives in the sibling too.*

Every claim of fact in the baseline was checked against the tree **before** being compressed. Nine
did not survive the check. Compression makes a false statement shorter, denser and more
authoritative, and coverage, subject and lint checks all pass it, because every symbol it names
exists — they verify existence, never truth.

| # | claim as it stood | what the tree says |
|---|---|---|
| **F-34-1** | *"the authority allocates no relay stores at all (`registerPredictionOwner`'s provider-absent branch is the only site that creates one, and it runs on the client)"* | **FALSE.** The authority registration facade calls `registerPredictionOwner` with a **null** provider, so the provider-absent branch runs there too and `SimulationInputResolution::registerRemoteCharacter` allocates a neutral-seeded store for **every** authority-registered character. The behaviour the sentence defends survives — the accessor does answer nothing on the authority — but because the store is never written, not because it is absent. ⚠ The same false sentence sits in two files this task does not own; see the routed findings below. |
| **F-34-2** | *"exactly the test the radial viz above already uses (line ~555)"* | **A STALE LINE JOIN.** The radial visualization's `hasInputComponent()` test is at `:831` in the baseline; `:555` is inside `attachInputRelayHost`, an unrelated function. Re-anchored on the symbol per the never-cite-a-line-number rule. |
| **F-34-3** | *"`SimulationNetSync::collectInputAll`, authority branch"* | **WRONG OWNER.** `collectInputAll` moved to `SimulationInputResolution` when input resolution was extracted; the qualified `SimulationNetSync` form is already declared a dead owner in `CorrectionCache-rationale.md`. Re-owned. |
| **F-34-4** | *"`noteDelayedInputComponent` is what makes `relayRemoteInput` able to find this component and **write the ring**"* | **HALF TRUE.** The route claim is right — `relayRemoteInput` does resolve through that map, verified at the manager. But since the write path became a staged write it **stages**, and the host's `PreReplication` publishes; nothing writes the ring directly from there. |
| **F-34-5** | *"This component is designed to tick after the LiveLink component… We also use a tick prerequisite on LiveLink components"* | **FALSE ON BOTH HALVES.** The prerequisite call is commented out on the next line, and `LiveLink` appears nowhere in this project's source outside those three comment lines — there is no such component to order behind. §2. |
| **F-34-6** | *"the first writes would land in `m_detachedRelayRing`"* | **WRONG MEMBER.** The staged write path falls back to `m_detachedRelayStagingRing`. Both members exist, which is what makes the error invisible to any check that only asks whether a name resolves. §6. |
| **F-34-7** | the block-prediction guard: *"render-rate live sample locally, **server correction ~1 RTT behind** for remote simulated proxies"* | **STALE, AND THE FILE CONTRADICTED ITSELF SEVENTY LINES APART.** The re-source block above it already said the remote source is the relay store's last-known relayed input, *"fresher by roughly the return leg"*. This guard still described the retired correction-cache column. §10. |
| **F-34-9** | *"the registration-time `ROLE_AutonomousProxy` test used in `tryRegisterWithManager`"* | **A SYMBOL THAT DOES NOT EXIST.** The only occurrence of `tryRegisterWithManager` anywhere in the tree was that comment. The two real functions are `tryInitializeWithManager` and `tryRegisterWithNewFramework`, and the test in question is in the second. Re-anchored on §4 rather than on a name a reader would grep for and never find. |
| **F-34-8** | seventeen lines of commented-out code | **UNRESTORABLE.** `m_simulationStateSyncedBuffer`, `EpicGamesAssignment`, `LiveLinkComponent` and the two `StructeredLog` headers (a misspelling) exist **nowhere** in this tree or the engine include paths. Un-commenting any of them is a compile error, so none is code that could be brought back. Deleted; the one fact they carried — that no tick prerequisite is installed — is stated in §2. (Task 25: no longer tagged — re-adding a prerequisite on a component that does not exist is a compile error, so there is no edit left to fence.) |
| **F-25-1** | *"~10 seconds at 60 Hz"* at `kMaxRegistrationAttempts` | **WRONG CLOCK.** One attempt per `SetTimerForNextTick`, i.e. per game frame, not per 60 Hz sim tick. §2. |
| **F-25-2** | this file's §8: *"`kWireFormatVersion` … is **4** today"* | **STALE.** It is **5** (`CorrectionStateBufferCodec.h`), bumped when `hitRootBodyId` left the wire. §8. |
| **F-25-3** | *"SPAWN THE HOST BEFORE THE noteDelayedInputComponent ROUTE -- until that route exists, relayRemoteInput stages into m_detachedRelayStagingRing"* | **WINDOW REVERSED.** Until the route exists `relayRemoteInput` finds no component and returns. The hazard is the route existing while no host is linked. Guard G-06. |
| **F-25-4** | *"NEVER HasAuthority() HERE -- it is always true on a non-replicated actor"* | **WRONG ACTOR.** That describes the manager (`bReplicates = false`); this component's owner is a replicated pawn. The prohibition survives on the other half: the predicate must match the one `ASimulationManagerUImpl::BeginPlay` fills its instance slot with. Guard G-02. |
| **F-25-5** | header, `m_detachedRelayRing`: *"A server-side write that landed here would be invisible to every client, so the spawn is sequenced BEFORE the relay tap's id->component route"* | **WRONG MEMBER, AGAIN.** F-34-6's error, still standing in the header: server writes are staged, and with no host they land in `m_detachedRelayStagingRing`. Nothing writes `m_detachedRelayRing` on the server path; it is the read fallback. §6. |
| **F-25-6** | header, the callbacks: *"Null until Task 7 wires the new path; OnRep_ handlers fall through to old logic when null"* | **FALSE.** There is no old logic. `OnRep_CorrectionState` calls the callback if bound and otherwise does nothing further. |
| **F-25-7** | header, the host's forward declaration: *"the two accessors that dereference it are defined in the .cpp"* | **UNDERCOUNTED.** Six members dereference the host in the `.cpp`: both `getRelayedInputRing` overloads, `stageRelayedInput`, `setOnRelayedInputReceivedCallback`, `attachInputRelayHost`, `EndPlay`. The forward declaration's reason (a widely included header) stands. |
| **F-25-8** | header, the tier sink: *"this component is its own target, so it asserts `id == GetUniqueID()`"* | **FALSE SINCE TASK 25.** The `check` compares `id` with the storage key of the pawn's replicated `SimCharacterId`. §13. |
| **F-25-9** | *"THE DELAY BAR IS A THIRD BAR ON THE METER, so it implies the LANE poll runs even when the other two bars are off"* | **STALE COUNT.** `anyBarEnabled()` counts four selections: provenance, input delay, character state, and relay health (nearest stack only). The conclusion — any bar implies the lane poll — holds. §10. |
| **F-25-10** | *"findOrSpawnForConnection logged the reason"* | **PARTLY.** It logs on its reachable null path (no owning actor on the wire). A null world or connection returns silently; the connection cannot be null here. |
| **F-25-11** | header, the correction-input absence: *"a remote character's input reaches peers on the relay ring below"* | **NOT BELOW.** The ring is a property of `ASimulationInputRelay`. Guard G-36. |
| **F-25-12** | *"the accepted tear argued at ASimulationManagerUImpl's CROSSING block"* | **RE-POINTED.** The manager header was converted; the CROSSING table now lives in `SimulationManagerUImpl-rationale.md`. §10. |
| **F-25-13** | the tier block's citation of a relay-delay-spectrum design document, §12 | **UNRESOLVABLE HERE.** That document is in a private workspace outside this repository (as R-34-d). Dropped; the reasoning it pointed at is in §5. |

### Routed — not this file's to fix

- **R-34-a.** `SimulationInputResolution.h`'s `getLastRelayedInput` banner states *"no store exists
  for a LOCAL character or on the **AUTHORITY** at all"*. The first half is right; the second is
  F-34-1's false claim, in the core, in the file that owns the accessor.
- **R-34-b.** `SimulationInputResolutionTest.cpp`'s listen-server case says *"the authority allocates
  no relay stores at all, **verified rather than assumed**"* — and the assertion under that sentence
  registers through `registerAuthorityOwner` alone, never through the authority `registerSimulatable`
  facade that production uses. The test passes; it verifies a path production does not take.
- **R-34-c.** The authority `registerSimulatable` facade's own guard reasons about *"an id exposed
  early reading a relay store that will never exist for it"*. By F-34-1 the store does exist on that
  path; the branch dispatch it protects is keyed on the remote-move queue, so the fence's conclusion
  is unaffected, but its stated premise is not.
- **R-34-d.** Two comments cite `risks_and_plan.md §5.2`, a document in a private research workspace
  outside this repository. Removed from this file; the same citation survives in five other files
  with no owner. The wire-format fence's reasoning is now in §8, which resolves standalone.

---

## §12 The task-25 conversion — where every pre-conversion comment went

*Names here are as they stood then: task 21a renamed this component `USimmableUpdateComponent` to `UOGBrawlerSimulatableComponent`, its files to `OGBrawlerSimulatableComponent.h` and `OGBrawlerSimulatableComponent.cpp`, and this docs pair to `OGBrawlerSimulatableComponent-rationale.md` and `OGBrawlerSimulatableComponent-guards.md` (ids unchanged).*

*History. Code moved to `SimulatableNetSyncComponent.cpp` (task 20a): the rows naming G-09 … G-11,
G-17, G-18, G-32 … G-34, G-36, G-37, §8 and §9 record where those comments went in task 25. The ids
retired in task 20a (`SimmableUpdateComponent-guards.md` §R names each successor), and §8 and §9 are
pointers now. Code moved to `SimulatableNetSyncComponent.cpp` (task 20b): the rows naming G-08, G-12,
G-14 … G-16, the relay forwarders, the owner-skip precondition and the tier sink, and the "Carried
header prose" paragraphs on the tier sink and the host link, describe this component before task 20b;
those ids retired (§R there), and §5 and §6 point at the sibling's §9 and §10.*

Both files went from the Phase C convention (one-line `⛔ … §N` fences, long header banners) to
code plus tags. Nothing was discarded without a destination. **Guards** are
`SimmableUpdateComponent-guards.md` G-01 … G-37 (G-01 was written with task 25's code; the rest quote
the shipped fence they replaced). **One fence became a `static_assert`**: the header's "NO DEPTH
PARAMETER" (§6). Everything else is here.

| pre-conversion comment | where it went |
|---|---|
| `.cpp` orientation banner | §0 (phase order updated for §13) |
| movement-cvar block; `TickComponent` movement, poll, echo, aim, projectile and block-prediction prose | §10 and its movement/poll subsection; the prohibitions in them are G-24 … G-31 |
| `kMaxRegistrationAttempts` "~10 seconds at 60 Hz" | §2, corrected (F-25-1) |
| tick group / LiveLink one-liner | §2 (already there, F-34-5) |
| `BeginPlay` / "Kick off body creation" / loop comments | §3 |
| `⛔` one-liners in `tryInitializeWithManager`, `tryRegisterWithNewFramework`, `EndPlay` | G-02 … G-08, G-15; the CLIENT pull-half note is §6 "Linking" |
| `OnRep_CorrectionState` fences; the version-byte note | G-09; §8 |
| retired `OnRep_*` one-liners; `GetLifetimeReplicatedProps` absence fences | G-10, G-11, G-32 … G-34; §9 |
| relay forwarders (section banner, re-read at bind, idempotent link, weak capture, replay) | §6; G-12 … G-14 |
| owner-skip precondition, `num() > 0`, `ensure` | §6 "The owner-skip precondition"; G-15, G-16 |
| RPC-boundary one-liners; the `ConnectionTierSink` `static_assert` note | G-17 … G-23; §7 |
| tier sink one-liners (pure transport, relay actor, defensive null) | §5; F-25-10 |
| `OGSIM_OPTIMIZE_ON` fence | G-35; §2 |
| trailing labels (`GEngine` include, legacy arcs default, `RemoveDependentActor, then Destroy`, idempotent, "logged the reason"), the `//Component` banner, the relay-carrier include note | deleted as labels; their facts are in §6, §10 and G-08 |
| header: concept typedefs, the callbacks, forwarders, attach/arrival declarations | §6, §7 and "Carried header prose" below |
| header: tier block (Option A, why the sink stayed on the component) | "Carried header prose" below; §5 |
| header: `stageRelayedInput` banner | `static_assert` + §6 |
| header: retired members (correction-input property and callbacks, `getSyncedCorrectionInputBuffer`, the ring property, the tier property and consumer, `getMachineVizState`) | G-36, G-37; §9 and below |
| header: wire-format latch, toast key | §8 (latch wording corrected) |
| header: `m_inputRelayHost`, detached rings, `m_hasLocalInputProvider` | §6; G-12; F-25-5 |
| header: `UObject` constructor doc, `//Physics`, `//Visualization`, "To add mapping context" | deleted as labels |
| header: two commented-out physics-state overrides | deleted; recorded in §2 |
| header: the two attack-axis body setters, the attack-axis body pointer and async-physics handle (no caller in either physics configuration), the Chaos tick-mapper forward declaration, and the `.cpp`'s Chaos includes | deleted by og-simulationscheduler-withjolt task 19 (design D §3.4); the component names no physics backend (§10, "The visualization adapters come from the manager") |

### Carried header prose (verified before the move)

**The concept typedefs.** `SyncedCorrectionBufferType` is the correction-state role
(`FSimulationStateSyncBuffer`). `RelayedInputRingType` is named through a typedef for the same
reason: `SimulationNetSync` binds the arrival callback and reads the ring at registration, and must do
so without naming a UE type. A third typedef, for the client→server input buffer, was retired with
that buffer in og-syncedInput-rework task 9 (2026-10-03); see §9. `SimulatableOwnerTraits<SimulatableBrawler>` is specialised in
`SimulatableBrawlerOwnerTraits.h`; any site that instantiates `SimulationNetSync<SimulatableBrawler>`
includes that header.

**`sendLocalInputToAuthority`** builds an `FInputRedundancyBundle` from the most recent
`redundancyDepth` ticks still in the core's `PendingInputQueue` (the builder clamps the depth to the
bundle's slot capacity) and fires the unreliable `ServerReceiveRemoteMove`. It is defined in the `.cpp`
so the bundle-builder include stays out of this widely included header. The RPC is **unreliable**
since Stage 1: no head-of-line blocking on input RPCs, and a dropped datagram self-heals because each
send repeats the last `redundancyDepthTicks` ticks.

**`deliverDelayedRemoteInput`** is the delivery entry point for an input released from the server's
`ServerInputDelayQueue`, reached on the game thread from `UESimulationSession::deliverRemoteInput` (the manager's until task 26a)
before the authority physics step. It routes through the **same** callback the RPC's no-wire fallback
uses, with the **original** capture tick, so from `RemoteMoveQueue` onward a delayed input is
indistinguishable from an undelayed one and the capture-tick dedup keeps working. The delay is
expressed only as *when* this is called (G-23).

**The tier (Option A, server-authoritative).** The server derives the tier from its own per-connection
round-trip time (`ServerReceiveRemoteMove` resolves the primitive and forwards it to
`ServerReceptionCoordinator::noteRttSample`) and publishes it; the client only reads it. With one
producer there is no second estimator to disagree with, so the boundary-RTT tier split and its
recurring one-tick corrections cannot happen. The replicated property, its notification and the client
consumer moved off this per-character component onto the per-connection `ASimulationConnectionRelay`:
one actor per wire makes "one wire, one tier" structural. **Why the sink stayed here**: it must
resolve id → owning wire, and this object holds that link unconditionally. The manager's
id → component map is populated at registration, several frames after the first input RPC can
arrive, and the core records a tier as published **before** calling the sink
(`ServerReceptionCoordinator` updates its last-published table, then calls
`sendConnectionTierToOwningClient`), so a publish the sink could not route would never be retried. The
`ConnectionTierSink` `static_assert` pins that this component is the sink.

**The host link.** `m_inputRelayHost` is weak because the host is destroyed with the character and the
teardown order between the two is not guaranteed. It is set on the authority at spawn — registration
completion, strictly before the first relay write (G-06) — and on a client by whichever link path gets
there first. `m_detachedRelayRing` is the **read** fallback: the core reads the ring once at
registration bind on both roles and the concept types it as a reference, so an unlinked component
returns this never-replicated, always-empty ring (version 0, so the bind-time ingest no-ops).

**Retired, and not fenced by a tag:** `set/clearOnCorrectionInputReceivedCallback` and
`m_onCorrectionInputReceivedCallback` (retired with the correction-input channel);
`getSyncedCorrectionInputBuffer` (the authority's outbound surface is now the correction-state buffer,
carrying the applied-capture-tick ref, plus the relay ring); the tier property, its notification, the
pre-notification latch and the `ReplicatedTierConsumer` (now the manager's); `getMachineVizState()`
(its only caller was the pawn's flinch-freeze predicate on the retired movement-component path; the
freeze now happens inside `brawlerMovementSimulation::integrate`). The fenced retirements are G-10,
G-11, G-32 … G-37.

---

## §13 The simulation character id (task 25)

Every id this component hands the core — `tryRegister`, `noteDelayedInputComponent`,
`unregisterFromNewFramework`, the provider lambda, every `id=%u` log line, the storage lookup in
`TickComponent`, the relayed-input read, the input-history polls — is the pawn's replicated
`SimCharacterId`, read through the private helper `simCharacterId()` (which answers
`SimCharacterId::None` if the owner is not an `AOGBrawlerUECharacter`). It is **not** this component's
`GetUniqueID()`, which is a different number on every peer. *R0 (task 29):* the registration's own reads never went
through the helper: the loop read `character->GetSimCharacterId()` on the cast pawn, and since task 29 the hook
`resolveRegistrationKey` does, answering `toStorageKey` of it; the same pawn, the same value.

**How the id arrives.** In the registration hook `resolveRegistrationKey`, which the binding's skeleton calls once the
manager exists (in this component's own `tryRegisterWithNewFramework` until task 29); the hook answers the storage key,
`toStorageKey` of the pawn's id, and the skeleton hands that key to the session's `tryRegister` and
`noteDelayedInputComponent`, the provider and the `NewFramework: registered` line:

* **Authority:** if the pawn's id is still `None`, allocate one from
  `BrawlerRosterUImpl::allocateSimCharacterId`, through the manager's `roster()` (count-up 1 … 255, never reused), and set it with
  `AOGBrawlerUECharacter::SetAuthoritativeSimCharacterId`. A refusal (all ids spent) is final —
  guard G-01: the hook answers `abandon()`; the roster logs it at Error and `OG_CHECK`s.
* **Client:** while the replicated id is still `None`, the hook answers `retry()`, and the skeleton stays pending on the
  same retry loop and the same `m_registrationAttempts` budget (the binding's since task 29), ending in a `checkf` if it
  never arrives.

The id is replicated with the pawn, not on the correction wire, so no wire format changes. The
sibling's `sendConnectionTierToOwningClient` `check` (task 20b) compares its `id` argument with the
binding's `simulationKey`, which is the same storage key.

---

## §14 Latency-budget stamps — moved to the binding (task 21)

Code moved to `UESimulatableOwnerBinding.h` (task 21): the five stamp blocks (`sendLocalInputToAuthority`,
`receiveRemoteMoveBundle`, `onCorrectionStateAccepted`, `noteRelayedRingArrived` and the flush callback
`makeRingFlushedCallback` returns), verbatim and in place, each through `UELatencyBudgetDiagnostics::withInstance`.
Their account moved to `UESimulatableOwnerBinding-rationale.md` §4. The brawler's hop table, threads and every site's
measurement error stay `OGBrawlerSimulationManager-rationale.md` §16.

**What stays here (the brawler's side).**

**The provider lambda is unchanged, and G-05 still holds.** The capture stamp needs the manager, which
that lambda may not capture, so it is a wrapper the manager applies in `tryRegister`
(`OGBrawlerSimulationManager-rationale.md` §16; since og-ue-simulationcomponentandmanagersplit task 28 the session's
`tryRegister` applies it, `UESimulationSession-rationale.md` §12.1). The only addition to this component's header for the stamps was
`getOwnerInputCollection`, a `const` read the wrapper uses to reach the input-sample time (through
`BrawlerSessionGameUImpl::inputSampleSource`). og-tickdriver task 18 added a second, a `const` overload of
`getSyncedCorrectionStateBuffer`, so that the latency diagnostics read the correction buffer's tick through a const
path only (`UELatencyBudgetDiagnostics-rationale.md` §4); production callers still get the mutable overload. Since task
20a both overloads forward to the sibling's, which owns the buffer, and since task 21 they are the binding base's.

## §15 The two per-packet log lines — moved to the binding (task 21)

Code moved to `UESimulatableOwnerBinding.h` (task 21): `[ReceiveCorrectionState]` in `onCorrectionStateAccepted` and
`[ServerReceive]` on the no-wire path of `receiveRemoteMoveBundle`, both `OG_LOG_ROUTED` at their row's `plainLevel`
(`Verbose`). Their account moved to `UESimulatableOwnerBinding-rationale.md` §5. The brawler's measurement that set the
level stays here: `[ReceiveCorrectionState]` ran at 57–59 lines per second per client in the gate-22 Jolt packaged final
run (og-simulationscheduler-withjolt task 80; the rule and the rates are `UESimulationLogRoutes-rationale.md` §6).

---

## §16 The NetSync sibling and the binding (og-ue-simulationcomponentandmanagersplit task 20a)

**The ruled deviation.** Task 20a moved this component's reflected wire surface — the replicated correction-state
buffer, `OnRep_CorrectionState`, `ServerReceiveRemoteMove` with its two fences, the mismatch latch and the toast key, and
`GetLifetimeReplicatedProps` — verbatim into `USimulatableNetSyncComponent` (OGSimulationUnreal), created on the
character as the default subobject `SimulatableNetSync` (`OGBrawlerUECharacter-rationale.md` §15). This component
stopped replicating. Rulings Q8, Q11, Q12 and Q-D: the replicated object changed, so builds from before the task do not
interoperate with builds after it (`SimulatableNetSyncComponent-rationale.md` §6). Their narrative went with them: this
file's §1 table, §8 and §9 are that file's §1, §4 and §5.

**The binding.** This component derives from `ISimulatableOwnerBinding` (`UESimulatableOwnerBinding.h`), a plain
secondary base after `UActorComponent`, and overrides its members itself, privately (three in task 20a, the other two
in task 20b, §17):

| member | body | the sibling calls it |
|---|---|---|
| `simulationKey` | `toStorageKey(simCharacterId())` | in the tier send's `check` and log and the owner-skip lines (since task 20b) |
| `onCorrectionStateAccepted` | the typed peek, `[ReceiveCorrectionState]` and the `ClientReceived` stamp, verbatim from the old OnRep | in `OnRep_CorrectionState`, after the wire-format fence, before the core's callback |
| `receiveRemoteMoveBundle` | the old RPC body below its fences, verbatim from G-19 on | in `ServerReceiveRemoteMove_Implementation`, after both fences |

⚠ `simulationKey` was the interface member's own name until task 21, which moved the override into the binding
template and gave the brawler's hook the distinct name `storageKeyOfOwner` (review B-2, ruling Q-F; §18).

**The sibling pointer.** `bindNetSync` sets `m_netSync` and attaches this component to the sibling; `unbindNetSync`
detaches it and clears the pointer, and is a no-op on a component that was never bound. The character calls
`bindNetSync` in its `PostInitializeComponents` (its guard G-21), because an Iris client delivers the first
`OnRep_CorrectionState` of an arriving character before any `BeginPlay` (`SimulatableNetSyncComponent-rationale.md`
§3). Seen failing in task 20a: with the bind moved into this component's `BeginPlay`, the sibling's unbound `checkf`
fired on a client receiving an existing character, at first join and on rejoin.

**The forwarders.** The owner-concept members whose state moved are one-liners onto the sibling: the correction-state
callback pair and both `getSyncedCorrectionStateBuffer` overloads (the const one is the latency diagnostics' read).
`sendLocalInputToAuthority` builds the bundle as before and hands it to the sibling's `sendInputBundleToAuthority`,
which calls the RPC. The aliases `SyncedCorrectionBufferType` and `RelayedInputRingType` are unchanged, the core's owner
type is still this component (`SimulatableOwnerTraits<SimulatableBrawler>` is unchanged), and
`OGBrawlerSimulatableComponentConceptTest.cpp` compiles unchanged. Task 21 replaced the overrides and the forwarders with the
CRTP binding template (§18).

**Teardown.** `EndPlay` runs the manager's unregister, then (since task 20b) the sibling's relay-host detach, then
`unbindNetSync`, then `Super` (§17). The unregister reaches the core's `clearOwnerCallbacks`, which clears the
correction callback through its forwarder, so the sibling must still be bound then; the unbind comes after it for that
reason.

**Equivalence.** Every log line keeps its text, level, category, order and thread: the Errors and the toast moved
verbatim (ruling Q6), and `[ReceiveCorrectionState]`, `[ServerReceive]` and the latency stamps stay in this file at the
same statement positions (§14, §15). The only new behaviour is the sibling's two unbound `checkf`s, which a correctly
built character never reaches.

---

## §17 The relay-host link, the rings and the tier send move to the sibling (og-ue-simulationcomponentandmanagersplit task 20b)

**What moved, verbatim and in order** (to `SimulatableNetSyncComponent.h` and `.cpp`; its rationale §9–§11): the
weak host pointer `m_inputRelayHost`, the two `UPROPERTY(Transient)` fallback rings `m_detachedRelayRing` and
`m_detachedRelayStagingRing`, the provider flag `m_hasLocalInputProvider` and the latch `m_loggedOwnerSkipDivergence`,
both `getRelayedInputRing` overloads, the relayed-ring callback pair and its member, `attachInputRelayHost`,
`onRelayedInputRingArrived` with its owner-skip check, the staging target choice (`relayStagingTarget`), the relay
detach (`detachInputRelayHost`), and `sendConnectionTierToOwningClient` with its `ConnectionTierSink` `static_assert`.
The rings are `Transient` and were never replicated, so nothing on the wire changed; this component still declares no
replicated property.

**The binding's last two members**, overridden here with today's bodies:

| member | body | the sibling calls it |
|---|---|---|
| `noteRelayedRingArrived` | the client `ClientReceived` stamp block, verbatim from the old arrival handler | in `onRelayedInputRingArrived`, after the owner-skip check, before the core's callback |
| `makeRingFlushedCallback` | returns the old flush lambda verbatim, with its own weak capture of this component | in `attachInputRelayHost`, on the authority, as the argument of `setOnRingFlushedCallback` |

**What stays, and how it reaches the sibling.**
- The four core-facing relay members are one-line forwarders: both ring accessors, and the relayed-ring callback
  pair. `attachInputRelayHost` is a forwarder too, because the manager's listener and this component's registration
  call it on this component. The `const` accessor forwards through a `const` sibling, so it reaches the sibling's
  `const` overload.
- The registration writes the provider decision with `m_netSync->setHasLocalInputProvider(isLocallyPredicted)` at the
  statement where it wrote its own member (§6, "The provider latch").
- `stageRelayedInput` stayed here until task 21, which moved it into the binding with G-13 and the no-depth
  `static_assert` (§18). It reads the host
  with `getInputRelayHost` and the target with `relayStagingTarget`, two reads of the same weak pointer on the game
  thread, which agree.
- The RPC tail passes `*m_netSync` to `noteRttSample` as the tier sink; the coordinator calls it at once and does not
  keep it.

**The teardown, statement by statement** (`EndPlay`):
1. `isAuthority`, `instanceFor`, and `manager->unregisterFromNewFramework(simCharacterId(), *this, isAuthority)` when
   there is a manager — the unregister, while the sibling is still bound;
2. `m_netSync->detachInputRelayHost(isAuthority)` when bound (G-38) — the authority destroys the host, a client unbinds
   it, then the link is cleared (since task 21 spelled `netSync().detachInputRelayHost(isAuthority)` under
   `isNetSyncBound()`, §18);
3. `unbindNetSync()` — the sibling's `detachBinding`, then `m_netSync = nullptr`;
4. `UActorComponent::EndPlay(EndPlayReason)`.

The sibling's own `EndPlay` detaches only when no binding is attached (its G-11), so the order of the two `EndPlay`
calls, which is not a contract, cannot move the detach ahead of the unregister. When the sibling ends after this
component, its call finds the host already cleared and does nothing, since the detach is idempotent.

**Equivalence.** `[InputRelay]` (the owner-skip Error and its `ensure`) and `[ConnectionTier]` moved verbatim, with
`m_binding->simulationKey()` in place of `toStorageKey(simCharacterId())`, which is what this component's
`simulationKey` returns. Every latency stamp stays in this file at the same position relative to the sibling's code
(§14).

---

## §18 The typed half becomes the CRTP binding (og-ue-simulationcomponentandmanagersplit task 21)

**The base.** `ISimulatableOwnerBinding` became `UESimulatableOwnerBinding<UOGBrawlerSimulatableComponent,
BrawlerSessionTraitsUImpl>`, after `UActorComponent` (`UESimulatableOwnerBinding-rationale.md` §2). The template derives
from the interface, so the sibling's five calls are unchanged. Step 0 built the base change alone, with the template
empty, in all four configurations before anything moved.

**What moved into the template, verbatim and in order:** the five overrides, now private and `final`; the
correction-state and relayed-ring callback pairs, both buffer getters, both ring getters and `attachInputRelayHost` (the
forwarders); `sendLocalInputToAuthority`, the remote-move callback pair with its member, `deliverDelayedRemoteInput` and
`stageRelayedInput` (the typed owner-concept members); `m_netSync` with `bindNetSync` and `unbindNetSync`; the aliases
`SyncedCorrectionBufferType` and `RelayedInputRingType`; the five stamp blocks. Brawler types became `TraitsT` names
(`PlayerInput`, `State`, `Simulatable`, `SessionOwner`), `toStorageKey(simCharacterId())` became the hook, and this
component's own engine calls (`GetOwner()`, the weak self) go through the binding's `derived()`.

**What this component keeps.**
- `storageKeyOfOwner()` (private), today's `toStorageKey(simCharacterId())` and the old `simulationKey()` body. The
  binding's `final` `simulationKey()` returns it and checks for it with an inline requires-expression; the binding is a
  friend (`friend class UESimulatableOwnerBinding<UOGBrawlerSimulatableComponent, BrawlerSessionTraitsUImpl>;`).
- The registration writes the provider latch through `netSync().setHasLocalInputProvider(isLocallyPredicted)` (since
  task 29 the binding's skeleton writes it, §19), and `EndPlay` keeps its order (G-38): unregister, then `if (isNetSyncBound()) netSync().detachInputRelayHost(isAuthority);`,
  then `unbindNetSync()`, then `Super`.
- The header declares the specialization `extern template class …;` above the class, and the `.cpp` defines it once
  (`template class …;`, after `storageKeyOfOwner`), so the template's virtual members compile only there
  (`UESimulatableOwnerBinding-rationale.md` §7).

**The core's owner type is still this component.** `SimulatableOwnerTraits<SimulatableBrawler>` is unchanged,
`OGBrawlerSimulatableComponentConceptTest.cpp` compiles unchanged (the aliases and members are found through the base), and
the manager calls `attachInputRelayHost`, `deliverDelayedRemoteInput` and `stageRelayedInput` on this component as
before.

**Equivalence.** Every log line keeps its text, level, category, order and thread; the stamp blocks sit at the same
statement positions. One check is new and fatal-only: `netSync()`'s `checkf`, at the provider latch, where the old code
dereferenced the pointer unchecked. Lead ruling F1: the moved bodies compile optimized in Development (the template is
instantiated in this `.cpp`, which ends with `OGSIM_OPTIMIZE_ON`); the hook `storageKeyOfOwner` stays unoptimized with
the rest of this file.

---

## §19 The registration loop becomes the binding's skeleton (og-ue-simulationcomponentandmanagersplit task 29)

**What moved** (ruling Q-M6), statement for statement, into `UESimulatableOwnerBinding<UOGBrawlerSimulatableComponent,
BrawlerSessionTraitsUImpl>` (`UESimulatableOwnerBinding-rationale.md` §9): `BeginPlay`'s next-tick call (the binding's
`beginRegistration()`, which `BeginPlay` calls after `UActorComponent::BeginPlay()`), `tryInitializeWithManager`,
`scheduleNextInitializationAttempt`, `scheduleNextRegistrationAttempt`, `tryRegisterWithNewFramework`, the two attempt
counters and `kMaxRegistrationAttempts`; with them the guards G-02, G-03, G-04, G-06 and G-07 (G-07 … G-11 there) and
this file's §3/§4 narrative, §2's attempt budget and §6/§7's spawn-order, provider-latch and delivery-route accounts.

**The four hooks this component keeps** (private; the binding, a friend, calls them and checks them with an inline
requires-expression in each loop):

| hook | the statements it holds | answers |
|---|---|---|
| `initializeWithManager(manager, owner)` | the input-collection cache (`Cast<AOGBrawlerUECharacter>`), the target query volume through `manager.session().registerVizVolume`, the three visualization states | `Retry` while the volume waits for the Jolt slot (§3), else `Ready` |
| `resolveRegistrationKey(manager, isAuthority)` | the character cast and its `checkf`, the authority's allocation through `manager.roster()` with G-01's refusal, the replicated id's read | `abandon()` on a refused allocation (G-01), `retry()` while the id is `None`, else `ready(toStorageKey(simId))` |
| `makeInputProvider(key)` | the input-collection read, the id and the G-05 lambda | the lambda |
| `makeSimulatable()` | `SimulatableBrawler(*m_staticData)` | the simulatable (constructed in the skeleton's local: guaranteed elision) |

The owner actor is passed to `initializeWithManager` by the skeleton (which reads it and returns while it is null), so
the hook reads no second `GetOwner()`. The hooks' names are their own: none is named like an `ISimulatableOwnerBinding`
member, whose overrides are `final` (`UESimulatableOwnerBinding-rationale.md` §9.1).

**The brawler's part of the delivery route.** The skeleton registers the route on the authority (binding G-11); the
ring-out score push (`BrawlerRosterUImpl::pushRingoutScores`, called by the `afterVisualization` hook with the session's
`getDelayedInputComponentsById`) walks the same map as its list of characters whose scores leave the server, so a
narrower registration silently drops their scores (`BrawlerRosterUImpl-rationale.md` §3; this file's G-07's second
half, kept in its §R entry).

**Which manager door.** The skeleton calls the session's `tryRegister` and `noteDelayedInputComponent` with the storage
key; the manager's two `SimCharacterId` wrappers (task 28) are no longer called by this component (they stayed on the
manager until og-ue-simulationcomponentandmanagersplit task 36 removed them; its `unregisterFromNewFramework` wrapper is
still this component's `EndPlay` call). The `tryRegister` wrapper's `checkf` that the id is not `None` guarded a
condition the hook's `retry()` excludes one step earlier.

**Equivalence.** `transcribe.py` (`map_29.json`) inlines each hook at its call site and maps each answer to the
skeleton's handler, against the pre-task-25 pin: no statement difference. No log line changed. Three `checkf` texts are
game-neutral now (ruling Q7): the manager wait in loop one, loop one's retry and loop two's id wait (this component's
class name and "the pawn's SimCharacterId" are gone from them). The skeleton is a template, compiled optimized in
Development (lead ruling F1); none of its statements is floating-point. The hook bodies stay in this file's
`OGSIM_OPTIMIZE_OFF` region; the one floating-point expression of the old loop, the volume radius
`getOuterRadius() * 2.f`, is in the hook and keeps its unoptimized compile.

**The teardown is unchanged** (G-38): `EndPlay` unregisters, then detaches the relay host through the sibling, then
unbinds; the delivery route's erase stays inside the session's unregistration.
