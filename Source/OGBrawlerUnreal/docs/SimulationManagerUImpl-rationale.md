<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `ASimulationManagerUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/SimulationManagerUImpl.h` and
`Source/OGBrawlerUnreal/SimulationManagerUImpl.cpp`. **The source files carry the guards; this
file carries the reasoning, the provenance and the worked derivations.** The `§N` marks in those
two files point here.

<!-- ================= DECLARED LINT ESCAPES =================================
     Every token below is CORRECT and cannot resolve. None is here to silence a
     name that is wrong: the two wrong names this lint found were FIXED, not
     escaped (§11 F-32-5 and F-32-6).
     ========================================================================= -->
<!-- lint-external-ref: AActor::Owner -- Unreal Engine type, outside every scan root; the engine is not vendored into this repository -->
<!-- lint-external-ref: UNetConnection::Tick -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: UDataStreamChannel::Tick -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: UDataStreamChannel::WriteData -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: GameEngine::GetMaxTickRate -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: FRewindData::FindValidResimFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FPBDRigidsSolver::ConditionalApplyRewind_Internal -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: TickFlush -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: OutBytes -- Unreal Engine connection stat, outside every scan root -->
<!-- lint-external-ref: OutPackets -- Unreal Engine connection stat, outside every scan root -->
<!-- lint-external-ref: StatPeriod -- Unreal Engine stat window, outside every scan root -->
<!-- lint-external-ref: DesiredTickRate -- Unreal Engine net-driver field, outside every scan root -->
<!-- lint-external-ref: MaxNetTickRate -- Unreal Engine net-driver setting, outside every scan root -->
<!-- lint-external-ref: BaseEngine.ini -- Unreal Engine's own config file, outside every scan root -->
<!-- lint-external-ref: ChaosMarshallingManager.h -- Chaos engine header, outside every scan root -->
<!-- lint-external-ref: PBDRigidsSolver.cpp -- Chaos engine source, outside every scan root -->
<!-- lint-external-ref: NoLogging -- an Unreal log-verbosity enumerator, written as an ini value -->
<!-- lint-external-ref: notReady -- a field NAME inside one probe's log format string, not a declared identifier -->
<!-- lint-external-ref: writesThisFrame -- a term in the stated ceiling formula, not a declared identifier -->
<!-- lint-external-ref: .Build.cs -- a FILE-EXTENSION pattern naming a class of build files, not one file -->
<!-- lint-external-ref: .Target.cs -- a FILE-EXTENSION pattern naming a class of build files, not one file -->
<!-- lint-external-ref: ResimCooldownTicks -- ABSENCE FENCE (§3): the ini key that was built and removed on a ruling. It must NOT resolve; the day it does, that ruling has been reversed -->
<!-- lint-external-ref: routeInboundHits -- ABSENCE FENCE (§5): the retired adapter-side routing shim. It must NOT resolve -->
<!-- lint-external-ref: m_replicatedInputSyncedBuffer -- ABSENCE FENCE (§6): the retired second write of the discharged dual-write fence. It must NOT resolve -->
<!-- lint-external-ref: getLatestInput -- ABSENCE FENCE (§6): the deleted correction-input read. It must NOT resolve -->
<!-- lint-external-ref: sampleAndDeriveConnectionTier -- ABSENCE FENCE (§7): relocated to the RPC boundary and deleted here. It must NOT resolve -->
<!-- lint-external-ref: tryEnqueueDelayedRemoteInput -- ABSENCE FENCE (§7): relocated to the RPC boundary and deleted here. It must NOT resolve -->

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes. It is deliberately not
> in the `og-simulation` docs tier, which is MPL-2.0 and travels with a different repository.

**Read `SimulationManagerUImpl.h`'s orientation block first.** It states the shape — two role
instances, the thread table, the construction order, the one delay formula, the four session
knobs — in about fifty lines. Everything below assumes it.

---

## §1 Threads, and why every entry point here is narrow

`ASimulationManagerUImpl` straddles two threads and owns the boundary between them.

| | runs on | what it is |
|---|---|---|
| `BeginPlay` / `EndPlay` | GAME | the composition root |
| `onConnectionTierReceived` / `Replayed`, `onRelayDelayFloorReceived` / `Replayed`, `onInputRelayHostReady` | GAME | the four replication listeners |
| `deliverRemoteInput`, `relayRemoteInput`, `noteDelayedInputComponent` | GAME | the transport sinks, driven from the RPC receipt path |
| `InjectInputs_External` → `releaseDelayedInputsForStep` | GAME | the drain, the reap and PROBE A |
| `FSimulationManagerAsyncCallback::OnPreSimulate_Internal`, `OnPostSolve_Internal`, `TriggerRewindIfNeeded_Internal`, `FirstPreResimStep_Internal`, `ApplyCorrections_Internal` | PHYSICS | the five Chaos hooks, and everything the core `SimulationManager` runs beneath them |
| `pollInputHistory`, `pollInputHistoryLanes` | GAME | the input-history display's render-rate feed, driven from `USimmableUpdateComponent::TickComponent` |

**There are two crossings, and they are not alike.** One is a write of one scalar; the other is
a read of a whole capture, and it is an accepted tear rather than a synchronized access.

**The write crossing carries one scalar.**
`publishClientEffectiveInputDelayTicks` writes a lone `std::atomic<int32>` inside
`SimulationInputResolution`, which `SimulationInputResolution::collectInputAll` loads once per tick on the physics thread.
The worst a race can do is apply a new delay one tick late.

**Everything else on this class is game-thread-only and has no internal synchronization:**
`m_receptionCoordinator`, `m_frameHealthProbe`, `m_relayWriteProbe`, `m_connectionBudgetProbe`,
`m_inputHistory` and `m_delayedInputComponentsById`. `m_manager->editResimGateProbe()` is the mirror case on the
other side: physics-thread-only, and its correctness rests on nothing else touching it.

### The read crossing — `pollInputHistory`, and why its tear is accepted

`getLocalInputCache` opens a game-thread door onto `m_localInputCaches`, whose *contents* are
written on the physics thread by `SimulationInputResolution::collectInputAll` (`LocalInputCache::push`)
and by `wipeAllForResync` (`LocalInputCache::clear`). Unlike its neighbour `getLastRelayedInput`,
this reader is **not** same-thread with that writer. `pollInputHistory` is its only caller, so the
argument belongs here rather than at the door.

**What is *not* racing, and it is the larger half.** The `std::unordered_map` that holds the lines
is only ever restructured on the **game thread** — `registerLocalCharacter` and
`unregisterCharacter` run from registration, which the peer's own thread roster records as
game-thread in full. The poll's `find` is therefore same-thread with the only writer of the map's
structure, and the container cannot rehash under it. `LocalInputCache`'s slot vector is sized once
in its constructor and never resized, so the storage a read lands in cannot be reallocated either.
**The exposure is confined to the bytes of one slot**, not to the container that holds it.

**What can tear, and what the worst observable consequence is.** A slot is
`{ tick, occupied, input }`, and `push` writes those in that order, so a game-thread read can
observe a new `tick` beside the previous occupant's payload — or a payload half-updated. A
`simulatableBrawler::PlayerInput` is several vectors rather than the single byte
`SlotStateProvenance` rides the same unsynchronized access with, so the precedent is cited but
**not borrowed**. The worst outcome is that **one capture tick is classified from wrong values**:
one row of a diagnostic panel briefly shows the wrong direction glyph, button mask or action id.
Because the row fold is idempotent, a wrong classification does not self-heal — it persists as a
mis-drawn or mis-split row until that row scrolls out of the 64-row ring.

**Why it cannot corrupt the ring.** `InputHistoryRowRing::appendCapture` is keyed on the capture
tick **alone**, and the poll passes its own loop index, never a value read out of the slot. A torn
payload therefore cannot move a tick, cannot reach `++tickCount` twice, and cannot make a stale
tick rejoin an older row. The blast radius of a tear is the three *fields* of at most one row.

**Why it cannot reach the simulation.** The ring is client-local: it is never replicated, never
enters a correction payload and never reaches `compute_checksum`. The poll's route into the core is
`const`-only in both directions — `getLocalInputCache` returns a pointer to `const`, and both
reconciliation seams are `const` members reached through a `const` reference (`slotStateProvenance`
on the diagnostics view, `getAppliedCaptureTickRef` on the peer itself) — so there is no write path
back at all, and nothing downstream of the ring decides anything.

**Why it cannot fault.** A torn read yields wrong values, never a wrong pointer: a
`static_assert` at the poll pins `simulatableBrawler::PlayerInput` trivially destructible (it owns
no memory) and the sub-input the display actually reads trivially copyable. A member that owned an
allocation would turn this tear into a crash, and that assert is what stops one landing quietly.
*(The composite fails `is_trivially_copyable` through `std::tuple`'s implementation alone, not
through any member of its own — `std::tuple<int, float>` fails the same trait identically.)*

**And it is bounded in time.** The read is diagnostics-only and gated on one predicate at its call
site, so it can be switched off outright without reshaping anything.

**The second caller shares this crossing and heals faster than the first.** `pollInputHistoryLanes`
reads the physics-written correction cache through the same two `const` seams, fronted by
`ReconciliationSlotReader` so both are asked at one simulation tick. Its worst outcome is one wrong
*cell*, not one wrong row — and unlike the row fold, which is idempotent and therefore keeps a bad
classification until it scrolls away, a provenance cell is **rewritten on every poll while its tick
is still resident**, so a torn lineage byte is corrected on the next frame. The machine-state lane
crosses nothing at all: its sample is read at the caller's own visualization site, from state the
block-prediction visualization on the line above already holds.

**A third feed inside the same passthrough, and it crosses NOTHING at all.** The input-delay
decomposition `pollInputHistoryLanes` computes for `OGBrawler.InputHistoryInputDelay` reads
`m_replicatedTierConsumer` (GAME-THREAD-written, §5), `m_manager->getTimeConfig()`
(GAME-THREAD-bound at `BeginPlay`, mutated only by the OnRep listeners) and
`m_inputResolution.getClientEffectiveInputDelayTicks()` (the atomic the WRITE crossing above
publishes — GAME-THREAD-written, and this reader is on the SAME thread as that writer, not the
physics thread that only loads it). `pollInputHistoryLanes` itself already runs on the game
thread. Three same-thread reads of three game-thread-written values is not a crossing, so the
CROSSING table above names it in the READ row's prose rather than adding a third bullet, and
this paragraph is the same-thread argument that entry names. ⛔ **NO NEW PUBLIC ACCESSOR was
added for any of the three** — the tier consumer and the TimeConfig pointer were already public
(`getReplicatedTierConsumer`, `getTimeConfigPtr`), and the resolution peer's atomic is read
through the SAME private member this method already touches for the offset.

**The presence test crosses nothing either.** `ReconciliationSlotReader::hasCorrectionCache()`
asks `m_reconciliation.findCorrectionCache<T>(id) != nullptr` through the same reader the two
diagnostic seams already share, inside the same `pollInputHistoryLanes` call; the map it
inspects is mutated on the GAME THREAD alone (`createCacheFor`/`removeCacheFor`, from the
registration facade), never the physics thread, so this is a same-thread read like the three
above it, not a fourth crossing.

### Why the passthroughs are narrow rather than `edit*()` accessors

`requestInputDelayIncreaseStall`, `publishClientEffectiveInputDelayTicks`, `getLastRelayedInput`,
`getLocalInputCache`, `pollInputHistory`, `getInputHistoryRows`, `pollInputHistoryLanes`,
`getInputHistoryLanes`, `noteResimRequest` and
`noteResimGrant` are one-purpose entry points. Each of the objects behind
them is otherwise driven **exclusively** by the core `SimulationManager`'s tick loop. Handing
game-thread `UObject` code a general mutable handle to one of them would invite exactly the
cross-thread reach the entry point exists to bound. **Do not widen one into an accessor.**

⚠ **This list has now gone stale twice**, both times because a new member matching the pattern
exactly was added without extending the sentence. The rule it states is about the *class* of
member, not about these ten names: **every public member of `ASimulationManagerUImpl` that
forwards to one core-owned object for one purpose belongs here**, and the enumeration is a
reading aid. Replacing the list outright with that sentence would end the drift, at the cost of
the ten anchors `doc_anchor_lint.ps1` currently resolves through it — a trade worth making only
if the lead wants the anchors spent elsewhere.

`getInputHistoryRows` and `getInputHistoryLanes` both return a pointer to `const` for the same
reason: the panel that draws the rows, and the bars that draw the cells, must not write one.

`requestInputDelayIncreaseStall` additionally guards on `runsPrediction()`: a server or
standalone manager has no client clock at all, and `getClientClock()` would `std::terminate`.

`pollInputHistoryLanes` guards on the same predicate for the same reason, around the one
line that reads `getNetworkEstimator()`: an authority manager has no prediction offset, and
it passes `std::nullopt` rather than a zero, because a display told "the offset is 0" would
draw an authority marker on the newest cell. It guards the input-delay decomposition the same
way, on `m_replicatedTierConsumer.has_value() && m_manager.has_value()`, and for the same
reason: a role with neither has nothing to decompose.

⛔ **THE LIST ABOVE IS UNCHANGED BY THE INPUT-DELAY DISPLAY, AND THAT IS THE POINT.**
`pollInputHistoryLanes` already names the one entry point this display reaches through; its
delay decomposition is three more same-thread reads INSIDE that existing passthrough, not a
new one beside it. Widening what one narrow entry point does is exactly what "narrow" is
meant to allow — it is adding a second *name* that would have been the drift this banner
warns about.

⛔ **THE CLOCK READING ADDS NO NEW CROSSING CLASS, AND THIS SAYS SO RATHER THAN LEAVING IT
SILENT.** The `ClockDriftReading` the same passthrough builds for the frame meter's clock line
(`InputHistoryDisplay-rationale.md` §7.12) is the SAME PAIR of postures argued two paragraphs
above for the prediction offset, not a third thing. `getTargetPredictionTick` and
`getLastAuthorityTick` read `NetworkTimeEstimator` state its own contract declares
GAME-THREAD-written, by `updateRTT` and `recordAuthorityTick`, and this reader is on that
thread — same-thread, nothing to tear. `getPredictionTick`, `evaluateDrift` and
`getRequiredInputDelayIncreaseStallTicks` reach the physics-written prediction tick and stall
debt, which is the accepted tear already taken: a naturally-aligned four-byte load on x64
cannot tear, so the worst case is one line of diagnostic text one tick stale. **The CROSSING
table gains no bullet and the NARROW PASSTHROUGHS list gains no name** — `pollInputHistoryLanes`
was already on it, and this is more reads inside that one entry point.
⛔ It takes `runsPrediction()` for the harder of the two reasons: `getClientClock()` does not
return a wrong number on a role that does not predict, it calls `std::terminate`.

⛔ **THE CLOCK'S EVENT SEAM IS SEVEN MORE READS INSIDE THAT SAME PASSTHROUGH, AND STILL NO
NEW CROSSING CLASS.** `skipCount`, `lastSkipTick`, `stallCount`, `lastStallTick`,
`hardResyncCount`, `lastHardResyncFromTick` and `lastHardResyncToTick` are plain
physics-written words on `ClientPredictionClock`, taken under the SAME `runsPrediction()` guard,
in the SAME call, for the SAME display that decides nothing. Each is a naturally-aligned
four-byte load that cannot tear, so the accepted tear argued above covers all seven and **the
CROSSING table gains no bullet and the NARROW PASSTHROUGHS list gains no name**.
⛔ **THEY ARE READ THROUGH `getDiagnostics()`, NEVER THROUGH A BARE ACCESSOR** — the clock
deliberately publishes none, so the display cannot acquire a second, unfenced route to the same
words.

⭐ **THE COUNT OF EACH PAIR IS READ BEFORE ITS TICK, AND THE ORDER IS THE ARGUMENT.** The clock
writes them the other way round — tick, then count — at every site in `advancePrediction`. Two
opposed orders make one of the two possible straddles unreachable: a reader that has already
seen the incremented count is, by the writer's program order, past the tick-write as well, and
its tick-read comes later still. ⛔ **A FRESH COUNT BESIDE A STALE TICK CANNOT BE OBSERVED**
here. What can is the reverse — a stale count beside a fresh tick — and it costs nothing: the
poll's difference is zero, so it files nothing and the next poll picks the event up whole.
⛔ **DO NOT REVERSE THE READ ORDER.** It is not a style choice: reading tick-first makes the
unreachable case reachable, and that case is the one that would attribute an event to a tick
that predates it — a wrong answer rather than a late one, on the one crossing in this file whose
consumer draws a position from the value.

⭐ **The offset is read at the POLL, not at the draw, and that placement is the whole point.**
The lane axis is built from the `liveTick` this same call is given, so pairing the offset
with it there makes the authority marker and the axis it is measured against one snapshot by
construction. An earlier version handed the display a `const` accessor that read the
prediction tick a second time at HUD draw time; a physics step landing between the two reads
moved the marker a column while its printed offset held, and the accessor is gone rather than
tolerated. ⛔ Do not reintroduce a clock accessor for a display: give the display the poll's
  reading.

### One acknowledged wart

`getServerReceptionTick()` reads `m_serverClock` through `getServerClock()`, which is written on the physics thread, from
the game thread with no synchronization. The RTT sample path has always tolerated this and the
tier EMA is insensitive to a one-tick skew. It was left as-is so that relocating the reception
subsystem into the core stayed a pure relocation; a follow-up may source it mapper-derived, the
way the drain already does.

---

## §2 Construction, ownership and teardown

**Declaration order in the header IS construction order**, because C++ constructs class members
in declaration order regardless of initializer-list order:

```
m_storage → m_staticData → m_reconciliation → m_inputResolution → m_netSync
```

then `BeginPlay` emplaces, in order:

```
m_physAdapter / m_physReaderAdapter / m_queryAdapter → m_integrationLayer → m_manager
    → m_replicatedTierConsumer   (borrows m_manager's TimeConfig)
    → m_receptionCoordinator     (borrows m_manager's TimeConfig, authority only)
```

`EndPlay` unwinds the last two **before** `m_manager`, because both hold `const TimeConfig&`
into it.

### The reorder hazard, stated exactly

Nothing enforces the peer ordering but the language rule. **There is no `static_assert` and no
`-Wreorder`-as-error anywhere in this tree's `.Build.cs` / `.Target.cs` files.** A future edit
that reordered `m_reconciliation` / `m_inputResolution` / `m_netSync` while leaving the
initializer lists textually unchanged would compile silently and construct in the new, wrong
order.

It has not bitten because every one of those three constructors is a **trivial reference store**:
none calls into a sibling peer during construction, so a reorder today would only bind a
reference to a not-yet-constructed-but-not-yet-*accessed* object. **It becomes load-bearing the
day a constructor BODY — not just its initializer list — calls a method on one of those sibling
references.** That is undefined behaviour for which this tree has no compiler diagnostic.

### `m_staticData` is the ownership root

`simulatableBrawler::StaticData` must be constructed in place and **never copied or moved**. Its
nested sub-`StaticData` members — `m_attackSimulationStaticData` and
`m_guardSimulationStaticData` — hold references bound to sibling members `m_attackSequences` and
`m_attackCircle`. A copy or move would leave those internal references dangling into the
moved-from original, silently. Every downstream consumer (the integration executor, the
`SimulationManager`) therefore takes it by `const&` only; there is **no by-value `StaticData`
path anywhere in the tree.** This invariant was migrated here from the now-deleted
`SimulationIntegrationExecutor::getStaticData()` site.

### The alias chain

`BrawlerSimulatables` is the single source of truth for the game's simulatable pack: widen that
one alias and every storage and executor type below it inherits the widening. The aliases are
**class-scoped**, not file-scoped, so these names cannot leak into the global namespace from a
widely-included adapter header. OGSim primitives are named unqualified because the whole OGSim
core lives in the global namespace.

`SimulationIntegrationExecutor`'s leading three parameters are engine- and game-specific, so
`apply_t` cannot unpack the pack marker into them; `BrawlerIntegrationExecFor_UE` fixes those
three slots first and `apply_t` then applies the pack, preserving the single-source-of-truth
property.

`getTimeConfigPtr()` returns a **pointer, not a reference**, precisely so that the
pre-construction state is representable rather than undefined behaviour. Callers emplace lazily
and retry.

---

## §3 The session knobs

Three ini keys are read at composition, in `BeginPlay`, through the only `GConfig` uses in this
codebase. **Every one takes the same four steps:**

| step | what | why it is not optional |
|---|---|---|
| 1 INTAKE | read the ini once, before the manager exists where possible | a value that arrives after the first publish has to be absorbed as a *change* |
| 2 CLAMP / VALIDATE | out of range is **reported**, never silently corrected | a floor that big is a sizing mistake the operator needs to see |
| 3 SET | stamp the effective value into the one shared `TimeConfig` | every derivation site must read one number |
| 4 PROVE | an **unconditional** `Warning` line naming the value actually stored | see below |

**Step 4 is at `Warning`, not `Log`**, because `Config/DefaultEngine.ini` sets `LogOGNet=Warning`
and a `Log` line therefore does not exist on a dedicated server. Both ini homes are accepted for
every key — `DefaultGame.ini` first, then `DefaultEngine.ini` — because the former is the
conventional place for a gameplay-tuning value and the latter is where this project's other
netcode settings already live. **And it is unconditional**,
because a line that is absent both when the key was read and when it was not cannot distinguish
those two cases — which is the whole question a proof line exists to answer. Each proof line
reports values **read back from `TimeConfig`**, never the parsed request, so it cannot claim a
setting the manager did not store.

**None of these may become a cvar.** The rotation width, because a run's probe output is read
*against* the cadence and a value that moves mid-run makes those readings unattributable. The
resim policy, because it is pushed into every `StateCorrectionCache` and read on the game thread
at the correction-landing site **with no synchronization** — which is sound only because it is
written once at composition, before any correction can land.

### `RelayDelayFloorTicks` — the session floor

**Authority only, and that is correctness rather than economy.** The floor is server-owned
session state that is *replicated* to clients. A client reading its own ini could disagree with
the server, which is the two-ends-diverge failure the whole tier ruling exists to avoid.

The clamped value is stamped into `TimeConfig` and then published on the session timing relay,
which the composition root (`USimulationManagerSubsystem::OnWorldBeginPlay`) spawns BEFORE this
manager precisely so that write has somewhere to land: reaching the failure branch there means no
client will ever learn a nonzero floor.

Two intake points share one clamp: this ini override, and `onRelayDelayFloorReceived` where the
value arrives off the wire as a `uint8` and is clamped rather than trusted (a corrupt or
version-mismatched byte must not schedule a read past the point where the delay line has already
evicted the capture). The setter clamps again; the call sites exist so an out-of-range value is
*visible* in the log rather than silently corrected.

`logRelayDelayFloorAdvisory` is **advisory only, never an assert**, and is called from both
intake points. Floor `0` is the documented "scheduled regime OFF" mode, so
`classifyRelayDelayFloor` (`Network/ConnectionTierTable.h`) never flags it; that function carries
the full classification table.

### `CorrectionRotationK` — the state rotation width

Controls how many characters' correction-state buffers `SimulationNetSync::sendCorrectionAll`
writes per tick, round-robin, so each character's state replicates at `60 * K / N` Hz. A
character not written is not dirty and costs zero bytes, so this is a real wire saving rather
than a deferred write. It turns an emergent cadence into a decided number: at N characters the
per-character correction rate should read `60*K/N` in `[DivergenceProbe.Window]`.

**Authority only, but NOT for the floor's reason.** `K` is never replicated at all — only the
authority sends corrections, and a receiver reconciles against whatever arrives without needing
to know the sender's cadence. A client read would have no reader.

> ⚠ **The compiled default is `TimeConfig::correctionRotationK` and this document does not
> restate its value.** See §11: the `.cpp` comment restated it, the value moved, and the comment
> did not. Read it at the declaration.

**Sentinel collision, harmless and knowingly.** An ini that literally reads
`CorrectionRotationK=-1` cannot be told from an absent key, because `-1` is also the intake
variable's own "not present" sentinel. The proof line reports the effective number either way.
(`-1 == absent` is the intake variable's own convention, not the field's.)

### `ResimTriggerPolicy` — the resim-gate policy

Decides which landed corrections open the resim gate: `FrontierExact`, the compiled default,
which reproduces the legacy gate; or `OnDisagreement`, the designed trigger. The field lives on
`TimeConfig` (`PCTimeManagement/TimeConfig.h`); the mechanism is `OGSimulation/ResimGatePolicy.h`
and `StateCorrectionCache`. The shipped configuration is set by the ini key, currently
`ResimTriggerPolicy=OnDisagreement`.
**`TimeConfig::resimTriggerPolicy`'s own declaration states the compiled default and the shipped
configuration as two separate facts, and they differ — read them there.**

**There is deliberately no `ResimCooldownTicks` key.** A trigger-rate ceiling was built here and
removed on a user ruling: it defers acting on a correction *already known to disagree*, which is
the defect this mechanism repairs. The throttle is structural instead. If you are here because a
design document names that key, the code is right and the document predates the ruling.

**Not authority-gated, unlike the two above, and that is the point rather than an oversight.**
The resim gate exists only on a predicting client — an authority allocates no correction caches
and never rewinds. Gating this intake on authority would read the ini on the one role that cannot
use it and skip it on the role that can. Applying it on both roles keeps one `TimeConfig` shape
and costs nothing; on a server the value simply has no reader.

**Presence is a bool, not a sentinel value.** The value is a string, so there is no numeric
sentinel to collide with, and `GetString` already reports presence. The step-2 parse is
case-insensitive because an ini is hand-written, and an unrecognised policy string is **reported
with the compiled default kept**: a typo that quietly selected the other value would change gate
behaviour on a build nobody thinks they changed.

For this knob the proof line is also the **behaviour-neutrality receipt**. The gate ships
defaulted to reproduce the old one, so every claim made from a later run — "the ratios are
indistinguishable from the baseline" — is a claim about *which policy was live*. Without that
line in the log it is an assertion about the source rather than an observation about the run.
`depthPolicy` therefore carries `(inert under FrontierExact)` rather than falling silent, and
`rateLimit = none (structural)` is stated rather than omitted, because the absence of a cooldown
is a ruling: a reader must be able to tell "the ceiling is off" from "this build predates it".

### The retired fourth knob

There was a fourth key: a session-configurable **relay-ring retention depth**. The bare-C1
flush-on-poll write path replaced the mechanism it sized — the stage capacity is
`relayedInputRing::kMaxDepth`, a compile-time constant with no ini key to feed — and the key, its
clamp intake, its setter and its `[RelayDepth]` startup proof line were removed together. Two ⛔ RETIRED fences
in the `.cpp` mark where the intake and the apply block used to sit. **They are absence fences
and they are not compressible any further:** the identifier they are about occurs nowhere in this
tree, so there is no symbol to grep and the comment is the only record.

---

## §4 Log categories and `RouteOGMessage`

`RouteOGMessage` routes one SIMLOG string to one `LogOG*` category by its leading `[Tag]`.

**Tag-matching runs against `body`** — `fmsg` minus a leading `[Verbose]` or `[Warning]` token,
both nine characters — while the **full** `fmsg`, severity token included, is what is logged. So
one message can carry both a severity escalation and a routable tag.

### Order is load-bearing

`[Resim.Input]` must be matched **ahead of** the `[Resim.` catch-all, because the catch-all is a
prefix of it. `[Resim.Input]` is a per-character-per-resim-tick line — the same volume class as
`[CollectInput]` — and it landed in the rare-lifecycle bucket purely by inheriting the `[Resim.`
prefix from `[Resim.Pre]` / `[Resim.Post]`, which really are rare, while `LogOGSim` shipped at
Verbose. So the highest-frequency line in the system was filed as a lifecycle event and printed
by default.

It was **re-routed rather than renamed.** Both were available, and renaming would have silenced
it just as well — but `[Resim.Input]` is the name the impl notes, the PIE scripts and the
spectrum design all use when they tell an operator what to grep for. A rename invalidates every
one of those instructions for a cosmetic gain. Re-routing keeps the string an operator already
knows and changes only which knob controls it: `LogOGSimTick=Verbose` now shows the resim
table, exactly as it shows `[CollectInput]`, which is routed there under the comment
*"dominates log volume"*.

### Three probe families, three categories

`LogOGRelayProbe`, `LogOGDivergenceProbe` and `LogOGResimProbe` each own a category, and the
reason is the same all three times: **both severities ride one tag family** — per-window
summaries at `Warning`, per-event detail at `Verbose` — so its own category is the only thing
that lets `LogOGRelayProbe=Warning` (the shipped default) keep the summaries while dropping the
detail, and `LogOGRelayProbe=NoLogging` drop both, without disturbing any other channel. The
same holds for `LogOGResimProbe=Warning` and `LogOGDivergenceProbe=Warning`. Routed to `LogOGNet` instead, a family would be
inseparable from the whole replication bucket; left unrouted, its Verbose half would fall to
`LogOG=Warning` and be unreachable with no way to turn it on.

**No probe family may be filed under `[Resim.` or `[ResimCheck.`.** The first inherits
`LogOGSim=Verbose`. The second is split across `LogOGSim` and `LogOGSimTick`, so a family filed
under it could not be switched on or off as one thing — which is why `[ResimCheck.IsSimilar]`
has zero occurrences in every log on disk.

`[ResimProbe` sits ahead of the `[Resim.` catch-all **defensively**. It does not strictly need
to: `[Resim.` requires the dot and `[ResimProbe` has a `P` there, so the two cannot collide
today. It sits there because the one thing that would make them collide is somebody widening the
catch-all to `[Resim`, and the consequence would be silent.

**One `StartsWith` covers each family** — the router tests `body.StartsWith("[ResimProbe")` and
its two siblings — so a future sub-tag needs no router edit.

`[RelayProbe.Frame]` is the one **server-side** member of the relay family. It shares that
category deliberately: it measures the *cause* of the cadence `[RelayProbe.Arrival]` measures the
*effect* of, and the two are only interpretable together, so one knob should turn both on.

`[DivergenceProbe.*]`'s signal is not new — `StateCorrectionCache::tryInsertingCorrectState` has
computed the verdict on every correction for as long as that method has existed. What was missing
was a **route**: the cache's own line carries no tag, so it lands on the `LogOG` fallback at
`Log` severity and `LogOG=Warning` suppresses it. The router branch is the whole of that fix.


### The tag families, in full

The source names each family by its prefix; the members are:

| category | tags |
|---|---|
| `LogOGRelayProbe` | `[RelayProbe.Read]`, `[RelayProbe.Arrival]`, `[RelayProbe.Stale]`, `[RelayProbe.Miss]`, `[RelayProbe.Delta]`, `[RelayProbe.Frame]` |
| `LogOGResimProbe` | `[ResimProbe.Gate]`, `[ResimProbe.Chaos]`, `[ResimProbe.Apply]`, `[ResimProbe.Landing]`, `[ResimProbe.Request]`, `[ResimProbe.Stranded]`, `[ResimProbe.Session]`, `[ResimProbe.Frame]` |
| `LogOGDivergenceProbe` | `[DivergenceProbe.Correction]`, `[DivergenceProbe.Window]` |

`[RelayProbe.Miss]` says *why* each miss missed — an in-span coverage hole, asking above the
newest arrival, or below the oldest; `[RelayProbe.Delta]` is the signed probe-tick-to-newest
distribution; `[RelayProbe.Stale]` is the longest consecutive fallback run. `[ResimProbe.Chaos]`
is the request/grant/refusal ledger and the requested-vs-granted pin; `[ResimProbe.Apply]` is the
apply edge and the replay span; `[ResimProbe.Landing]` is the frontier-landing split;
`[ResimProbe.Stranded]` is the per-event Verbose companion to it.

---

## §5 The client's effective input delay

Two independent channels feed one number:

```
effective = max(floor, tierKnown ? tierInputDelayTicks(tier)
                                 : rttTierInputDelays[kMaxConnectionTierIndex])
```

The pre-arrival case is therefore `max(floor, rttTierInputDelays[kMaxConnectionTierIndex])`. The
published value lands in `SimulationInputResolution`'s
`m_clientEffectiveInputDelayTicks` atomic.

The **floor** rides the session relay (`ASimulationTimingRelay`); the **tier** rides the
per-connection relay (`ASimulationConnectionRelay`). The two `OnRep`s are genuinely independent
and can land in either order, which is exactly why both call **one** recompute holding **both**
cached inputs rather than each writing the effective-delay atomic on its own: two writers each
holding half the formula would answer with a stale other-half whenever their `OnRep`s
interleaved. The composition root's baseline publish goes through the same site.

Both arms are evaluated inside `ReplicatedTierConsumer::effectiveInputDelayTicks`, which is the
single derivation the server's `ServerInputDelayQueue` mirrors — that shared helper is what makes
the two ends agree by construction rather than by coincidence.

`recomputeAndPublishEffectiveInputDelay` **returns the change** in published delay
(new − previous) so a caller that must pay for an increase can. A caller publishing a baseline
rather than reacting to a transition ignores it. `m_lastPublishedEffectiveInputDelayTicks` is kept
on this class rather than read back out of the net sync, because the delta is a game-thread
bookkeeping quantity: the atomic exists to hand one scalar to the physics thread, not to be used
as shared state. It is seeded to `0` so the composition root's first publish reports its full
value as the delta — which that caller deliberately ignores, since nothing has been predicted yet.

### The tier is server-owned

The **server is the sole owner of the RTT tier**. It derives each connection's tier from its own
per-connection ping reading and replicates the result to the owning client as
`ASimulationConnectionRelay::m_connectionTier`. The client never
computes a tier and never samples RTT for tier purposes — that is what makes client/server tier
disagreement impossible *by construction* rather than merely unlikely.

**One consumer per WORLD, not per CHARACTER.** A tier is a property of the wire, and every
character on a machine shares one wire. The retired per-character shape had N characters each
driving the *same* effective-delay atomic and each requesting its own tier-transition stall — so
for a couch co-op client, one wire transition produced two stall requests against a clock that
*accumulates* debt. One listener per world makes the request count match the transition count.
Single-character clients are unaffected.

`m_replicatedTierConsumer` is emplaced on **both** roles deliberately. A dedicated server never
reads the value it publishes there, but a **listen-server host does** — its local player's input
delay comes from that cache, and an unbound cache would answer `0` delay where the pre-arrival
no-tier fallback is correct. No tier ever reaches an authority world, because the server *writes*
the relay property and `OnRep` is a non-authority callback, so the authority cache stays at that
fallback for the whole session — which is exactly what the retired per-character path also
converged on.

### The preserved tier-0 quirk, and its two consequences

**The core never publishes tier 0 as a FIRST value** (`m_lastPublishedTier` is baselined at 0). A wire that never leaves tier 0 therefore
produces no publish, no relay actor and no call into `onConnectionTierReceived` at all: the
client sits on the pre-arrival no-tier fallback while the server parks at tier-0 delay. That
standing divergence is today's behaviour and is preserved deliberately — a later change *widened*
it as a side effect of the worst-tier ruling but did not introduce it and did not change its
cause. Changing the cause changes felt input lag and belongs with the floor work, not with a
transport migration.

**The second consequence of the same preservation** is the one that was a real defect. That
fabricated `oldTier = 0` used to reach `applyTierTransitionStall` as if it were the client's real
prior tier, requesting a spurious multi-tick prediction stall on the connection's **first** real
tier resolution, in the wrong direction for every `newTier` except `0`. It was fixed by giving
the stall decision `hadAnyTier` — whether the client had *already* received an authoritative tier
— as an explicit input rather than inferring "had a tier" from `oldTier`'s value, so a first-ever
resolution — `hadAnyTier == false` — requests zero stall regardless of which tier arrives. **The standing divergence above
is unchanged by that fix:** `hadAnyTier` only ever suppresses a stall.

`hadAnyTier` **must be captured before `applyReplicatedConnectionTier`**, because that call feeds
`m_replicatedTierConsumer`, whose `hasReceivedTier()` becomes true unconditionally as a side
effect. Reading it afterwards would make every call report "had a tier" — including the very
first one.

The decision itself lives in core, as `shouldStallForTierTransition`
(`Network/ConnectionTierTable.h`), a pure function with its own low-level test coverage — which
this UE-bound class has none of. `applyTierTransitionStall` is only the plumbing around it, and
non-positive results are dropped by the clock too, belt-and-braces.

### Replayed values never stall

`onConnectionTierReplayed` and `onRelayDelayFloorReplayed` apply and republish but **do not**
stall. For the tier, it is the first tier ever applied, so no tick was predicted against a
previous tier's delay, and its "previous" state is the pre-arrival no-tier fallback rather than a
tier at all. For the floor, the pull happens inside this manager's own `BeginPlay`, before the
first prediction tick.

A floor **rise** on the live channel is indistinguishable from an upward tier transition to the
client: the frontier must fall back by the difference, which the clock pays down as Stall ticks.

Both listeners **bind, then PULL**. Each property dirties only on change, so an `OnRep` that
fired before the bind would never be re-notified; the relay latches it and hands it over at the
pull. The timing relay is `bAlwaysRelevant`, so on a client it may legitimately already exist or
not — a missing relay simply means no floor has arrived, and the relay's own client `BeginPlay`
replays it if it turns up later.

### The zero input

`getZeroPlayerInput()` fills the `[0, effectiveDelay)` window at session start and after a hard
resync. **It is not `PlayerInput{}`** — `getZeroPlayerInput` builds `(0,0,1)` forward vectors —
so the injection is load-bearing rather than defensive.

It is set on the **authority** branch too, and that is not a precaution: a dedicated server reads
this value on every tick it substitutes an input for a remote character (the remote branch of
`collectInputAll` on a queue underrun) and seeds each character's replicated applied-input with it
at `registerAuthorityOwner` (the value itself lives in the resolution peer's `m_neutralInputs`). Deleting the call would make the authority simulate — and publish to
every peer — the zero-forward-vector input for the whole of every join window. `netSync` warns at
registration if the line ever stops running before it. **Ordering is load-bearing: it must precede
every `registerAuthorityOwner` call**, and registration happens per character, later.

### Inbound-hit routing

Cross-character inbound-hit routing runs **inside** `m_manager->onGameSimulation`, via
`brawlerHitRouting::System::postIntegrate`, fired by the systems executor after `integrateAll` on
every tick — including each resim replay tick — so the routed `HitFlinch` flags stay
deterministic. There is no adapter-side routing wrapper any more: the former `routeInboundHits()`
shim and its map were removed once the routing system owned the whole pass, and no reset-order
hazard remains because the single routing pass *is* the system's `postIntegrate`.

---

## §6 The relay ring

### The host boundary

`ASimulationInputRelay` lives in `OGSimulationUnreal`, which **must not depend on
`OGBrawlerUnreal`**, so it cannot name `AOGBrawlerUECharacter` or `USimmableUpdateComponent` and
cannot resolve its own owner to the component that consumes relayed input. This manager is the
bridge — and **only** the bridge: it holds no state for this channel and no per-arrival traffic
passes through it. Once the two objects are linked, ring `OnRep`s (`OnRep_RelayedInputRing`) go straight from the
host to the component's callback.

**No map, deliberately.** The resolution is host → `GetOwner()` → character → component: three
lookups on data that already exists, because `AActor::Owner` replicates and the authority spawns
the host with `SetOwner(character)`. A registry keyed by character would be a second structure to
keep in step with every spawn, death and travel, for nothing.

**Idempotent.** The host calls in from `BeginPlay`, from `OnRep_Owner`, and again from any ring
`OnRep` that arrives while still unlinked; `attachInputRelayHost` absorbs the repeats. Every hop
can legitimately fail during the join window, and every failure is a plain "not yet".

### The relay tap: stage, do not write

`relayRemoteInput` is the **outbound** half of the receipt path. Where `deliverRemoteInput` routes
an input *into* the simulation for the character that sent it, this stages the same
`(captureTick, dA, input)` for that character's replicated relay ring, which carries it to the
**other** clients so each peer simulates that character with its real input instead of
extrapolating. `dA` is the **schedule stamp** — the effective input delay the authority held for
that wire at receipt — so a peer derives the application tick as `captureTick + dA`.

Under bare-C1 flush-on-poll the arrival is **staged**, and the host actor's `PreReplication`
publishes the whole staged burst once per replication poll, so two arrivals in one server frame
both reach the wire instead of the second overwriting the first.

**No depth is read at that site any more, and that is the point.** It used to pass a
session-configurable retention depth into `writeLatest`. On the flush path that same value would
make every staged entry after the first supersede its predecessor, the ring would carry exactly
one entry per round, and bare C1 would silently become replace-latest again — with no compile
error and no warning. `stageRelayedInput` has no depth parameter; the capacity is
`relayedInputRing::kMaxDepth`, taken as a constant inside the codec. The machine-checked fence
that replaces the retired startup proof line lives in
`Source/OGSimulationTests/extern/og-simulation-tests/Source/OGSimulationTests/Network/RelayRedundancyDepthTest.cpp`.

The staging outcome is deliberately unchecked: `accepted` can only be false on the stale-write
arm, which the coordinator's monotonic `acceptedNew` gate makes unreachable from this call site,
and `droppedOldest` is already counted on the host.

**The dual-write fence is discharged.** The relay tap originally wrote only the ring, leaving
`SimulationNetSync::sendCorrectionAll`'s input write into `m_replicatedInputSyncedBuffer` running
beside it so no reader had to move and be moved back. The readers were then switched and that
second write — and the whole correction-input channel with it — was removed. This tap is now the
only path by which a character's input reaches other clients. The viz call site reads it through
`SimulationInputResolution::getLastRelayedInput`, whose nullopt contract was modelled on the
retired `editReconciliation().getLatestInput(...)` so `BrawlerVisualizationInputSource.h`'s
cold-source skip did not have to be re-specified.

> ⚠ **Which ring.** The replicated ring is `ASimulationInputRelay::m_relayedInputRing`, registered
> `DOREPLIFETIME_CONDITION(ASimulationInputRelay, m_relayedInputRing, COND_SkipOwner)`. It is
> **not** `USimmableUpdateComponent::m_detachedRelayRing`, which is the no-host fallback whose
> writes nothing replicates. See §11 — the header said otherwise until this pass.

---

## §7 The reception coordinator and the transport adapter

The whole server reception subsystem — the tier table, the delay queue, the claim map, and the
orchestration over them — lives in the engine-agnostic core `ServerReceptionCoordinator`. This
manager owns **one** instance of it and has shrunk to a thin transport adapter: it acquires engine
primitives (address, player slot, RTT, sim tick, wire decode) and forwards them in. **Authority
role only** — `std::nullopt` on a pure client, because the coordinator borrows `const TimeConfig&`
from `m_manager`'s owned config.

**Reception *policy* is in the core; primitive *acquisition* is at the RPC boundary**
(`USimmableUpdateComponent::ServerReceiveRemoteMove`), where the bundle and the owning actor
naturally live. Three accessors are all this manager still supplies — the coordinator instance,
the server sim tick, and the id→component delivery-routing registration — and **none carries
netcode policy.**

`sampleAndDeriveConnectionTier` and `tryEnqueueDelayedRemoteInput` are **gone**; their
engine-primitive acquisition moved up to that RPC boundary, which now forwards straight into
`ServerReceptionCoordinator::noteRttSample` (once per bundle) and
`ServerReceptionCoordinator::receiveInputBundle` (the whole per-slot loop). Nothing of
the RPC per-slot path remains manager-side.

### The two sinks

This manager satisfies both `RemoteInputDeliverySink` and `RemoteInputRelaySink`. Both are
compile-checked at the `receiveInputBundle` call site, and both are asserted again beside their
definitions in the `.cpp` so a breaking signature change surfaces legibly.

`deliverRemoteInput` is the **one** delivery method that both the per-id drain (built in
`releaseDelayedInputsForStep`) and the core receive-loop fallback (a malformed slot in
`receiveInputBundle`) route through. It resolves id → component through
`m_delayedInputComponentsById` and hands the input to the same inbound path the RPC uses
(`deliverDelayedRemoteInput` -> `m_onRemoteMoveReceivedCallback`), with the **original**
`captureTick`. A stale weak handle means the owner was garbage-collected without an
unregister: the map entry is dropped and the input discarded, because a dead component has nothing
to receive it.

`m_delayedInputComponentsById` exists because the core claim map is **id-keyed** — it cannot hold
a `TWeakObjectPtr` — so the coordinator's `deliver` callback hands back an id and this map resolves
it. It is populated by `noteDelayedInputComponent` from the RPC adapter
(`USimmableUpdateComponent::tryRegisterWithNewFramework`, which has both the id and the
component), pruned in the deliver callback when a weak handle goes stale, and erased in
`unregisterFromNewFramework`. `id == component GetUniqueID()`.

The per-id `deliver` callback answers "is this owner still alive", so the coordinator can drop a
stale claim — mirroring the retired drain's own `target.Get()==nullptr` prune, and routes a live delivery through `deliverRemoteInput`. **The liveness check stays
on the adapter side** because the drain's prune contract is a `bool` return while the sink method
itself returns `void` per the concept.

### The drain and the reap

`releaseDelayedInputsForStep` supplies only the game-thread-safe upcoming sim tick (§9) and the
per-id callback, then calls `ServerReceptionCoordinator::releaseDelayedInputs` and `reapConnections`. The reap was relocated
here off the former arrival-gated RTT sample path: it now runs once per physics frame regardless
of traffic — a documented benign cadence change, since an idle server now reaps — and the
coordinator gates it on the dwell boundary internally. Its tick comes from the mapper, **not** the
physics-thread-written server clock.

The coordinator's early return guards **only** the drain, not PROBE A below it, because that probe
runs on both roles and a pure client never has a coordinator.

---

## §8 The four probes

All four are purely diagnostic: nothing reads them but the log lines they feed. **Volume
convention: per-window summaries at `Warning`, per-event detail at `Verbose`, and nothing
per-tick or per-write at any verbosity.**

### PROBE A — frame health (`m_frameHealthProbe`), both roles

Sim ticks per game-thread frame.

**What it settled on the server.** Clients measure a relay-ring arrival gap of about two capture
ticks where the design expects about one. Property replication runs once per server game-thread
frame and `NetServerMaxTickRate` is a **cap** on that, not a floor — so a 60 Hz sim on a 30 fps
server advances two sim ticks per replication, and a depth-1 replace-latest ring could only carry
the newer one. If this ratio equals the clients' measured gap, the gap is a **host performance
artefact** rather than a netcode defect.

**Why the client needed it too.** `[RelayProbe.Frame]` was, until it gained a client role, the
only wall-clock timing instrument anywhere in the netcode surface, and it was server-only by
construction — yet the server never resims. Every client cost figure backing the shipped
`ResimTriggerPolicy` was derived from `ResimGateProbe` window cadence, which sees only the physics
tick and is blind to game-thread or render hitching, which is precisely where a resim-driven cost
would show up.

**Why it lives on this class and not in `SimulationNetSync` with the other three probes:** it is
measured on this actor's game thread, from the Chaos pre-step hook, and the only tick source legal
to read there is the `ChaosTickMapper`'s atomic offset — which this actor owns and
`SimulationNetSync` has no access to.

**Why this hook and not `OnPostPhysicsStep`, on either role:** that hook is game-thread on both
roles too, but it is handed only an `FChaosScene`. It has no physics step number and therefore no
route to a sim tick through the only source safe on that thread. Sampling there would have
required either reading the physics-thread-written clock or inventing a second counter, both ruled
out. This hook already has both numbers: the step, and Chaos's own sub-step count.

**Why reusing `firstUpcomingSimTick` is safe on the client** even though the `+1` derivation in §9
is proved under an authority-only assumption: the probe never consumes the value directly, only
**deltas** between consecutive samples, and a constant additive skew cancels under subtraction. A
client-side departure from unconditional advance — a resim replay, a stall — surfaces either as an
ordinary sample, if the tick delta stays plausible, or as a counted discontinuity
(`kFrameHealthDiscontinuityTicks`) if it does not. Neither corrupts the measurement. What *would*
be unsafe is a **different** tick source, such as reading the prediction clock off the physics
thread; this reuses the identical atomic read the server call site already relied on, so no new
thread-safety question is introduced.

**The ratio is hook-independent**, which is why the metric is defined against `GFrameCounter`
rather than an invocation count, and it is why the probe gained a second role without a second
implementation. It additionally reports how its *own* invocations distributed over frames — once
per frame, more than once, or less often — so this hook's cadence is measured and reported rather
than assumed. On a resimming client that sub-step cross-check is the whole point:
`numStepsAboveOne > 0` reads as "resim or sub-stepping ran"; `== 0` with a ratio above 1 reads as
"the game thread simply hitched".

**Category decided per role, not inherited.** The server line keeps `[RelayProbe.Frame]` for the
original reason (it pairs with `[RelayProbe.Arrival]`). That reason does not transfer to the
client, which never emits `[RelayProbe.Arrival]` against its own frame rate; on the client the
natural pairing is frame health against **resim cost**, so the client line rides
`[ResimProbe.Frame]` and inherits routing for free from the `[ResimProbe` catch-all. The two tags
are deliberately different families rather than one tag with a role suffix — a prior review
mis-assigned which process emitted `[RelayProbe.Frame]` and had to re-derive roles from other
receipts. Both lines additionally carry an explicit `role=` field, belt and braces.

`runsPrediction()` is this codebase's established role check, and `false` covers **both** a
dedicated server and a standalone or listen-server-host manager — so a standalone session's
frame-health line rides the server tag, matching how its tick clock is already treated everywhere
else in this file.

**Window comparability with `ResimGateProbe`:** `kFrameHealthProbeWindowSamples` and
`kResimGateProbeWindowSamples` are both 120 samples, so a reader can line one `[ResimProbe.Frame]`
window up against the surrounding `[ResimProbe.Gate]` windows without a resim-tick count on this
line. They are **comparable, not identical**: this window closes on `noteFrame`'s `GFrameCounter`
cadence, that one on `noteCheck` — one call per `checkDivergenceAll` — which is a logically different event. The two drift apart over a
session exactly as far as their emitters' cadences do, which is itself diagnostic.

### PROBE 5 — relay writes per frame (`m_relayWriteProbe`), server only

**What it settles.** Replication polls a replicated property once per server game-thread frame and
compares the live value against its shadow. **When this probe was built** the ring shipped at depth
1 — replace-latest — so a second write inside the same frame overwrote the first in server memory,
and no client and no client-side probe could tell that apart from a send-path drop: both surface as
`[RelayProbe.Arrival] gapCaptureTicks > 1`. Flush-on-poll has since removed that mechanism (§6);
the probe stayed because the **quantity** — writes per frame — is still the one nobody had
measured.

The relay-loss elimination chain does not cover this. It eliminated the server's own write with
"writes on every accepted receipt, and receipts are complete", which is true and is about the wire
*into* the server; and the server frame rate with "about one sim tick per frame", which is true and
about a different clock — these writes are paced by **packet arrival**, not by the sim.

**`GFrameCounter`, not an invocation count.** The frame is the unit replication polls on, so the
engine's own frame identity is the only correct key; a local counter incremented at the tap would
measure the tap's call rate instead.

**Three fractions, never collapsed.** `receivedX1000` is upstream completeness (only input
redundancy raises it). `observableX1000` is the coalescing ceiling (depth raises exactly this one),
and it reports the **flush** ceiling since the write path changed:
`min(writesThisFrame, kMaxDepth)` rather than 1, which is why the stage capacity is *injected* into
the probe from the codec constant rather than assumed. `deliverableX1000` is their product and is
the only one comparable to the client's arrival rate. A single merged "loss" number would decide
the remedy on merged evidence. `replaceLatestObservableX1000` recomputes the same window the way
the retired write path imposed it, so archived windows stay directly comparable and the improvement
is two numbers on one line rather than a claim.

Line 2 carries the run-length shape plus the capture-tick range, so a server window can be aligned
against a client window: owner ids are per-**process** and do not match across logs, but capture
ticks do.

**Volume: two `Warning` lines per 120 writing frames per relayed character.** Nothing per-write is
emitted at any verbosity, deliberately — a per-write line is a per-tick line.

### PROBE 6 — per-connection send budget (`m_connectionBudgetProbe`), server only

**Why arithmetic was not enough.** The budget model is
`allowance = CurrentNetSpeed / DesiredTickRate` bytes per tick, with up to two ticks of bankable
credit. Both halves are read from engine source, but **every term in it is derived**:
`CurrentNetSpeed` is what the server clamped the client's request to at runtime, on a path nobody
here has watched execute, and the modelled round counts two properties out of an unknown total.
This probe measures all of them: the negotiated net speed, the real bytes and packets on the wire,
the ack-derived outgoing loss, and `notReady` — frames on which `QueuedBits + SendBuffer > 0`,
which is exactly the state in which `UDataStreamChannel::Tick` returns having written
**nothing**; `IsNetReady()` is the same question asked of the connection.

**Read before `TickFlush`, which is the right place.** `QueuedBits` is updated at the *end* of
`UNetConnection::Tick`, so the value sampled here is the credit this frame's replication write will
actually be judged against.

**Cumulative counters, not `OutBytes`/`OutPackets`:** the latter are `StatPeriod` accumulators the
engine zeroes on its own schedule, so differencing them across our window would silently drop
whatever it reset mid-window.

The allowance denominator is `NetServerMaxTickRate`, what `GameEngine::GetMaxTickRate` clamps a
dedicated server to and therefore what `DesiredTickRate` resolves to when `MaxNetTickRate`
(`BaseEngine.ini`) does not bind. It is read **from the net driver** rather than hardcoded, so a config change
cannot silently invalidate the occupancy figure. `netSpeedBps` is printed rather than assumed,
because if it is not the configured ceiling the whole derivation is wrong. `QueuedBits` is a debt
counter, so `min` is the *most* headroom and `max` the closest to saturation; `lost` is
ack-derived, i.e. the emulation's actual outgoing loss rather than a configured percentage.

**The capacity pin.** Every byte table in this work budgets against a **derived** usable
single-bunch capacity computed from `UNetConnection::GetMaxSingleBunchSizeBits()`, and an earlier
published figure for the same quantity does not reproduce from the formula. Budgeting against an
unverified derived constant is precisely how this tree's inert-ini defect happened, so the running
engine gets to be the referee — once, cheaply, from the real connection.
`MaxPacketHandlerBits` is the term that can move it, and it can only move it **down** (encryption
or any registered packet handler reserves bits out of the same budget), so the literal pinned in
`Source/OGBrawlerTests/extern/og-brawler-tests/Source/OGBrawlerTests/RoundVsPacketBudgetTest.cpp` is an **upper bound**: if the value logged here
is ever smaller than that literal, the literal is optimistic and must be lowered there. The
derivation is `GetMaxSingleBunchSizeBits()` word-rounded by `UDataStreamChannel::WriteData`; the
line reports `occupancyPctX10=340` for 34.0 % occupancy. One shot
per session, on the first connection, at `Warning` — the value is a property of the build and the
handler stack, not of the connection.

### The resim-gate feeds (`noteResimRequest` / `noteResimGrant`), client only

**Why our own counters at all.** `FRewindData::FindValidResimFrame` and
`FPBDRigidsSolver::ConditionalApplyRewind_Internal` log every refusal and every silent frame-skip
behind `DEBUG_REWIND_DATA` / `DEBUG_NETWORK_PHYSICS`, which are compiled out of any normal build. A
refused rewind is therefore **completely silent** today, and our side simply retries next frame
because nothing cleared `needsResimulation()`. These two calls are the only way that second gate
becomes countable, and `requests − grants` is the refusal count. (The retry property is now
structural rather than incidental: only the resim-completion edge can consume a pending anchor, so
a refusal cannot clear the gate even by accident. The counters' meaning is unchanged.)

**The request is counted after the tick conversion**, so the Chaos frame recorded is exactly the
one Chaos receives and `clampedGrants` compares a grant against the number actually asked for. The
per-event `Verbose` line carries the mapper **offset**, which is the discriminator: the offset
legitimately moves ±1 across Stall/Skip steps, and a ±1 skew at trigger time converts directly into
a request landing on a refused frame or a replay one frame short of the clock's catch-up need.

**The grant** is recorded where Chaos starts the rewind, at `PhysicsStep`, which can differ from
the frame we asked for **only by being deeper**: an engine-side requester merged in via a `Min`,
or the validation walking down (`FMath::Min` on the engine side). A *shallower* clamp is structurally impossible on this wiring —
validation walks downward, the merge can only deepen, and the replay-loop push-data skip that could
start a replay late is dead code on this engine. So `clampedGrants` reads a constant `0` by
construction and a nonzero value is an **engine-behaviour-change alarm**. The engine may also
rewind on its own initiative with no request of ours on record; that stays uncharged. See `ResimGateProbe::noteGrant`.

`noteResimGrant` is called **before** `prepareResimulation`, so a grant is recorded even if
anything below early-returns: `grants` and `prepares` are counted on opposite sides of that
boundary on purpose, and their agreement is the wiring check.

**The startup proof line** (`[ResimProbe.Session]`) is the fourth step of that family's
category path — declaration, router branch, ini block, then this — and the one that makes the
other three checkable from a log instead of from the source. It reports the **effective runtime
verbosity**, not a constant: `verboseDetail` says whether the per-event half will emit at all,
which is the single most likely thing to be silently off when somebody goes looking, and
`windowSamples` states the denominator every per-window line is divided by, taken from the constant
rather than retyped. **Its own absence is also information:** no `[ResimProbe.Session]` line in a
client log means either the category was set to `NoLogging` or the branch never ran, and both are
worth knowing before reading a zero off any counter.

---

## §9 Tick alignment — the `+ 1`

**An off-by-one here shifts every player's input by one tick, silently and uniformly.**

`physicsStep` is the **upcoming** solver step: Chaos passes
`MarshallingManager.GetInternalStep_External()`, documented in `ChaosMarshallingManager.h` as *"the internal step that the current PushData will
be associated with once it is marshalled over"*, and the solver's frame counter is
only incremented at the **end** of a tick (`GetCurrentFrame()++`, `PBDRigidsSolver.cpp`), so step `N`'s `OnPreSimulate_Internal` observes
`GetCurrentFrame() == N`.

`ChaosTickMapper`'s offset is written in `OnPreSimulate_Internal` as `chaosTick - simulationTick`.
Crucially it is written **before** `onGameSimulation()` runs, and `onGameSimulationAuthority()`
advances the server clock as its first action. So the `simulationTick` captured at step `K` is the
tick simulated at step `K−1`:

```
offset = K - S(K-1)          where S(K) is the sim tick simulated at step K
S(K) = S(K-1) + 1            authority advanceTick() is unconditional -
                             no Stall, Skip or resim exists on the server
=>  offset = K - (S(K) - 1) = (K - S(K)) + 1
=>  toSimulationTick(X) = X - offset = S(X) - 1
```

`toSimulationTick(physicsStep)` therefore names the tick **before** the one that step will
simulate, and the upcoming tick is that value **plus one**. The `+ 1` in
`releaseDelayedInputsForStep` is a derived correction, not a fudge factor.

**Why not cross-check against the server clock at runtime:** the clock is written on the physics
thread, and reading it from the game thread is exactly the unsynchronized cross-thread read this
whole design exists to avoid. The mapper's offset is a `std::atomic` and is the only tick source
safe to read from the game thread, which is why the resolution specifies it.

**Sub-stepping:** with `NumSteps > 1` the physics frame runs several sim ticks back to back, so the
drain releases input for each of them. The normal fixed-tick case is `NumSteps == 1` and collapses
to a single drain.

The Chaos hook `OnPhysicsPreTick` → `InjectInputs_External` → `releaseDelayedInputsForStep` is
game-thread and immediately precedes the step. The drain is a no-op on a client and on a server
with nothing parked; the frame-health probe inside the same function is **not** a no-op on either
role.

---

## §10 Registration and unregistration

`tryRegister` is a two-phase contract. The first call creates bodies; later calls re-test
resolvability and, once every body resolves, perform the registration and return `Ready`.

**The capsule body id.** `setCharacterBindings` stamps the authoritative capsule `BodyId` into the
brawler's `CharacterBindings`. **Source today:** the engine character's capsule component body,
created and owned and moved by the character-movement component — this site only learns its
`BodyId`. When the planned character-movement sub-sim lands, modelled on radial and guard with its
own `PhysicsDeclaration`, the capsule body will be created and registered through the factory pass
instead, and `capsuleBodyId` will come from that sub-sim's `bindings.ownBodyId`. See
`BrawlerMovementSimulation.h`'s `CharacterBindings` comment for the full future-direction note.

**Attach parent and parent body are the same capsule** under the one-deep hierarchy, so passing one
handle expresses "these shapes belong to this character". They used to be passed separately, which
let callers get them out of sync; the factory derives the parent `BodyId` from the attach parent's
body-instance handle internally now, and that factory-owned parent id becomes the root for every
shape it registers — so `overlap()` emits an actor-level `rootBodyId` equal to the capsule id.

**On the authority path**, registration also notifies the systems executor that the character is
now *in storage*, so `brawlerHitRouting::System::onCharacterRegistered` indexes it for inbound-hit
routing and reads `capsuleBodyId` itself. Wiring that notify and dropping the old adapter-owned
`m_byRootBodyId` insert landed in the **same** change, so there was never a window in which both
the adapter map and the system map were populated and an inbound-hit stream could be double-routed.

**Unregistration is a contract, not a garbage-collection hope.** `unregisterFromNewFramework`:

1. notifies the systems executor to drop the routing entry **before** `unregisterSimulatable`
   destroys the `SimulatableBrawler`, while the character is still in storage — so the hook
   resolves it through the view and erases by stored-pointer identity, and the per-tick routing
   pass never dereferences a dangling pointer. The `has<>` guard is preserved: a character never
   fully registered into storage has no system-map entry to drop, and the hook's `view.get<>(id)`
   would be unsafe;
2. drops this owner's claim and dedup watermark from the reception coordinator and its
   id→component mapping — **promptly**, rather than waiting for GC to make an engine handle stale.
   This is what replaced the core's former GC-liveness read, since the core claim map is id-keyed
   and cannot hold an engine weak pointer. No-op on a pure client;
3. drops the write probe's per-owner state, so its map stays bounded by live ids exactly as the
   coordinator's does. A half-open run belonging to a dead owner is discarded rather than reported,
   which is correct: its length is unknowable;
4. erases the id from `m_authorityRegisteredIds`.

### The pre-diet character cap

`kPreDietCharacterCap = 4`, checked at character registration on the authority — a path that
provably runs every session, once per character. **A cap that depends on nobody spawning a fifth
character is not a fence**, and this tree has already shipped three things that were silently inert.

**Why 4, derived.** The binding constraint is the **input guarantee**: all the remote characters'
relay rings must fit one packet by themselves — pre-diet, `81*SumE + 13.1*(N-1) <= 943 B`, because a ring that gets scheduled out under
redundancy 0 loses its whole staged burst with no recovery path. Modelling a join as "the joiner's
ring at the measured settling burst, everyone else at the measured average", N = 4 clears that
bound with about nine tenths of one entry to spare and N = 5 does **not** — at five characters an
*ordinary* join crosses the bound, with no server hitch required. The same arithmetic is asserted
from inside the suite by `Source/OGBrawlerTests/extern/og-brawler-tests/Source/OGBrawlerTests/RoundVsPacketBudgetTest.cpp`'s pre-diet table. The
other half of the pre-diet configuration is `TimeConfig::correctionRotationK` (§3).

**Emission is once per over-cap character**, not per frame and not per session: registration runs
exactly once per character, so the emission site is its own throttle. No memoization is needed and
none is used — a per-session latch would report the fifth character and stay silent about the
sixth. It is a `Warning` rather than a `Log` because a `Log` line does not exist on a dedicated
server, and it is not an `ensure`/`check` because an over-cap session still **runs**: it runs with
input-loss margins the design has not underwritten, which is a thing an operator must be told, not
a thing that should take the server down mid-brawl.

**The denominator is a `std::set`, not a counter**, and the reason is that the two ends are not
symmetric: `tryRegister` increments only on the `Ready` path, while `unregisterFromNewFramework`
runs for any component ending play including one abandoned mid-`Pending`, so a bare counter would
silently drift downwards and disarm the cap. Reaping on unregister means a session that churns
characters is judged on the roster actually resident rather than on a high-water mark, and erasing
an id that never completed registration is a no-op — which is exactly why a set is the right shape.

**Both the constant and its check are deleted by the wire diet**, and their **absence** afterwards
is the "cap lifted" statement: there is no flag to flip and no value to raise, which is deliberate,
because a cap you can quietly widen is not a cap.

---

## §11 Corrections — claims these files carried that were not true

This pass verified every claim before compressing it. Four did not survive, and are recorded here
rather than quietly repaired, because an archive that repairs itself stops being a record.

### C1 — `correctionRotationK`'s compiled default was stated, and the value had moved

`SimulationManagerUImpl.cpp`'s rotation-width intake block said:

> *"ABSENT => the compiled default (2 — every-frame at two characters, which is what keeps the
> archived two-character baselines comparable across this change)."*

`TimeConfig::correctionRotationK` is **1**. Three independent sources agree: the field's own
declaration; `Config/DefaultEngine.ini`, whose `[OGNetcode]` block states *"THE COMPILED DEFAULT IS
NOW 1"* and leaves `CorrectionRotationK` commented out so the compiled default ships; and
`Source/OGSimulationTests/extern/og-simulation-tests/Source/OGSimulationTests/PCTimeManagement/TimeConfigDefaultsTest.cpp`, which records that the value
will be *restored to 2* by the wire diet.

**The pair contradicted itself**: `SimulationManagerUImpl.h`'s pre-diet-cap block already said
`correctionRotationK = 1`, correctly, a few hundred lines away.

⛔ **A second paragraph in the same block was void as a consequence.** Its sentinel-collision note
argued that `clampK(-1)` would be `1`, *"which is NOT the compiled default — so a `-1` written on
purpose silently takes the compiled default 2 instead of clamping to 1"*. At the real default the
two are the same number and the difference it describes does not exist.

**Fix:** the block now points at `TimeConfig::correctionRotationK` and does not restate the value.
The sentinel note keeps the true half — that `-1` cannot be told from an absent key — and drops the
arithmetic that depended on the wrong default.

### C2 — the `RelayedInputRingCodec.h` include justification named a call that does not exist

The include comment said the header was named directly because *"relayedInputRing::clampDepth — the
shared depth guard **the intake below calls** before it logs the effective depth"*.

`clampDepth` occurs in that translation unit **only inside that comment**. The intake it refers to
was retired together with the ring-depth ini key — a fact stated by the ⛔ RETIRED fence about
five hundred lines further down the same file. The justification survived the retirement it sits
above.

**Fix:** the include comment now names `relayedInputRing::kMaxDepth`, which the pair really does
use (`SimulationManagerUImpl.h` initialises `m_relayWriteProbe` with it), and states that no
`clampDepth` call remains.

### C3 — the relay ring's owner and its replication condition were both wrong

`SimulationManagerUImpl.h`'s `relayRemoteInput` comment said this writes

> *"into that character's replicated relay ring (`FRelayedInputRing` on
> `USimmableUpdateComponent`, `DOREPLIFETIME` with **NO** `COND_`)"*.

Both halves are false in the current tree:

- the replicated ring is **`ASimulationInputRelay::m_relayedInputRing`**. `USimmableUpdateComponent`
  holds `m_detachedRelayRing`, whose own comment says it is the *no-host fallback* and that a write
  landing there *"would be invisible to every client"*;
- the registration is **`DOREPLIFETIME_CONDITION(ASimulationInputRelay, m_relayedInputRing,
  COND_SkipOwner)`**. `SimmableUpdateComponent.cpp` states in its own words that the old
  `DOREPLIFETIME(USimmableUpdateComponent, m_relayedInputRing)` "stood on" the component and was
  replaced.

A third clause was stale in a third way: it said this *writes* the ring, where the write path has
since become **staging** and the `.cpp` at the same call site says so explicitly.

⛔ This is the fourth site of a relay-ring mis-attribution class that two earlier reviews of this
initiative found and routed as unowned. **Grep every symbol before carrying it into a comment**:
this one would have been carried into a compressed guard with a checker's blessing, because every
symbol it names exists.

### C4 — a probe banner described a retired mechanism in the present tense

PROBE 5's banner said *"This ring ships at depth 1, i.e. replace-latest. So a second write inside
the same frame OVERWRITES the first in server memory"*. The ring's stage capacity is
`relayedInputRing::kMaxDepth`, which is **8**, and the write path is flush-on-poll — as the
paragraph eight lines below the banner already said, in the same file.

**Fix:** the sentence is now closed-tense — *"when this probe was built"* — and states that the
mechanism was removed while the quantity remains unmeasured, which is why the probe stayed.

### Routed, not fixed — an observation about a neighbouring file

A wave-6 sweep of the engine-free core retired the token `TestYo` "to 0" on the finding that *"the
TestYo layer exists nowhere in this tree"*. It does exist: `Config/DefaultEngine.ini`'s
`[/Script/Engine.Engine]` block carries six live `ActiveGameNameRedirects` /
`ActiveClassRedirects` entries naming `TestYo`, redirecting it to `/Script/OGBrawlerUnreal`. It is
this module's **original project name**. The two files that use the phrase are in the
`og-simulation` tree and are not owned by this pass; the finding is recorded here so the next
editor of either has the evidence.

---

## Provenance

The two source files carried **317 workspace-only citations** — backlog item numbers, task
numbers, review-note numbers, initiative document names and design section marks — before this
pass. They are meaningless to a reader who does not have that workspace, so they were removed from
the source and their content folded into the sections above. **The facts they carried are here; the
item numbers are not**, deliberately: an item number is provenance, not a guard, and this document
is where provenance belongs. The initiative workspace remains the record of *when* each decision
was taken.
