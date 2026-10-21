<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerSessionGameUImpl.h` — guards

Prohibitions with a tagged site in `Source/OGBrawlerUnreal/BrawlerSessionGameUImpl.h`. Ids are opaque, stable, and
retired rather than reused. The reasoning lives in `BrawlerSessionGameUImpl-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

No live entry. The rule on the latency hooks' call site (the source is resolved once, outside the wrapper's lambda) is
typed in the plugin template, so its guard is there: `UELatencyBudgetDiagnostics-guards.md` G-02 (OGSimulationUnreal).

---

## §R — Retired ids

None.
