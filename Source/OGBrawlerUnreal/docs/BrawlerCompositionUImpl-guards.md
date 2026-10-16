<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerCompositionUImpl.h` — guards

Prohibitions with a tagged site in `Source/OGBrawlerUnreal/BrawlerCompositionUImpl.h`
(og-simulationscheduler-withjolt task 81). Ids are opaque, stable, and retired rather than reused. The
reasoning lives in `BrawlerCompositionUImpl-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

The detection-before-routing order of `SystemsExec` is a `static_assert` in the header and needs no
entry; so is the identity of the manager's `ManagerType` with `BrawlerCompositionUImpl::Manager`, which
`SimulationManagerUImpl.h` asserts in `compositionContractsHold()` (rationale §3).

---

## G-01 — the composition aliases live inside the struct, and `Simulatables` is their single source of truth

**Tag site:** `BrawlerCompositionUImpl.h`, directly above `using Simulatables = SimulatableList<SimulatableBrawler>;`, first of the alias chain.

**Predecessor.** `SimulationManagerUImpl-guards.md` G-18 (*"The simulatable-pack aliases are
CLASS-scoped"*), retired by task 81 when the chain moved here. Its fence, verbatim — the bytes it
occupied in `SimulationManagerUImpl.h` before that header's task 11:

<!-- header lines 1032-1035 at b9f6d81 -->
```
// ---- SIMULATABLE-PACK ALIAS CHAIN -------------------------------------
//
// Single source of truth: widen this one alias and every type below inherits it.
// ⛔ CLASS-scoped so these names cannot leak to global scope from an adapter header. §2
```

**The prohibition.** ⛔ Do not hoist the aliases out of `struct BrawlerCompositionUImpl` to file scope,
and do not spell the simulatable pack anywhere but `Simulatables`: every other alias here derives from
it.

**What breaks if the edit is made.**
* **File scope.** The OGSim core lives in the global namespace, and so would the hoisted names:
  `Storage`, `NetSync`, `Manager`, `SystemsExec` and the rest are far more generic than the
  Brawler-prefixed names G-18 protected. `SimulationManagerUImpl.h` includes this header, and 12 of
  the module's 29 `.cpp` files include that one directly; task 83's backend host includes this header
  too. Every one of them would see the names at global scope. Only the struct name is meant to reach
  file scope.
* **A second spelling of the pack.** Widen `Simulatables` and the storage, the three peers, the
  integration executor, the systems executor and the manager all follow. An alias that names
  `SimulatableBrawler` itself does not follow, and the composition then disagrees with itself about
  which simulatables exist.

---

## §R — Retired ids

*(None.)*
