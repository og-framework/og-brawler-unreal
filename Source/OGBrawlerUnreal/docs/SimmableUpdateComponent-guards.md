<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `SimmableUpdateComponent.h` and `.cpp` — guards

Every prohibition that survived the task-25 conversion of both files, except the fifteen that moved to
`SimulatableNetSyncComponent-guards.md` in og-ue-simulationcomponentandmanagersplit tasks 20a (the component's
reflected wire surface, ten) and 20b (the relay-host link, five), and the six that moved to
`UESimulatableOwnerBinding-guards.md` in task 21 (the stage and the RPC tail); §R names each successor. G-38 is new in
task 20b. Each entry has an **opaque,
stable id**. In the source a single line `// ⛔G-nn  docs/SimmableUpdateComponent-guards.md` sits
directly above the statement or declaration where the forbidden edit would be typed. An **absence
tag** — one with a blank line under it — marks the place where something retired would be re-added.

**If this file and the source disagree, the source is authoritative and this file is stale.**

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time check, moves to
§R and its number is spent forever. A retired id may be **named** in prose or an assertion message;
it may never again appear as a `⛔G-nn` tag.

⭐ The join is machine-checked in both directions by this repository's guard-tag lint. ⚠ It checks
that an entry EXISTS, never that its text is TRUE.

⛔ **Nothing in this file is a rationale.** Orientation, provenance and derivations live in
`SimmableUpdateComponent-rationale.md`; §12 there is the conversion record (every pre-conversion
comment and where it went).

**Quoted fences.** Each entry quotes the comment it replaced, as shipped. Where R0 found the shipped
text false, the entry says so under **Correction** and the prohibition is stated as the tree
supports it.

---

## G-01 — a refused `SimCharacterId` allocation is final: never retry it, never fall back to another id

**Site:** `if (assigned == SimCharacterId::None) return;` in `tryRegisterWithNewFramework`.

**The prohibition (task 25, written with the tag):** when the authority's
`BrawlerRosterUImpl::allocateSimCharacterId` (reached as `regManager->roster()`) answers `SimCharacterId::None`, this character does
not register, ever. Do not `scheduleNextRegistrationAttempt()` here, and do not substitute
`GetUniqueID()` or any other id source.

**Consequence.** A retry cannot succeed — the allocator is count-up and never reuses an id (see
`SimCharacterId-guards.md` G-01) — so it only spins until the 600-attempt `DeferredReg` `checkf`. A
fallback id is per-process again: it differs between peers, can collide with an authority-assigned
id, and breaks the peer-stable, never-reused property every id consumer relies on. The refusal is
already logged at Error and `OG_CHECK`ed by the roster; that site's own guard is
`BrawlerRosterUImpl-guards.md` G-02 (the manager's G-78 until og-ue-simulationcomponentandmanagersplit task 23).

**What breaks if it moves.** Nothing mechanical: the early return is the only thing that ends the
loop on refusal.

---

## G-02 — the role test is the world's net mode, never `HasAuthority()`

**Site:** `const bool isAuthority = (GetNetMode() != NM_Client);` in `tryInitializeWithManager`.

**As shipped:**
> ⛔ NEVER HasAuthority() HERE -- it is always true on a non-replicated actor and would
> disagree with the world-mode gate the manager picks its own role with. §3

**Correction (R0).** The stated mechanism is about the wrong actor. "Always true on a non-replicated
actor" describes the MANAGER (`bReplicates = false`); this component's owner is a replicated pawn. The
prohibition stands for the other half of the sentence: `ASimulationManagerUImpl::instanceFor(isAuthority)`
indexes the manager slot that `ASimulationManagerUImpl::BeginPlay` filled from
`worldIsAuthority = (worldNetMode != NM_Client)` (since og-ue-simulationcomponentandmanagersplit task 25 the slot is the
session's registry, and since task 27b the session's `beginPlay` fills it, with the same predicate). Only the same world-level predicate is guaranteed to
pick the manager of this component's own world; an actor-role test answers differently for an actor
spawned locally on a client.

**Consequence.** A component resolving the other role's manager registers into the wrong storage — in
PIE both managers live in one process, so the lookup succeeds and nothing fails loudly.

**Scope note.** The same predicate is computed in `tryRegisterWithNewFramework`, `EndPlay` and
`TickComponent` (`vizIsAuthority`); only this site carried the fence, and only this site is tagged.

---

## G-03 — absence: nothing tier-related is bound per character

**Site:** absence tag in `tryInitializeWithManager`, after the input-collection cache.

**As shipped:**
> ⛔ NOTHING TIER-RELATED IS BOUND HERE -- the tier is a WIRE property, bound once per world. §5

**The prohibition.** Do not re-add a per-character tier consumer binding here. The client-side
consumer is the session's `m_replicatedTierConsumer` (`UESimulationSession`, OGSimulationUnreal; the manager's own
member until og-ue-simulationcomponentandmanagersplit task 25), emplaced from the manager's `BeginPlay` through the
session's `beginTierConsumer`, one per world; the tier itself replicates from the per-connection
`ASimulationConnectionRelay`.

**Consequence.** A per-character binding reintroduces one consumer per sibling on a shared wire —
the shape whose disagreement the per-connection relay removed.

---

## G-04 — a simulated proxy must not register a local input provider

**Site:** `const bool isLocallyPredicted = !isAuthority && ownerActor != nullptr && ownerActor->GetLocalRole() == ROLE_AutonomousProxy;`

**As shipped:**
> ⛔ A SIMULATED PROXY MUST NOT REGISTER A PROVIDER -- absence is what buys it a relay store. §4

**Consequence.** Provider presence is the core's identity test (rationale §4). A provider on a
simulated proxy selects `registerLocalCharacter` instead of `registerRemoteCharacter`: the proxy gets
no relay store and is predicted from nothing, and its captures are put on the wire as if local.

**What breaks if it moves.** `m_hasLocalInputProvider` latches this exact evaluation (G-15); a second,
differently-worded role test would split the two halves of the owner-skip check.

---

## G-05 — the provider lambda captures the input collection and the id, never the manager

**Site:** `inputProvider = [ic, id](const SimulationTimeStep& step, ...) { ... };`

**As shipped:**
> ⛔ DO NOT CAPTURE THE MANAGER -- this lambda reaches nothing outside its arguments. §4

**Consequence.** The core hands the character's own `LocalInputCache` in as an argument. Capturing
the manager re-creates a lifetime edge from a core-held callable to an actor whose teardown order
against the registration is not guaranteed.

---

## G-06 — spawn the relay host BEFORE registering the delivery route (ordering)

**Site:** the `if (isAuthority)` block that calls `ASimulationInputRelay::spawnForCharacter`, above
the `noteDelayedInputComponent` call.

**As shipped:**
> ⛔ SPAWN THE HOST BEFORE THE noteDelayedInputComponent ROUTE -- until that route exists,
> relayRemoteInput stages into m_detachedRelayStagingRing, unseen and unlogged. §6

**Correction (R0).** The window is stated backwards. Until the route exists,
`UESimulationSession::relayRemoteInput` (the manager's until og-ue-simulationcomponentandmanagersplit task 26a) finds no component and returns — nothing is staged
anywhere. The hazard is the opposite window: the route registered while no host is linked. Then
`relayRemoteInput` reaches `stageRelayedInput`, which falls back to `m_detachedRelayStagingRing`
(the sibling's G-13 since task 20b; this file's G-12 before): unseen by every client and unlogged.

**The prohibition.** Do not move `noteDelayedInputComponent` above the host spawn. ⚠ This is an
ordering fence.

---

## G-07 — the delivery route is registered once, authority only, and is the ring-out score push's roster

**Site:** `if (isAuthority) regManager->noteDelayedInputComponent(simId, *this);`

**As shipped:**
> ⛔ ROUTE REGISTERED ONCE, erased in unregisterFromNewFramework. Authority only. §7

**The prohibition.** Register it here, once, under `isAuthority` exactly — not per parked slot, not
on a client, and not behind any further condition.

**Consequence.** The coordinator's drain and the malformed-slot fallback resolve against this route;
a missing route drops delivered input. And `m_delayedInputComponentsById` (the session's since task 26a) is also the roster
`BrawlerRosterUImpl::pushRingoutScores` walks (`BrawlerRosterUImpl-rationale.md` §3, the ring-out score
push): narrow this registration and those characters' scores silently never leave the server.

---

## G-24 — `hasInputComponent()` is the render echo's local-versus-remote discriminator

**Site:** `const bool hasLiveLocalInput = m_ownerInputCollection != nullptr && m_ownerInputCollection->hasInputComponent();`

**As shipped:**
> ⛔ hasInputComponent() IS THE LOCAL-vs-REMOTE DISCRIMINATOR -- the host's own pawn is LOCAL.
>
> ⛔ NOT the registration-time ROLE_AutonomousProxy test: that one excludes the host. §10

**Consequence.** The `ROLE_AutonomousProxy` test (G-04) is false for the listen-server host's own
pawn — the character this echo exists for.

---

## G-25 — a remote proxy's echo reads the relay store's LAST-KNOWN input, not the scheduled read

**Site:** `const std::optional<simulatableBrawler::PlayerInput> vizPlayerInput = simulatableBrawler::selectVisualizationInput(...);`

**As shipped:**
> REMOTE proxy: the relay store's LAST-KNOWN input, NOT the per-tick scheduled read. §10

**Consequence.** The visualization wants "what is this player doing"; the scheduled read answers
"which input does tick N run on" and lags by the schedule (rationale §10).

---

## G-26 — the movement debug draw's master switch is read alone, first, every frame

**Site:** `if (OGBrawlerMovementVisualizationCVars::movementVizEnabled)` in `TickComponent`.

**As shipped (at the cvar's namespace and at this call site):**
> ⛔ DEFAULT 0, AND OFF MUST COST ONE BOOL READ. ... the gate at the call site is a plain early-out
> and never a conjunct inside a walk that has already paid for something.
>
> ⚠ READ PER FRAME, DELIBERATELY -- DO NOT CONVERT THIS TO A ONE-TIME READ.
>
> ⛔ THE MASTER READ, ALONE, FIRST: with the cvar off this costs one bool read and nothing else.

**Consequence.** A cached read defeats the point of a viz cvar (a tuner types it mid-session); a
conjunct behind a slice fetch makes "off" cost a walk. The reasoning is also written at
`brawlerMovementVisualization::visualize` (rationale §10).

---

## G-27 — the input-history master switch is read alone, first

**Site:** `if (!inputHistoryVisualizationUImpl::masterEnabled()) return;`

**As shipped:**
> ⛔ THE MASTER, READ ALONE, FIRST: with it off this costs one bool read and returns.

**Consequence.** Default-off must cost one `bool` read. `InputHistoryDisplay-rationale.md` ("The
gate") describes this exact statement.

---

## G-28 — the both-off early-out sits above the id walk, never as a conjunct in it

**Site:** `if (!feedRowPanel && !feedAnyBar) return;`

**As shipped:**
> ⛔ ABOVE THE ID WALK BELOW, NEVER A CONJUNCT IN IT: off must not pay for that walk.

---

## G-29 — the row poll keeps its gate: the first local player's character alone

**Site:** `if (rowCharacterId.has_value() && *rowCharacterId == ownCharacterId)`.

**As shipped:**
> ⛔ THE ROW POLL KEEPS THE GATE BELOW. Its source is this client's own capture
> line, which a remote proxy does not have at all.
>
> ⛔ STILL THE FIRST LOCAL PLAYER'S CHARACTER ALONE.

---

## G-30 — the lanes poll feeds THIS component's own id, unconditionally, from the same snapshot

**Site:** `vizManager->inputHistoryFeed().pollInputHistoryLanes(ownCharacterId, ...)` (the feed's poll since
og-ue-simulationcomponentandmanagersplit task 22; the manager's before).

**As shipped:**
> ⛔ THIS COMPONENT'S OWN ID, UNCONDITIONALLY.
>
> The machine state comes off the SAME attackSimState the block-prediction viz above already reads,
> at the same site. ⛔ NO NEW SEAM IS OPENED FOR IT.
>
> ⚠ SAMPLED LIVE AND NEVER BACK-FILLED.

**Consequence.** Binding the lanes to the selected character is what made a remote proxy's corrections
invisible (rationale §10); a second machine-state source is a new cross-thread seam.

---

## G-31 — the lanes' "no input" is the panel's own classification

**Site:** `liveInput = brawlerInputHistoryVisualization::captureRowFieldsOf(...)`.

**As shipped:**
> The pause's input half is the PANEL'S OWN classification of this capture,
> so "no input" means one thing across both displays.
> ⛔ NO SECOND IDEA OF NEUTRAL IS DERIVED HERE.

---

## G-38 — `EndPlay` detaches the relay host through the sibling, between the unregister and the unbind

**Site:** `if (isNetSyncBound()) netSync().detachInputRelayHost(isAuthority);` in `EndPlay`. (New in task 20b; since
task 21 the sibling is reached through the binding's `isNetSyncBound()` and `netSync()`.)

**The prohibition.** Keep this call, and keep it after `unregisterFromNewFramework` and before `unbindNetSync()`. Do
not leave the detach to the sibling's own `EndPlay`: that one detaches only while no binding is attached (the sibling's
G-11), so with this call gone a sibling that ends first never detaches, and one that ends last detaches after the
unbind; the order between the two components' `EndPlay` calls is not a contract.

**Consequence.** Moved above the unregister, the authority destroys the host before the manager has dropped the
character. Moved below `unbindNetSync()`, it never runs, because the unbind clears the binding's sibling pointer. Deleted (or moved
below the unbind), the host stays linked and, on the authority, alive whenever the sibling ends first (rationale §17).

---

## G-35 — `OGSIM_OPTIMIZE_ON` closes the pair at true end-of-file

**Site:** `OGSIM_OPTIMIZE_ON`, the last line.

**As shipped:**
> ⛔ CLOSES THE OGSIM_OPTIMIZE_OFF PAIR AT TRUE EOF. The file carried no closing pragma from
> its first commit, so the whole TU compiled unoptimized in every build. §2

**Consequence.** Deleting it silently returns the file to "off to EOF". No lint in `tools/` checks
the pair (verified: no script there names `OGSIM_OPTIMIZE_ON`), so this tag is the only fence.

---

## §R — Retired ids

⛔ **An id is never reused.** A retired id may be named in prose; it may never again appear as a
`⛔G-nn` tag.

### G-09 — RETIRED by og-ue-simulationcomponentandmanagersplit task 20a (2026-10-21): moved to `SimulatableNetSyncComponent-guards.md` G-03

**Was:** *"once a wire-format mismatch is latched, every later OnRep no-ops"*. Its tag sat in `SimmableUpdateComponent.cpp`, above the latch test at the top of `OnRep_CorrectionState`.

**Why it retired.** Its subject moved. Task 20a moved the component's reflected wire surface (the replicated
correction-state buffer, its notification, the input RPC and the lifetime registrations) into
`USimulatableNetSyncComponent` (OGSimulationUnreal), and the guarded code went with it. The entry moved with
its text unchanged apart from its site and cross-references. **Successor:** `SimulatableNetSyncComponent-guards.md` G-03.

### G-10 — RETIRED by og-ue-simulationcomponentandmanagersplit task 20a (2026-10-21): moved to `SimulatableNetSyncComponent-guards.md` G-04

**Was:** *"absence: the retired correction-input notification stood here"*. Its tag sat in `SimmableUpdateComponent.cpp`, an absence tag after `OnRep_CorrectionState`'s definition.

**Why it retired.** Its subject moved. Task 20a moved the component's reflected wire surface (the replicated
correction-state buffer, its notification, the input RPC and the lifetime registrations) into
`USimulatableNetSyncComponent` (OGSimulationUnreal), and the guarded code went with it. The entry moved with
its text unchanged apart from its site and cross-references. **Successor:** `SimulatableNetSyncComponent-guards.md` G-04.

### G-11 — RETIRED by og-ue-simulationcomponentandmanagersplit task 20a (2026-10-21): moved to `SimulatableNetSyncComponent-guards.md` G-05

**Was:** *"absence: the ring's notification stood here"*. Its tag sat in `SimmableUpdateComponent.cpp`, an absence tag after G-10.

**Why it retired.** Its subject moved. Task 20a moved the component's reflected wire surface (the replicated
correction-state buffer, its notification, the input RPC and the lifetime registrations) into
`USimulatableNetSyncComponent` (OGSimulationUnreal), and the guarded code went with it. The entry moved with
its text unchanged apart from its site and cross-references. **Successor:** `SimulatableNetSyncComponent-guards.md` G-05.

### G-17 — RETIRED by og-ue-simulationcomponentandmanagersplit task 20a (2026-10-21): moved to `SimulatableNetSyncComponent-guards.md` G-06

**Was:** *"an empty input bundle is idle traffic, not a version mismatch"*. Its tag sat in `SimmableUpdateComponent.cpp`, above the empty-bundle return in `ServerReceiveRemoteMove_Implementation`.

**Why it retired.** Its subject moved. Task 20a moved the component's reflected wire surface (the replicated
correction-state buffer, its notification, the input RPC and the lifetime registrations) into
`USimulatableNetSyncComponent` (OGSimulationUnreal), and the guarded code went with it. The entry moved with
its text unchanged apart from its site and cross-references. **Successor:** `SimulatableNetSyncComponent-guards.md` G-06.

### G-18 — RETIRED by og-ue-simulationcomponentandmanagersplit task 20a (2026-10-21): moved to `SimulatableNetSyncComponent-guards.md` G-07

**Was:** *"refuse a mismatched bundle, but do not disconnect here"*. Its tag sat in `SimmableUpdateComponent.cpp`, above the `return;` that closed the RPC's version-mismatch branch.

**Why it retired.** Its subject moved. Task 20a moved the component's reflected wire surface (the replicated
correction-state buffer, its notification, the input RPC and the lifetime registrations) into
`USimulatableNetSyncComponent` (OGSimulationUnreal), and the guarded code went with it. The entry moved with
its text unchanged apart from its site and cross-references. **Successor:** `SimulatableNetSyncComponent-guards.md` G-07.

### G-32 — RETIRED by og-ue-simulationcomponentandmanagersplit task 20a (2026-10-21): moved to `SimulatableNetSyncComponent-guards.md` G-08

**Was:** *"absence: the correction-input property's lifetime registration"*. Its tag sat in `SimmableUpdateComponent.cpp`, an absence tag in `GetLifetimeReplicatedProps` after the state-buffer registration.

**Why it retired.** Its subject moved. Task 20a moved the component's reflected wire surface (the replicated
correction-state buffer, its notification, the input RPC and the lifetime registrations) into
`USimulatableNetSyncComponent` (OGSimulationUnreal), and the guarded code went with it. The entry moved with
its text unchanged apart from its site and cross-references. **Successor:** `SimulatableNetSyncComponent-guards.md` G-08.

### G-33 — RETIRED by og-ue-simulationcomponentandmanagersplit task 20a (2026-10-21): moved to `SimulatableNetSyncComponent-guards.md` G-09

**Was:** *"absence: the ring's registration on this component"*. Its tag sat in `SimmableUpdateComponent.cpp`, an absence tag after G-32.

**Why it retired.** Its subject moved. Task 20a moved the component's reflected wire surface (the replicated
correction-state buffer, its notification, the input RPC and the lifetime registrations) into
`USimulatableNetSyncComponent` (OGSimulationUnreal), and the guarded code went with it. The entry moved with
its text unchanged apart from its site and cross-references. **Successor:** `SimulatableNetSyncComponent-guards.md` G-09.

### G-34 — RETIRED by og-ue-simulationcomponentandmanagersplit task 20a (2026-10-21): moved to `SimulatableNetSyncComponent-guards.md` G-10

**Was:** *"absence: the owner-only tier registration"*. Its tag sat in `SimmableUpdateComponent.cpp`, an absence tag after G-33.

**Why it retired.** Its subject moved. Task 20a moved the component's reflected wire surface (the replicated
correction-state buffer, its notification, the input RPC and the lifetime registrations) into
`USimulatableNetSyncComponent` (OGSimulationUnreal), and the guarded code went with it. The entry moved with
its text unchanged apart from its site and cross-references. **Successor:** `SimulatableNetSyncComponent-guards.md` G-10.

### G-36 — RETIRED by og-ue-simulationcomponentandmanagersplit task 20a (2026-10-21): moved to `SimulatableNetSyncComponent-guards.md` G-01

**Was:** *"absence (header): the replicated correction-input property"*. Its tag sat in `SimmableUpdateComponent.h`, an absence tag after the `ServerReceiveRemoteMove` declaration.

**Why it retired.** Its subject moved. Task 20a moved the component's reflected wire surface (the replicated
correction-state buffer, its notification, the input RPC and the lifetime registrations) into
`USimulatableNetSyncComponent` (OGSimulationUnreal), and the guarded code went with it. The entry moved with
its text unchanged apart from its site and cross-references. **Successor:** `SimulatableNetSyncComponent-guards.md` G-01.

### G-37 — RETIRED by og-ue-simulationcomponentandmanagersplit task 20a (2026-10-21): moved to `SimulatableNetSyncComponent-guards.md` G-02

**Was:** *"absence (header): the relayed-input ring as a property of this component"*. Its tag sat in `SimmableUpdateComponent.h`, an absence tag after G-36.

**Why it retired.** Its subject moved. Task 20a moved the component's reflected wire surface (the replicated
correction-state buffer, its notification, the input RPC and the lifetime registrations) into
`USimulatableNetSyncComponent` (OGSimulationUnreal), and the guarded code went with it. The entry moved with
its text unchanged apart from its site and cross-references. **Successor:** `SimulatableNetSyncComponent-guards.md` G-02.

### G-08 — RETIRED by og-ue-simulationcomponentandmanagersplit task 20b (2026-10-21): moved to `SimulatableNetSyncComponent-guards.md` G-12

**Was:** *"on EndPlay the authority destroys the relay host; a client only unbinds it"*. Its tag sat in `SimmableUpdateComponent.cpp`, at the inner `if (isAuthority)` in `EndPlay` (now in the sibling's `detachInputRelayHost`).

**Why it retired.** Its subject moved. Task 20b moved the component's relay-host link (the host pointer, the two
fallback rings, the accessors, the relayed-ring callbacks, the attach, the arrival check, the staging target, the
detach) and the tier send into `USimulatableNetSyncComponent` (OGSimulationUnreal), and the guarded code went with it.
The entry moved with its text unchanged apart from its site and cross-references. **Successor:**
`SimulatableNetSyncComponent-guards.md` G-12.

### G-12 — RETIRED by og-ue-simulationcomponentandmanagersplit task 20b (2026-10-21): moved to `SimulatableNetSyncComponent-guards.md` G-13

**Was:** *"the no-host staging fallback is `m_detachedRelayStagingRing`, never `m_detachedRelayRing`"*. Its tag sat in `SimmableUpdateComponent.cpp`, above the staging-target choice in `stageRelayedInput` (now the sibling's `relayStagingTarget`).

**Why it retired.** Its subject moved. Task 20b moved the component's relay-host link (the host pointer, the two
fallback rings, the accessors, the relayed-ring callbacks, the attach, the arrival check, the staging target, the
detach) and the tier send into `USimulatableNetSyncComponent` (OGSimulationUnreal), and the guarded code went with it.
The entry moved with its text unchanged apart from its site and cross-references. **Successor:**
`SimulatableNetSyncComponent-guards.md` G-13.

### G-14 — RETIRED by og-ue-simulationcomponentandmanagersplit task 20b (2026-10-21): moved to `SimulatableNetSyncComponent-guards.md` G-14

**Was:** *"the host callback captures a weak pointer, never a raw `this`"*. Its tag sat in `SimmableUpdateComponent.cpp`, above `host->setOnRelayedInputReceivedCallback` in `attachInputRelayHost`.

**Why it retired.** Its subject moved. Task 20b moved the component's relay-host link (the host pointer, the two
fallback rings, the accessors, the relayed-ring callbacks, the attach, the arrival check, the staging target, the
detach) and the tier send into `USimulatableNetSyncComponent` (OGSimulationUnreal), and the guarded code went with it.
The entry moved with its text unchanged apart from its site and cross-references. **Successor:**
`SimulatableNetSyncComponent-guards.md` G-14.

### G-15 — RETIRED by og-ue-simulationcomponentandmanagersplit task 20b (2026-10-21): moved to `SimulatableNetSyncComponent-guards.md` G-15

**Was:** *"the owner-skip check stays gated on `num() > 0` and reads the LATCHED provider decision"*. Its tag sat in `SimmableUpdateComponent.cpp`, above the owner-skip condition in `onRelayedInputRingArrived`.

**Why it retired.** Its subject moved. Task 20b moved the component's relay-host link (the host pointer, the two
fallback rings, the accessors, the relayed-ring callbacks, the attach, the arrival check, the staging target, the
detach) and the tier send into `USimulatableNetSyncComponent` (OGSimulationUnreal), and the guarded code went with it.
The entry moved with its text unchanged apart from its site and cross-references. **Successor:**
`SimulatableNetSyncComponent-guards.md` G-15.

### G-16 — RETIRED by og-ue-simulationcomponentandmanagersplit task 20b (2026-10-21): moved to `SimulatableNetSyncComponent-guards.md` G-16

**Was:** *"the owner-skip divergence is an `ensure`, never a `check`"*. Its tag sat in `SimmableUpdateComponent.cpp`, above the `ensureMsgf` in `onRelayedInputRingArrived`.

**Why it retired.** Its subject moved. Task 20b moved the component's relay-host link (the host pointer, the two
fallback rings, the accessors, the relayed-ring callbacks, the attach, the arrival check, the staging target, the
detach) and the tier send into `USimulatableNetSyncComponent` (OGSimulationUnreal), and the guarded code went with it.
The entry moved with its text unchanged apart from its site and cross-references. **Successor:**
`SimulatableNetSyncComponent-guards.md` G-16.

### G-13 — RETIRED by og-ue-simulationcomponentandmanagersplit task 21 (2026-10-21): moved to `UESimulatableOwnerBinding-guards.md` G-01

**Was:** *"a stage overflow is counted on the host, never absorbed"*. Its tag sat in `SimmableUpdateComponent.cpp`, above the overflow count in `stageRelayedInput`.

**Why it retired.** Its subject moved. Task 21 moved the component's typed half (the five binding overrides, the
forwarders, the typed owner-concept members, the sibling pointer, the owner-concept aliases, the five latency stamp
blocks and the stage) into the CRTP template `UESimulatableOwnerBinding` (OGSimulationUnreal), and the guarded code went
with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:**
`UESimulatableOwnerBinding-guards.md` G-01.

### G-19 — RETIRED by og-ue-simulationcomponentandmanagersplit task 21 (2026-10-21): moved to `UESimulatableOwnerBinding-guards.md` G-02

**Was:** *"no netcode policy below the fence; the order is fence, one RTT sample, core loop"*. Its tag sat in `SimmableUpdateComponent.cpp`, above the first statement of `receiveRemoteMoveBundle` (the authority manager lookup).

**Why it retired.** Its subject moved. Task 21 moved the component's typed half (the five binding overrides, the
forwarders, the typed owner-concept members, the sibling pointer, the owner-concept aliases, the five latency stamp
blocks and the stage) into the CRTP template `UESimulatableOwnerBinding` (OGSimulationUnreal), and the guarded code went
with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:**
`UESimulatableOwnerBinding-guards.md` G-02.

### G-20 — RETIRED by og-ue-simulationcomponentandmanagersplit task 21 (2026-10-21): moved to `UESimulatableOwnerBinding-guards.md` G-03

**Was:** *"resolve the root connection once"*. Its tag sat in `SimmableUpdateComponent.cpp`, above the `rootConn` resolution in `receiveRemoteMoveBundle`.

**Why it retired.** Its subject moved. Task 21 moved the component's typed half (the five binding overrides, the
forwarders, the typed owner-concept members, the sibling pointer, the owner-concept aliases, the five latency stamp
blocks and the stage) into the CRTP template `UESimulatableOwnerBinding` (OGSimulationUnreal), and the guarded code went
with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:**
`UESimulatableOwnerBinding-guards.md` G-03.

### G-21 — RETIRED by og-ue-simulationcomponentandmanagersplit task 21 (2026-10-21): moved to `UESimulatableOwnerBinding-guards.md` G-04

**Was:** *"the no-wire fallback is adapter-side; the core only ever sees a valid wire"*. Its tag sat in `SimmableUpdateComponent.cpp`, above the no-coordinator-or-no-wire branch in `receiveRemoteMoveBundle`.

**Why it retired.** Its subject moved. Task 21 moved the component's typed half (the five binding overrides, the
forwarders, the typed owner-concept members, the sibling pointer, the owner-concept aliases, the five latency stamp
blocks and the stage) into the CRTP template `UESimulatableOwnerBinding` (OGSimulationUnreal), and the guarded code went
with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:**
`UESimulatableOwnerBinding-guards.md` G-04.

### G-22 — RETIRED by og-ue-simulationcomponentandmanagersplit task 21 (2026-10-21): moved to `UESimulatableOwnerBinding-guards.md` G-05

**Was:** *"one RTT sample per bundle, never per slot"*. Its tag sat in `SimmableUpdateComponent.cpp`, above `coordinator->noteRttSample` in `receiveRemoteMoveBundle`.

**Why it retired.** Its subject moved. Task 21 moved the component's typed half (the five binding overrides, the
forwarders, the typed owner-concept members, the sibling pointer, the owner-concept aliases, the five latency stamp
blocks and the stage) into the CRTP template `UESimulatableOwnerBinding` (OGSimulationUnreal), and the guarded code went
with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:**
`UESimulatableOwnerBinding-guards.md` G-05.

### G-23 — RETIRED by og-ue-simulationcomponentandmanagersplit task 21 (2026-10-21): moved to `UESimulatableOwnerBinding-guards.md` G-06

**Was:** *"the delay is applied exactly once, park-to-release, on the original capture tick"*. Its tag sat in `SimmableUpdateComponent.cpp`, above `coordinator->receiveInputBundle` in `receiveRemoteMoveBundle`.

**Why it retired.** Its subject moved. Task 21 moved the component's typed half (the five binding overrides, the
forwarders, the typed owner-concept members, the sibling pointer, the owner-concept aliases, the five latency stamp
blocks and the stage) into the CRTP template `UESimulatableOwnerBinding` (OGSimulationUnreal), and the guarded code went
with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:**
`UESimulatableOwnerBinding-guards.md` G-06.
