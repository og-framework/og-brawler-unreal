<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- ================= DECLARED LINT ESCAPES ==================================
     Every token below is CORRECT and cannot resolve. ========================= -->
<!-- lint-external-ref: UWorld::OnPostTickFlush -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: AddWeakLambda -- Unreal Engine delegate method, outside every scan root -->
<!-- lint-external-ref: AddUObject -- Unreal Engine delegate method, outside every scan root -->
<!-- lint-external-ref: FChaosScene::GetNetworkDeltaTimeScale -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: FChaosScene -- Unreal Engine type, outside every scan root -->
<!-- lint-external-ref: FPlatformTime::Seconds -- Unreal Engine function, outside every scan root -->
<!-- lint-external-ref: SendTo -- Unreal Engine socket method, outside every scan root -->
<!-- lint-external-ref: TickFlush -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: UE_LOG -- Unreal Engine macro, outside every scan root -->
<!-- lint-external-ref: PublicDefinitions -- Unreal Build Tool module-rules list, outside every scan root -->
<!-- lint-external-ref: net.IpConnectionUseSendTasks=1 -- Unreal Engine console variable and value, outside every scan root -->
<!-- lint-external-ref: TWeakObjectPtr::Get -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: MaxNetTickRate -- Unreal Engine net-driver setting, outside every scan root -->
# `LatencyBudgetDiagnosticsUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/LatencyBudgetDiagnosticsUImpl.h` and
`Source/OGBrawlerUnreal/LatencyBudgetDiagnosticsUImpl.cpp` (og-tickdriver task 18). Both source files carry no prose: a
two-line docs pointer, code, assertion messages, and one-line tags `⛔G-nn` → an entry in
`LatencyBudgetDiagnosticsUImpl-guards.md`.

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes.

* **§1** says what the class is and why it is diagnostics.
* **§2** is the gate, `OG_LATENCY_BUDGET_DIAGNOSTICS`, and what a build with it at 0 contains.
* **§3** is how the class reads production state, and the compile-time check on it.
* **§4** is the fence test, the machine check that the diagnostics feed no production output.
* **§5** maps the manager's former latency members and functions to this class.
* **§6** is the host side of the probe: stamp sites, threads and measurement errors. It was
  `SimulationManagerUImpl-rationale.md` §16 until this task moved it here with the code.

---

## §1 What the class is

`ASimulationManagerUImpl` ran the latency-budget probe in every build, Shipping included: it created the probe at
`BeginPlay`, wrapped every client input provider, walked every character on every step to post mailbox events, drained
the mailbox every frame and logged at `Warning` every 10 s. The probe's only output is its log lines, `[LatencyBudget]`,
`[LatencyBudget.Window]` and, through the Chaos host, `[ChaosDilation]`, and nothing reads them back. Deleting it and every
caller changes no production behaviour and no shipped telemetry, so it is diagnostics by og-simulation's own test
(`DiagnosticsConventions.md` §2: "who calls it, and what breaks if it is deleted").

That test puts it under the codebase's three structural markers (the user's ruling of 2026-08-29):

1. **A grouped view.** One class holds the whole feature, and its type is the marker. The user chose this shape for
   task 18 ("Group it", 2026-10-19). So, as `DiagnosticsConventions.md` §1 requires of a view, **no member carries a
   `Diagnostic` prefix or infix**: `m_probe`, not a prefixed name. The manager holds one member, <!-- lint-anchor-ignore: `Diagnostic` here is a naming INFIX under discussion, not a type this file claims exists -->
   `m_latencyBudgetDiagnostics`, and exposes it through the `get`/`edit` pair of §1:
   `ASimulationManagerUImpl::getLatencyBudgetDiagnostics()` (const) and
   `ASimulationManagerUImpl::editLatencyBudgetDiagnostics()`. Every production call site reads
   `editLatencyBudgetDiagnostics().x(...)`, so the channel is named at the call, not the fact (`DiagnosticsConventions.md`
   §4): no production local, decision or replicated value moved into the class.
2. **A fenced block.** Every data member sits in one block, guarded as `G-01`, which exists only while the gate is 1
   (§2). The two neighbours that feed the class have their own fenced blocks: the Chaos host's `[ChaosDilation]` window
   (`ChaosTickBackendUImpl-guards.md` G-11) and the input collection component's input-sample time.
3. **A fence test** (§4).

**The verbs.** `stamp`, `post_Internal`, `stampAfterStep_Internal` and `stampStepEnd_Internal` write probe stamps;
`noteWireSample` writes a probe sample (`note*`, `DiagnosticsConventions.md` §3); `wrapInputProvider` adds the capture stamp;
`onPostTickFlush` is the `H3` end; `tick_GameThread` drains, stamps `H3c` and `H7`, and closes the window. `begin` creates
the probe and its two mailboxes and binds `onPostTickFlush`; `end` unbinds it; `forgetCharacter` drops a departed
character's lane. `withInstance` runs a function on the role's instance, if there is one, and is how the component's
game-thread sites reach the class (§2). `clockCountsInWindow` and `restartClockWindow` are the Chaos host's reads and
writes of the clock counts.

**Threads.** Unchanged from the manager's functions they replace (§6, *Threads*): `stamp`, `noteWireSample`,
`onPostTickFlush`, `tick_GameThread`, `begin`, `end`, `forgetCharacter`, `withInstance`, `clockCountsInWindow` and
`restartClockWindow` are game-thread; `post_Internal`, `stampAfterStep_Internal`, `stampStepEnd_Internal` and the wrapped
provider run on the step's thread.

---

## §2 The gate, `OG_LATENCY_BUDGET_DIAGNOSTICS`

**One macro,** defined in the class's header as `#ifndef OG_LATENCY_BUDGET_DIAGNOSTICS` / `#define
OG_LATENCY_BUDGET_DIAGNOSTICS (!UE_BUILD_SHIPPING)`. That is exactly the user's ruling, "gate it behind
`#if !UE_BUILD_SHIPPING`" (2026-10-19), named once so that a Development build can force it to 0 through a
`PublicDefinitions` entry for the fence test (§4).

**At 1** (every non-Shipping build) the class is the feature, and the Development output is byte-identical to the
manager's: the same log categories (`LogOGLatencyBudget`, `LogOGChaosDilation`, now declared in this header and defined in
its `.cpp`), the same tags, line text and field names, the same stamp points, threads and order.

**At 0** (Shipping, and the fence build):

* **The class is empty.** Its members are inline no-ops with the same signatures, so **no call site carries an
  `#if`**. The two log categories are not declared, so any code that logs to them outside a gate fails to compile in
  that build: that is the compile-time half of the fence.
* **`wrapInputProvider` returns the provider unchanged.** Provider presence is the core's local-versus-remote identity
  test (`SimulationManagerUImpl-guards.md` G-01's alias carries the provider). The public `wrapInputProvider` is
  written once, above the gate, and `checkf`s in both states that a present provider stays present and an empty one
  empty; only the private `wrap` differs per state. Before task 18 the `checkf` covered the wrapping path only.
* **`withInstance` does nothing,** so the component's per-site manager lookups (`ASimulationManagerUImpl::instanceFor`)
  that existed only to stamp are not made. The function it would run is still compiled at 1 and 0 alike, which keeps
  every call site's code checked in both states, and is discarded at 0.
* **`begin` binds nothing,** so `UWorld::OnPostTickFlush` has no latency subscriber.
* **`tick_GameThread` does nothing,** so neither backend host's diagnostics view is called.
* The Chaos host's `[ChaosDilation]` window members and the input collection component's input-sample time do not
  exist, and their views are no-ops.

**What still runs at 0, deliberately.** The Chaos async callback still reads `FPlatformTime::Seconds` before
`onGameSimulation`: the step-start time must be taken before the step, and it is a production local beside production
statements, so it stayed. The authority's relay-host flush callback is still bound (`attachInputRelayHost`), but its body
is a single `withInstance` call, which is a no-op at 0.

---

## §3 Reading production state

The class reads the manager's `m_manager`, `m_storage`, `m_inputResolution` and `m_delayedInputComponentsById`, and the
client clock through `m_manager`. It reaches them through **one const reference**, `m_owner`, to its owner: the manager
declares the class a friend (`friend class LatencyBudgetDiagnosticsUImpl;`), and every read goes through that const
reference, so it can call only const members (`currentIntegratedTick`, `runsPrediction`, `getClientClock`,
`forEachSimulatable`'s const overload, `getLastUsedCaptureTick`, `getDiagnostics`).

⭐ **Machine-checked.** A `static_assert` beside `m_owner` requires its referenced type to be `const`. A diagnostics class
with a mutable reference to its owner, which befriends it, would be a write door into every private member of the
manager; the assertion makes that a compile error. Task 18 ran the POISON (a non-const `m_owner`) and saw it fail
(the impl notes quote it).

**Why the manager's member initializer is `{ std::as_const(*this) }`.** With a plain `{ *this }` (an implicit conversion
of `*this` to the const reference the constructor takes), MSVC (the toolchain of 2026-10) compiled every **later**
default member initializer of `ASimulationManagerUImpl` that names `*this` or a sibling as though `this` were const:
`m_reconciliation{ m_storage }`, `m_inputResolution`, `m_netSync` and `m_backend` failed with C2665, "Conversion loses
qualifiers", in every translation unit that includes the header. The explicit `std::as_const` compiles. A revert to
`{ *this }` is therefore a compile error, not a silent change.

**The component reads are const too** (lead ruling L1, og-tickdriver task 18). The server's `H3c` start reads each
character's correction-buffer tick through the `const` overload of
`USimmableUpdateComponent::getSyncedCorrectionStateBuffer`, added for this purpose: the class binds the pointer
`TWeakObjectPtr::Get` returns from the const map to a `const USimmableUpdateComponent*`, so only the const overload is
callable, and deleting that overload is a compile error in this class. Every path from the class into production state
is therefore const: the owner reference (asserted) and, through it, const member functions and const pointers only.

---

## §4 The fence test

The user's ruling of 2026-08-29 requires the fence to be machine-checked. For this class the check is a build and two
harness sessions:

1. Add a `PublicDefinitions` entry setting `OG_LATENCY_BUDGET_DIAGNOSTICS` to 0 in `OGBrawlerUnreal.Build.cs` (a copy
   first; restored afterwards and compared byte-for-byte, so `git diff --quiet` is 0).
2. Build the Development Editor in that state, once in the Jolt configuration and once in the Chaos configuration.
   That build compiling is the first half: no ungated code names a member that the gate removes.
3. Run one Jolt and one Chaos harness session (the initiative's headless one-server, two-client session) and compare each with the baseline: every non-latency
   invariant and every `--identical` prefix must PASS, and `[LatencyBudget]`, `[LatencyBudget.Window]` and
   `[ChaosDilation]` must be absent. The harness's two Chaos client checks for those lines fail there by design; they are
   judged as expected-absent.

So the diagnostics feed no production output: removing them leaves every other line of the session within the
same-binary noise. The evidence for task 18 is in its impl notes.

---

## §5 The manager's former latency members and functions

| before task 18 (`ASimulationManagerUImpl`), historical names | now |
|---|---|
| stampLatency | `LatencyBudgetDiagnosticsUImpl::stamp` |
| noteLatencyWireSample | `LatencyBudgetDiagnosticsUImpl::noteWireSample` |
| postLatencyEvent_Internal | `LatencyBudgetDiagnosticsUImpl::post_Internal` |
| stampLatencyAfterStep_Internal | `LatencyBudgetDiagnosticsUImpl::stampAfterStep_Internal` |
| stampLatencyStepEnd_Internal | `LatencyBudgetDiagnosticsUImpl::stampStepEnd_Internal` |
| createLatencyBudget, and the post-tick-flush registration beside it in `BeginPlay` | `LatencyBudgetDiagnosticsUImpl::begin` |
| the post-tick-flush removal in `EndPlay` | `LatencyBudgetDiagnosticsUImpl::end` |
| the two latency statements of `unregisterFromNewFramework` | `LatencyBudgetDiagnosticsUImpl::forgetCharacter` |
| wrapInputProviderWithLatencyStamp | `LatencyBudgetDiagnosticsUImpl::wrapInputProvider` |
| onLatencyPostTickFlush | `LatencyBudgetDiagnosticsUImpl::onPostTickFlush` |
| tickLatencyBudget_GameThread | `LatencyBudgetDiagnosticsUImpl::tick_GameThread` |
| LatencyMailbox, LatencyClockMailbox, LatencyClockCounts | `LatencyBudgetDiagnosticsUImpl::Mailbox`, `ClockMailbox`, `ClockCounts` |
| m_latencyProbe, m_latencyMailbox, m_latencyClockMailbox | `m_probe`, `m_mailbox`, `m_clockMailbox` |
| m_latencyClockPosted_Physics, m_latencyClockLatest, m_latencyClockAtWindowStart | `m_clockPosted_Physics`, `m_clockLatest`, `m_clockAtWindowStart` |
| m_latencyMailboxDropped, m_latencyLastHandedCorrectionTick, m_latencyPostTickFlushHandle | `m_mailboxDropped`, `m_lastHandedCorrectionTick`, `m_postTickFlushHandle` |
| the backend hosts' sampleLatencyWindow_GameThread and logLatencyWindow_GameThread | the same names on each host's `editDiagnostics()` view |
| the Chaos host's stampLatencyAfterStep_Internal and stampLatencyStepEnd_Internal forwarders | the same names on `ChaosTickBackendUImpl::Diagnostics` |
| the input collection component's getInputSampledSeconds | `inputSampledSeconds`, on the view `UOGBrawlerInputCollectionComponent::getDiagnostics` returns |

**What changed in behaviour, at 1: nothing.** Two mechanical differences, both equivalent:

* **The post-tick-flush binding** was an `AddUObject` binding of the manager's own handler (onLatencyPostTickFlush,
  historical name). It is now an `AddWeakLambda` binding on `m_owner` that calls `onPostTickFlush`: the same multicast delegate, bound at the same point of
  `BeginPlay`, weakly bound to the same manager object, so it still never runs once the manager is gone.
* **The `[ChaosDilation]` clock fields** were the Chaos host's direct subtraction of the manager's two members. They are
  now `clockCountsInWindow()`, the same `uint32` subtractions, and the reset is `restartClockWindow()`, called where the
  assignment was. (The member stayed on the manager until this task because its type was the manager's private
  type; `ChaosTickBackendUImpl-rationale.md` §2.)

---

## §6 The probe's host side — stamp sites, threads and errors (og-simulationscheduler-withjolt task 1)

*Moved from `SimulationManagerUImpl-rationale.md` §16 by og-tickdriver task 18, with the code. The names are today's;
§5 maps the old ones. Claims re-verified against the tree on 2026-10-19.*

**What it is.** A measurement of the Chaos build, per hop, that the scheduler migration was compared against. The
engine-free instrument is `latencyBudget::LatencyBudgetProbe` (`OGSimulation/LatencyBudgetProbe.h`; its own rationale
explains keys, joins, counters, windows and percentiles). This section is the host side: where each hop is stamped, on
which thread, and how far each stamp can sit from the event it names. **It changes no gameplay behaviour, timing or
netcode: every addition is a timestamp and a counter.**

### Ownership

Each manager instance owns one `LatencyBudgetDiagnosticsUImpl`, and it one probe, created in `BeginPlay` by `begin` with
the role's hop mask (`kServerHops` on the authority world, `kClientHops` on a client world), plus two single-producer
mailboxes: `m_mailbox` (stamps) and `m_clockMailbox` (the prediction clock's event counts). All three are created
before the backend starts stepping (`startStepping`, at the end of `BeginPlay`) and are never reset before the actor is
destroyed, so a step can never reach a null or freed mailbox. PIE in one process has two manager instances and two
probes; the `role=` field tells their lines apart.

### Threads — the probe adds no unsynchronized crossing

The probe itself is **game-thread-owned** and has no internal lock. Everything reaches it one of three ways:

| path | used by | synchronization |
|---|---|---|
| `stamp` / `noteWireSample` | every game-thread site | same thread as the owner; both `checkf` that they run on the game thread |
| `post_Internal` → `m_mailbox` → drained in `tick_GameThread` | every step-thread site | the mailbox's release/acquire pair (`LatencyBudgetProbe-guards.md` G-01) |
| `m_clockMailbox` | the client clock's skip/stall/hard-resync counts | the same mailbox type |

Every step-thread read a stamp makes is **same-thread with its writer**: `m_storage`'s per-step walk (the core walks it
on that thread every step), `getLastUsedCaptureTick` (written by `collectInputAll` on that thread), the relayed-read
observation ring (noted by `collectInputAll` on that thread), `currentIntegratedTick` and the client clock's diagnostics
(both advanced by `onGameSimulation` on that thread). Every game-thread read is same-thread as well: the correction
buffer's tick (written by `sendCorrectionAll` inside `onPostSimulationGameThread`), `m_delayedInputComponentsById`, the
scene's network delta-time scale (written by the engine's time-dilation client RPC) and the local player controller's
physics tick offset (written by the engine's timestamp-setup client RPC). The one value that crosses from the game
thread to the step thread, the input-sample time, is a `std::atomic<double>` on the input collection component. **The
manager's CROSSING count stays three** (`SimulationManagerUImpl-rationale.md` §1), and this paragraph is the argument
for it, made rather than assumed.

On a dedicated server the step thread *is* the game thread (Chaos steps inline; the Jolt configuration steps the
authority inline too), so the mailbox is pushed and drained by one thread. That is a legal degenerate case of
single-producer, single-consumer.

### Why the capture stamp is a wrapper in `tryRegister`, not a line in the provider lambda

`USimmableUpdateComponent`'s provider lambda may not capture the manager (its guard G-05). The capture stamp therefore
lives in the wrapper `wrapInputProvider` returns, which `tryRegister` applies to a client's provider before handing it to
the core. The wrapper calls the component's lambda, then posts `InputSampled` and `Captured` for `step.getTick()`. It
captures `this`, the diagnostics object, a member of the manager that owns the core container the callable is stored in,
so it adds no lifetime edge to another actor, and the same input-collection pointer the inner lambda already holds.
**An empty provider is returned unchanged**: provider presence is the core's local-versus-remote identity test, and a
`checkf` states that the wrapper never changes it (§2).

### The hop table: stamp sites and known measurement error

Times are `FPlatformTime::Seconds` (one monotonic clock per process). "GT" is the game thread and "PT" the step's
thread (a task-graph worker on a client, the game thread on a dedicated server).

| hop | start: file · function (thread) | end: file · function (thread) | known error |
|---|---|---|---|
| `H1` | `OGBrawlerInputCollectionComponent.cpp` · `updateGameThreadCache`, first statement after the owner check (GT) | `LatencyBudgetDiagnosticsUImpl.cpp` · the wrapper from `wrapInputProvider`, after the provider returns (PT) | Starts at the frame's input refresh, not at the device event: Enhanced Input dispatched the frame's events earlier in the same frame, and the OS queued them up to one frame before that, so the true input age is **under**-stated by up to one game frame. The sample time is a relaxed atomic, unordered with the caches the capture reads, so a capture racing a refresh can pair a new time with the previous frame's values: an error of at most one frame, rare. |
| `H2` | the same wrapper, `Captured` (PT) | `SimmableUpdateComponent.cpp` · `sendLocalInputToAuthority`, after `ServerReceiveRemoteMove(bundle)`, once per capture tick in the bundle, first send wins (GT) | Microseconds. Redundant re-sends of the same capture are duplicates and do not move the hop. |
| `H3` (client) | `sendLocalInputToAuthority` (GT) | `LatencyBudgetDiagnosticsUImpl.cpp` · `onPostTickFlush`, bound to `UWorld::OnPostTickFlush` (GT) | See *the `H3` hook* below. |
| `H3` (server) | `SimmableUpdateComponent.cpp` · the relay host's flush callback bound in `attachInputRelayHost`, fired from `ASimulationInputRelay::PreReplication` after a flush published entries (GT, inside the net driver's flush) | `onPostTickFlush` (GT) | Measures the rest of that flush after this relay's pre-update. The same send caveats as the client row. |
| `H3c` | `LatencyBudgetDiagnosticsUImpl.cpp` · `tick_GameThread`, when a character's correction-buffer tick changed since the last frame (GT, end of `OnPostPhysicsStep`) | `onPostTickFlush` (GT) | A property leaves only when the replication system polls and sends it; a correction written but not sent at this flush still reads as left. A **lower** bound. The start is late by the frame's visualization work (the backend's `updateVisualization_GameThread`). |
| `H4` | `SimmableUpdateComponent.cpp` · `ServerReceiveRemoteMove_Implementation`, after the version fence, once per capture tick in the bundle, first arrival wins (GT, inside the net driver's receive dispatch) | `LatencyBudgetDiagnosticsUImpl.cpp` · `stampAfterStep_Internal`, for the capture tick `getLastUsedCaptureTick` reports after the step, stamped with that step's start time (PT) | The datagram reached the socket before the frame's receive dispatch: up to one server frame **under**-stated. An underrun step reports the sentinel and stamps nothing. |
| `H4r` | as `H4` | the relay flush callback (as `H3` server) | The hand-off is the flush into the replicated ring, not the staging write at receipt: staging is synchronous with receipt and would read zero. |
| `H5` | `stampStepEnd_Internal`, at each step's end (below), per character, for `currentIntegratedTick` (PT; inline on a dedicated server) | as `H3c` start | `noEnd` counts the ticks a character's state was not written: the correction rotation (`CorrectionRotationK`) skips a character on purpose, so on a rotation of 2 about half the ticks have no end. |
| `H6` | `SimmableUpdateComponent.cpp` · `onRelayedInputRingArrived`, once per capture tick in the ring, first arrival wins (GT, from the ring's OnRep) | `stampAfterStep_Internal`: the relayed-read observation for the step's tick names the applied capture tick, stamped with the step's start time (PT) | The bytes arrived up to one client frame before the OnRep. Only reads that applied a relayed capture stamp an end; a miss stamps nothing and the capture finalizes as `noEnd`. |
| `H6c` | `SimmableUpdateComponent.cpp` · `OnRep_CorrectionState`, for the peeked tick (GT) | `stampAfterStep_Internal`: a pending stamp at the start of the first client step after receipt (PT) | Measures when the correction is available to the next step, not when a resimulation applies it. |
| `H7` | `stampStepEnd_Internal` (PT) | `tick_GameThread`, after the visualization pass: the newest pending tick per character only (GT) | Not the photon. The engine interpolates the render poses behind the simulation on the game thread, and the render thread and GPU add one or two frames more. A step that ends between the mailbox drain and this stamp is stamped one frame late. On a dedicated server it measures the per-frame visualization pass, which draws nothing. |
| `WIRE` | client: `onTimingInfoReceived`, the raw round trip the timing relay hands `NetworkTimeEstimator::updateRTT`, halved (GT) | — | Half of a round trip assumes symmetric paths. Server: `ServerReceiveRemoteMove_Implementation`, `readRoundTripMs` of the root connection, halved, once per bundle (GT). The two roles' values come from different estimators. |

**H1 and H2 are measured for a remote client's own characters only.** A listen host's local players have no client
provider, so the host role has no capture path to stamp.

### The step-side stamp sites per configuration (og-simulationscheduler-withjolt task 54)

In the Jolt configuration "PT" is the step's thread (the tick driver's task on a client, the game thread inline on the
authority), every game-thread site is unchanged, and the step-side sites are the game binding's step hooks
(`BrawlerTickDriverGameUImpl-rationale.md`):

| stamp | Chaos configuration (`ChaosTickBackendUImpl.cpp`, through the host's `editDiagnostics()`) | Jolt configuration (`BrawlerTickDriverGameUImpl.h`) |
|---|---|---|
| step-start time for `H4`, `H6`, `H6c` and the clock counts | `OnPreSimulate_Internal`, immediately before `onGameSimulation` | og-simulation's `TickDriverStepHooks` `beforeSimulate`, after any replay and immediately before the normal step (`TickDriverStepHooks-guards.md` G-03); on a client early only by the Jolt scratch save |
| `stampAfterStep_Internal(start)` | `OnPreSimulate_Internal`, after `onGameSimulation`, when not resimulating | `onStepStarted`, from `beforePhysics`, after `onGameSimulation`, before the physics step |
| `stampStepEnd_Internal` (`H5`, `H7` starts) | `OnPostSolve_Internal`, when not resimulating | `onStepEnded`, from `afterTick`, after the post-step pass and the ring save: microseconds later |

Both configurations take the start time at the same instant, so gate 22 compared them like for like. `[ChaosDilation]` is
the Chaos configuration's only.

### The `H3` hook

`UWorld::OnPostTickFlush` is broadcast by the world's tick immediately after the net drivers' `TickFlush`, and it is the
earliest engine hook after it. The actual `SendTo` runs inside `TickFlush` (the connection's flush of its send buffer),
so the stamp is late by whatever `TickFlush` did after this connection's send: sub-millisecond with one connection,
longer on a busy server. It is early, or simply wrong, whenever the message did not leave at this flush:

* a client rendering faster than its `MaxNetTickRate` (120) skips a connection tick, so the packet leaves at the next
  frame;
* a saturated connection or the bandwidth limit defers the bunch;
* `net.IpConnectionUseSendTasks=1` (default 0) moves the `SendTo` onto a task after the flush;
* a replicated property (the correction, the relay ring) is sent only when the replication system polls it, so a write
  can wait a poll period.

In each case `H3` reads as though the message left this frame. Read `H3` as a lower bound.

### The `[ChaosDilation]` line (client only)

Per window, from `tick_GameThread`, in the Chaos configuration only. The Chaos host samples and logs it, through its
`editDiagnostics()` view's `sampleLatencyWindow_GameThread` and `logLatencyWindow_GameThread`
(`ChaosTickBackendUImpl-rationale.md` §2):

* `min` / `mean` / `max` / `samples`: the physics scene's network delta-time scale, sampled once per game frame from
  the world's physics scene (`GetPhysicsScene`, an `FChaosScene`; `FChaosScene::GetNetworkDeltaTimeScale`, beside the
  setter the engine's time-dilation client RPC calls). It is the engine's physics time dilation.
* `resets` / `offset`: how many times the first local player controller's network physics tick offset changed in the
  window, and its current value. The engine's timestamp-setup client RPC writes that offset. ⚠ A reset to the **same**
  value is invisible to this poll, and only the first local player controller is read.
* `skips` / `stalls` / `hardResyncs`: the window's increase in `ClientPredictionClock`'s Skip, Stall and HardResync
  counts, read on the step thread and carried by `m_clockMailbox` (`clockCountsInWindow`). A client that joins
  mid-session shows one hard resync in its first window.

### Log lines and categories

```
[LatencyBudget] role=Client hop=H2 p50=0.131 p95=0.273 p99=0.308 max=0.397 n=601 unit=ms noStart=0 noEnd=0 outOfOrder=0 overflow=0
[LatencyBudget.Window] role=Client seconds=10.01 stale=0 duplicate=2402 laneOverflow=0 mailboxDropped=0
[ChaosDilation] role=Client min=0.9762 mean=0.9998 max=1.0237 samples=1000 resets=0 offset=537 skips=0 stalls=0 hardResyncs=0
```

One `[LatencyBudget]` line per enabled hop and one `[LatencyBudget.Window]` line per window, both under
**`LogOGLatencyBudget`**; the `[ChaosDilation]` line under its own **`LogOGChaosDilation`** (one category per probe family,
`SimulationManagerUImpl-rationale.md` §4). All three are `Warning`. They are logged with `UE_LOG` directly, from this class
and from the Chaos host's view, never as core SIMLOG strings, so `RouteOGMessage` has no arm for them and its route table
is unchanged. `Config/DefaultEngine.ini` pins `LogOGLatencyBudget=Warning`; `LogOGChaosDilation` has no ini line and runs
at its declared default (`Log`), which prints its `Warning` line. `duplicate` counts the copies first-stamp-wins discarded
(an input re-sent in several redundant bundles, a capture carried by more than one relay round), so a large number there
is the expected state, not a fault.

### Verified, and what was not

A headless run (one `-server` process and two `-game -nullrhi` clients on the ThirdPerson map, 2026-10-06) printed every
enabled hop on both roles, with `stale=0` and `mailboxDropped=0`. Representative p50s: client `H6` 90 ms and server `H4`
149 ms (the relay-delay floor and the input delay, as expected), `H5`'s `noEnd` about half its ticks (the rotation), and
the dilation scale moving between 0.976 and 1.024. A headless client is smoother than a rendering one, so these numbers
are wiring evidence, not a baseline.
