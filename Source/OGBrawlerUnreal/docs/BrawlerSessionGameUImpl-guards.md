<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerSessionGameUImpl.h` — guards

Prohibitions with a tagged site in `Source/OGBrawlerUnreal/BrawlerSessionGameUImpl.h`. Ids are opaque, stable, and
retired rather than reused. The reasoning lives in `BrawlerSessionGameUImpl-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

The rule on the latency hooks' call site (the source is resolved once, outside the wrapper's lambda) is typed in the
plugin template, so its guard is there: `UELatencyBudgetDiagnostics-guards.md` G-02 (OGSimulationUnreal).

---

## G-01 — the neutral input is `getZeroPlayerInput()`, not `PlayerInput{}`

**Tag site:** `BrawlerSessionGameUImpl.h`, directly above `return simulatableBrawler::getZeroPlayerInput();` in the inline
body of the hook `neutralInput()`, which the session's `beginPlay` passes to `setNeutralInput` on both role branches.

**The prohibition, as it stood in the source:**

> Fills the [0, effectiveDelay) window. ⛔ NOT PlayerInput{}: (0,0,1) forwards, load-bearing. §5

**Why this is still a tag, measured.** Making it a `static_assert` was tried and does not compile:
`simulatableBrawler::PlayerInput` has no `operator==` (C2678), and `getZeroPlayerInput()` is not `constexpr` in any case.

**The consequence.** A value-initialised input carries zero forward vectors where the game expects `(0,0,1)`.

*R0, task 27b:* re-checked: `simulatableBrawler::PlayerInput` is `SyncedPlayerInput` (`SimulatableBrawlerTypes.h`), which
declares no `operator==`, and `getZeroPlayerInput()` is a plain `inline` function returning `PlayerInput::zero()`. Since the
guard was written, `BrawlerSyncedPlayerInput.h` asserts at compile time that `SyncedPlayerInput::zero()` aims at `(0,0,1)`
and that the default-constructed input's aim differs from it; neither assertion sees which value this hook returns, so the
tag still marks the one place the wrong edit (`return PlayerInput{};`) is typed. The session uses the hook's value on both
roles, so the guarded value now reaches the authority's neutral input as well as the client's (it did before the move: both
branches called `getZeroPlayerInput()`; the tag sat on the client's).

*Moved from `SimulationManagerUImpl-guards.md` G-60 (its task 12 conversion) by og-ue-simulationcomponentandmanagersplit task 27b. Ruling Q-J: it guards the
value, not the sequence, so it stays BUSL with the hook body; the sequence's guard on the authority call is
`UESimulationSession-guards.md` G-23 (OGSimulationUnreal).*

---

## §R — Retired ids

None.
