<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerSessionGameUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/BrawlerSessionGameUImpl.h`. The source file carries no prose: a two-line docs
pointer and code.

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes.

* **§1** says what the class is.
* **§2** is the two latency hooks and the brawler's input-sample time (hop `H1`'s start).
* **§3** is how the header is compiled and who includes it.

---

## §1 What the class is

`BrawlerSessionGameUImpl` holds the brawler's **phase hooks**: the statements a game-agnostic OGSimulationUnreal template
calls at a fixed point of its own sequence, where the brawler-specific statement used to be. The templates call them
statically through `BrawlerSessionTraitsUImpl::Game`, so a hook is a static member function with no state of its own.
Each hook body is the brawler's statement, verbatim; the hook sits at the exact position of the statement it replaces.

As of og-ue-simulationcomponentandmanagersplit task 19a it has two hooks, both for the latency template
`UELatencyBudgetDiagnostics`; the design's other twelve arrive with the session template.

---

## §2 The latency hooks

| hook | brawler statement | when, which thread |
|---|---|---|
| `inputSampleSource(component)` | `owner.getOwnerInputCollection()`, the component's input collection component (or `nullptr`) | once per registration, in the wrapper's construction (`wrap`), on the game thread |
| `inputSampledSeconds(source)` | `source != nullptr ? source->getDiagnostics().inputSampledSeconds() : 0.0` | once per call of the wrapped provider, on the step's thread |

The read is **two-phase**, as it was before the move: the component is resolved once, on the game thread, and the step
thread only loads the time the input collection component published. Why that matters, and the guard on the call site,
are the plugin's (`UELatencyBudgetDiagnostics-rationale.md` §2 and `UELatencyBudgetDiagnostics-guards.md` G-02, in
OGSimulationUnreal's docs).

**The input-sample time** is `UOGBrawlerInputCollectionComponent`'s `std::atomic<double>`, written by `noteInputSampled`
(`FPlatformTime::Seconds`, relaxed) as the first statement of `updateGameThreadCache` after the owner check, on the game
thread, once per frame: hop `H1`'s start. It starts at the frame's input refresh, not at the device event: Enhanced Input
dispatched the frame's events earlier in the same frame, and the OS queued them up to one frame before that, so the true
input age is **under**-stated by up to one game frame. It exists only while `OG_LATENCY_BUDGET_DIAGNOSTICS` is 1, as does
its view `getDiagnostics()`, so `inputSampledSeconds` is declared only then; `inputSampleSource` is a production read and
exists in both states. A time of `0.0` (no refresh yet, or no input collection component) posts no `InputSampled` stamp.

`H1` and `H2` are measured for a remote client's own characters only: `tryRegister` wraps the provider on the client
branch, and a listen host's local players have no client provider.

---

## §3 How the header is compiled

* **Inline, in one `OGSIM_OPTIMIZE_OFF` / `OGSIM_OPTIMIZE_ON` pair.** The hook bodies are the statements that compiled in
  the latency class's own translation unit under that pair, so they keep its optimization state; the declarations sit
  above the pair.
* **Who includes it.** `SimulationManagerUImpl.cpp`, the one file that instantiates the wrapper (`tryRegister` calls
  `wrapInputProvider`). The header needs the complete component and input collection component, so it includes both;
  `BrawlerSessionTraitsUImpl.h` only forward-declares `BrawlerSessionGameUImpl`, which keeps those includes out of every
  file that includes the manager.
