<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: m_replicatedInputSyncedBuffer -- ABSENCE FENCE (G-32, G-36): the retired replicated correction-input property. It must NOT resolve -->
<!-- lint-external-ref: OnRep_CorrectionInput -- ABSENCE FENCE (G-10, G-36): the retired notification of that property. It must NOT resolve -->
# `SimmableUpdateComponent.h` and `.cpp` — guards

Every prohibition that survived the task-25 conversion of both files. Each entry has an **opaque,
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
`ASimulationManagerUImpl::allocateSimCharacterId` answers `SimCharacterId::None`, this character does
not register, ever. Do not `scheduleNextRegistrationAttempt()` here, and do not substitute
`GetUniqueID()` or any other id source.

**Consequence.** A retry cannot succeed — the allocator is count-up and never reuses an id (see
`SimCharacterId-guards.md` G-01) — so it only spins until the 600-attempt `DeferredReg` `checkf`. A
fallback id is per-process again: it differs between peers, can collide with an authority-assigned
id, and breaks the peer-stable, never-reused property every id consumer relies on. The refusal is
already logged at Error and `OG_CHECK`ed by the manager; that site's own guard is
`SimulationManagerUImpl-guards.md` G-78.

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
`worldIsAuthority = (worldNetMode != NM_Client)`. Only the same world-level predicate is guaranteed to
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
consumer is `ASimulationManagerUImpl::m_replicatedTierConsumer`, emplaced in the manager's
`BeginPlay`, one per world; the tier itself replicates from the per-connection
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
`ASimulationManagerUImpl::relayRemoteInput` finds no component and returns — nothing is staged
anywhere. The hazard is the opposite window: the route registered while no host is linked. Then
`relayRemoteInput` reaches `stageRelayedInput`, which falls back to `m_detachedRelayStagingRing`
(G-12): unseen by every client and unlogged.

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
a missing route drops delivered input. And `m_delayedInputComponentsById` is also the roster
`pushRingoutScoresToCharacters` walks (`SimulationManagerUImpl-rationale.md`, the ring-out score
push): narrow this registration and those characters' scores silently never leave the server.

---

## G-08 — on EndPlay the authority destroys the relay host; a client only unbinds it

**Site:** the inner `if (isAuthority)` in `EndPlay`.

**As shipped:**
> ⛔ AUTHORITY DESTROYS, CLIENT ONLY UNBINDS -- a local destroy races the destruction bunch. §6

**Consequence.** A client-side `Destroy` of a replicated actor races the server's destruction bunch.
The authority path is `ASimulationInputRelay::detachFromParentAndDestroy`, which clears the callback,
removes the Iris dependent-actor link (Iris builds only) and then destroys.

---

## G-09 — once a wire-format mismatch is latched, every later `OnRep_CorrectionState` no-ops

**Site:** `if (m_wireFormatMismatchDetected) return;`

**As shipped:**
> ⛔ ONCE A MISMATCH IS LATCHED EVERY LATER OnRep NO-OPS -- a mismatched peer must not correct. §8

**Consequence.** Without the latch a mismatched peer would drive correction logic with bytes it cannot
parse (rationale §8).

---

## G-10 — absence: `OnRep_CorrectionInput` stood here

**Site:** absence tag after `OnRep_CorrectionState`'s definition.

**As shipped:**
> ⛔ `OnRep_CorrectionInput` STOOD HERE, all of it retired -- see SimulationNetSync::sendCorrectionAll. §9

**The prohibition.** Do not re-add the server→client correction-input notification. The channel, its
property (G-36), its registration (G-32) and its core binding were retired together (rationale §9).

---

## G-11 — absence: `OnRep_RelayedInputRing()` stood here

**Site:** absence tag after G-10.

**As shipped:**
> ⛔ `OnRep_RelayedInputRing()` STOOD HERE -- it moved to the relay host with its property. §6

**The prohibition.** Do not re-add a ring notification on this component; the ring and its
notification live on `ASimulationInputRelay` (G-37, G-33, rationale §6).

---

## G-12 — the no-host staging fallback is `m_detachedRelayStagingRing`, never `m_detachedRelayRing`

**Site:** `FRelayedInputRing& stage = (host != nullptr) ? host->editRelayedInputStagingRing() : m_detachedRelayStagingRing;`

**As shipped (header, at `m_detachedRelayStagingRing`):**
> [T34] The same fallback, for the flush STAGE. `stageRelayedInput` must have somewhere to write when
> no host is linked, and it must NOT be the detached ring above: that one is what
> `getRelayedInputRing()` hands the core, so staging into it would let an unpublished burst read back
> as if it had replicated.

**Consequence.** As quoted: an unpublished burst would read back through the core's read accessor as
if it had replicated. The edit is typed here, which is why the tag moved from the header member to
this statement.

---

## G-13 — a stage overflow is counted on the host, never absorbed

**Site:** `if (outcome.droppedOldest && host != nullptr) host->noteStageOverflowDrop();`

**As shipped:**
> ⛔ COUNTED, NOT ABSORBED, and on the HOST -- the one input loss this side can see. §6

**Consequence.** A burst longer than `relayedInputRing::kMaxDepth` in one frame drops its oldest
entry; this is the only visible record of that input loss (rationale §6, "Staging, not writing").

---

## G-14 — the host callback captures a weak pointer, never a raw `this`

**Site:** `host->setOnRelayedInputReceivedCallback([weakSelf](const FRelayedInputRing& ring) { ... });`

**As shipped:**
> ⛔ WEAK CAPTURE, NEVER A RAW `this` -- the host can outlive this component by a frame. §6

**Consequence.** A raw `this` is a dangling call from a replicated notification during teardown.

---

## G-15 — the owner-skip check stays gated on `num() > 0` and reads the LATCHED provider decision

**Site:** `if (m_hasLocalInputProvider && ring.num() > 0 && !m_loggedOwnerSkipDivergence)` in
`onRelayedInputRingArrived`.

**As shipped (two fences, merged because they govern the same condition):**
> ⛔ DO NOT DROP THE `num() > 0` GATE -- the host replicates to its owner, so empty is normal.

> ⛔ LATCHED, NEVER RECOMPUTED AT ARRIVAL -- onRelayedInputRingArrived's divergence check
> is only meaningful if it reads literally this evaluation. §6

(The second stood at `m_hasLocalInputProvider = isLocallyPredicted;` in `tryRegisterWithNewFramework`;
the edit it forbids — a fresh role test in place of the latch — is typed here.)

**Consequence.** `ASimulationInputRelay` is `bAlwaysRelevant` and does reach its owner (only the
property is `COND_SkipOwner`), so an empty ring at a provider-present character is normal: dropping
the gate fires on every clean run. Recomputing provider presence here compares a different
evaluation from the one the core registered with, so the check no longer tests what it claims.

---

## G-16 — the owner-skip divergence is an `ensure`, never a `check`

**Site:** `ensureMsgf(false, TEXT("[InputRelay] relay ring arrived for a provider-present character (id=%u)"), ...)`

**As shipped:**
> ⛔ `ensure`, NEVER `check`, one-shot -- a bandwidth regression must stay playable to diagnose.

**Consequence.** A `check` turns a bandwidth/correctness diagnosis into a crash; the one-shot latch
(`m_loggedOwnerSkipDivergence`) keeps it from spamming.

---

## G-17 — an empty input bundle is idle traffic, not a version mismatch

**Site:** `if (bundle.wireBytes.Num() == 0) return;` in `ServerReceiveRemoteMove_Implementation`.

**As shipped:**
> ⛔ AN EMPTY BUNDLE IS IDLE TRAFFIC, NOT A MISMATCH -- no wire header means no version byte. §8

**Consequence.** Removing or reordering it below the version test logs a false wire-format mismatch
every idle frame.

---

## G-18 — refuse a mismatched bundle, but do not disconnect here

**Site:** the `return;` closing the version-mismatch branch of `ServerReceiveRemoteMove_Implementation`.

**As shipped:**
> ⛔ REFUSE, BUT DO NOT DISCONNECT HERE -- the disconnect path is engine-managed. §8

**Consequence.** Disconnect policy is a dedicated-server validation concern owned by the engine's
connection path, not this fence (rationale §8).

---

## G-19 — no netcode policy below the fence; the order is fence, one RTT sample, core loop

**Site:** `ASimulationManagerUImpl* authorityManager = ASimulationManagerUImpl::instanceFor(/*isAuthority=*/true);`

**As shipped:**
> ⛔ NO NETCODE POLICY BELOW THIS FENCE -- tier derivation, dedup, park/drain, the malformed-
> slot fence and the per-slot loop are all core. This side resolves engine primitives only. §7
>
> ⛔ ORDER IS LOAD-BEARING: fence, then ONE RTT sample, then the core per-slot loop. §7

**Consequence.** Policy added here is policy a second engine's adapter would silently lack, and the
core's tests do not cover (rationale §7).

---

## G-20 — resolve the root connection once

**Site:** `UNetConnection* rootConn = (coordinator != nullptr) ? GetRootNetConnection(GetOwner()) : nullptr;`

**As shipped:**
> ⛔ RESOLVE THE ROOT CONNECTION ONCE -- split-screen siblings collapse to their shared wire. §7

**Consequence.** A child connection, or a per-slot re-resolve, splits one wire into several tier and
slot keys.

---

## G-21 — the no-wire fallback is adapter-side; the core only ever sees a valid wire

**Site:** `if (coordinator == nullptr || rootConn == nullptr)`.

**As shipped:**
> ⛔ THE NO-WIRE FALLBACK IS ADAPTER-SIDE -- the core only ever sees a valid wire. §7

**Consequence.** Passing a null wire into `ServerReceptionCoordinator` hands the core a sentinel it is
written never to see.

---

## G-22 — one RTT sample per bundle, never per slot

**Site:** `coordinator->noteRttSample(handle, id, ..., readRoundTripMs(rootConn), *this);`

**As shipped:**
> ⛔ ONE RTT SAMPLE PER BUNDLE, NEVER PER SLOT -- per-slot couples the tier EMA to depth. §7

**Consequence.** Sampling per slot feeds one reading into the tier EMA up to `kMaxSlots` times and
couples smoothing to redundancy depth.

---

## G-23 — the delay is applied exactly once, park-to-release, on the original capture tick

**Site:** `coordinator->receiveInputBundle<SimulatableBrawler>(id, handle, playerSlot, bundle, *authorityManager, *authorityManager);`

**As shipped:**
> ⛔ THE DELAY IS APPLIED EXACTLY ONCE (park-to-release) -- a released input keeps its ORIGINAL
> captureTick, and SimulationInputResolution::collectInputAll pops it in ARRIVAL order. §7

**Consequence.** Any offset or re-stamp added at this call applies the delay twice.

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

**Site:** `vizManager->pollInputHistoryLanes(ownCharacterId, ...)`.

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

## G-32 — absence: the `m_replicatedInputSyncedBuffer` lifetime registration

**Site:** absence tag in `GetLifetimeReplicatedProps`, after the state-buffer registration.

**As shipped:**
> ⛔ THE `m_replicatedInputSyncedBuffer` REGISTRATION STOOD HERE -- removed in the SAME edit
> as the member: one half alone is a compile error, the other a silently dead field. §9
>
> ⛔ REMOVING A PROPERTY NEEDS NO VERSION BUMP -- registrations are lifetime entries. §9

---

## G-33 — absence: the ring's registration on this component

**Site:** absence tag after G-32.

**As shipped:**
> ⛔ `DOREPLIFETIME(USimmableUpdateComponent, m_relayedInputRing)` STOOD HERE, with NO COND_.
> It is now COND_SkipOwner on the ring's own per-character dependent object. §6
>
> ⛔ THE COND_ COULD NOT HAVE BEEN ADDED HERE -- the shared atomic batch is what forced it. §6

**Consequence.** Re-adding it here — even with the condition — re-couples the ring to the correction
state's atomic replication batch (rationale §6). `SimulationManagerUImpl-rationale.md` cites this
fence as evidence the component no longer carries the ring.

---

## G-34 — absence: the `COND_OwnerOnly` tier registration

**Site:** absence tag after G-33.

**As shipped:**
> ⛔ THE COND_OwnerOnly TIER REGISTRATION IS GONE -- owner-only RELEVANCY narrows it now. §5

**Consequence.** The tier replicates from `ASimulationConnectionRelay` (`bOnlyRelevantToOwner`); a
per-character registration here restores one tier per sibling on a shared wire.

---

## G-35 — `OGSIM_OPTIMIZE_ON` closes the pair at true end-of-file

**Site:** `OGSIM_OPTIMIZE_ON`, the last line.

**As shipped:**
> ⛔ CLOSES THE OGSIM_OPTIMIZE_OFF PAIR AT TRUE EOF. The file carried no closing pragma from
> its first commit, so the whole TU compiled unoptimized in every build. §2

**Consequence.** Deleting it silently returns the file to "off to EOF". No lint in `tools/` checks
the pair (verified: no script there names `OGSIM_OPTIMIZE_ON`), so this tag is the only fence.

---

## G-36 — absence (header): the replicated correction-input property

**Site:** absence tag in the private section of `SimmableUpdateComponent.h`, after
`m_clientToServerInputSyncedBuffer`.

**As shipped:**
> [og-netcode-v2-input-relay T8] THE REPLICATED CORRECTION-INPUT PROPERTY IS
> GONE. `UPROPERTY(ReplicatedUsing = OnRep_CorrectionInput)
> FSimulationInputSyncBuffer m_replicatedInputSyncedBuffer;` and its
> `OnRep_CorrectionInput()` UFUNCTION stood here, registered in
> GetLifetimeReplicatedProps beside the state buffer. The server->client echo
> of "the input I applied for you" is retired; a remote character's input
> reaches peers on the relay ring below, and the correction state names which
> capture it applied via T4's ref.
>
> NOTE the surviving `m_clientToServerInputSyncedBuffer` above is a DIFFERENT
> member with the same type and the opposite direction — it is not replicated
> and it is not affected.

**Correction (R0).** "on the relay ring below" — the ring is no longer declared below; it is a
property of `ASimulationInputRelay` (G-37).

---

## G-37 — absence (header): the relayed-input ring as a property of this component

**Site:** absence tag after G-36.

**As shipped (the retirement paragraph; the host-link paragraph that followed it is rationale §6):**
> [og-netcode-v2-input-relay / T1] The outbound input relay ring stood here as
> `UPROPERTY(ReplicatedUsing = OnRep_RelayedInputRing) FRelayedInputRing
> m_relayedInputRing;` with its `OnRep_RelayedInputRing()` UFUNCTION, and was
> registered with a plain DOREPLIFETIME (no COND_) in GetLifetimeReplicatedProps.
>
> ⭐ [T39] ALL THREE MOVED TO ASimulationInputRelay, IN ONE EDIT — property,
> OnRep and registration together ...

**Consequence.** Two reasons, rationale §6: scheduling (a shared atomic Iris batch killed ring and
state together) and ownership (`COND_SkipOwner` on an actor owned by the character).

---

## §R — Retired ids

None.
