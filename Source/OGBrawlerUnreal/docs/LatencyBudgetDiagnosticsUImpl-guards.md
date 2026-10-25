<!-- SPDX-License-Identifier: BUSL-1.1 -->
# LatencyBudgetDiagnosticsUImpl — guards (historical: retired ids only)

The game module's latency-budget class, a header and `.cpp` in `Source/OGBrawlerUnreal/` written by og-tickdriver task 18,
were **moved into OGSimulationUnreal by og-ue-simulationcomponentandmanagersplit task 19a (2026-10-21)**: code moved to
`UELatencyBudgetDiagnostics.h` (task 19a), a template over the brawler's `BrawlerSessionTraitsUImpl`. Its explanation
went with it: `UELatencyBudgetDiagnostics-rationale.md` (OGSimulationUnreal) for the template, and
`OGBrawlerSimulationManager-rationale.md` §16 for the brawler's wiring and the class's history.

This file is kept only as the record of the one id the header's tag used, so that it is never reused. It has no live
entry and no source file.

---

## §R — Retired ids

⛔ **An id is never reused.** A retired id may be named in prose; it may never again appear as a `⛔G-nn` tag.

### G-01 — RETIRED by og-ue-simulationcomponentandmanagersplit task 19a (2026-10-21): the fenced data block moved to the plugin template

**Was:** *"DIAGNOSTIC ONLY — READ BY NOTHING IN PRODUCTION"*. Its tag sat above `m_probe`, the first member of the block
of data members that exists only while `OG_LATENCY_BUDGET_DIAGNOSTICS` is 1.

**Why it retired.** Task 19a deleted the header; the block moved verbatim into the template, with its prohibition.
**Successor:** `UELatencyBudgetDiagnostics-guards.md` G-01 (OGSimulationUnreal).
