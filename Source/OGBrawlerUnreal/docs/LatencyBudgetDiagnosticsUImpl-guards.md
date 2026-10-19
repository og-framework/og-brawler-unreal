<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `LatencyBudgetDiagnosticsUImpl.h` — guards

Prohibitions with a tagged site in `Source/OGBrawlerUnreal/LatencyBudgetDiagnosticsUImpl.h` (og-tickdriver task 18).
Ids are opaque, stable, and retired rather than reused. The reasoning lives in
`LatencyBudgetDiagnosticsUImpl-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

Two properties are checked by the compiler, not here: the const access to production state (the `static_assert` beside
`m_owner`, rationale §3) and provider presence (the `checkf` in `wrapInputProvider`, in both states of the gate,
rationale §2).

---

## G-01 — DIAGNOSTIC ONLY — READ BY NOTHING IN PRODUCTION

**Site:** `std::unique_ptr<latencyBudget::LatencyBudgetProbe> m_probe;`, the first member of the block of data members
that exists only while `OG_LATENCY_BUDGET_DIAGNOSTICS` is 1.

**The prohibition.** ⛔ Every data member of this class sits in this one block, and nothing in production reads any of
them. Do not add a member here that a production decision, a production log line or a replicated value reads, do not
add an accessor that hands one of them to production code, and do not move a production value into this block. A value
that both feeds a decision and a latency line stays production, outside this class (`DiagnosticsConventions.md` §4:
the marker names the channel, never the fact).

**The consequence.** The block is compiled out of Shipping (`(!UE_BUILD_SHIPPING)`) and out of the fence build. A
production reader of one of these members either fails to compile there or, if it was itself gated to compile, makes
Shipping behave differently from Development, which is exactly what the fence test (rationale §4) exists to rule out.

**What breaks if the tag moves.** Nothing mechanical: the gate makes a Shipping or fence build fail to compile on an
ungated reader, but an edit inside this class can still hand a member out. The fence test's harness sessions are the
behavioural check.
