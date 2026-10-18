<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerChaosBindingUImpl` — guards

Every prohibition that governs a line of `BrawlerChaosBindingUImpl.h` and `BrawlerChaosBindingUImpl.cpp`. Each entry has an **opaque, stable id**. In the
source a single line `// ⛔G-nn  docs/BrawlerChaosBindingUImpl-guards.md` sits directly above the statement where the
forbidden edit would be typed.

**There is no live entry.** What the binding must provide is the `BrawlerEngineBinding` concept, which
`PhysicsBackendUImpl.h` asserts. Since og-tickdriver task 15b the binding has no member for the capsule check or the
park snap (the concept asks for them only of an engine with a world host), so there is nothing a Chaos path could call
that a guard would have to stop. Why each member is what it is lives in `BrawlerChaosBindingUImpl-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not
soften the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R
and its number is spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn`
tag.

---

## R. Retired ids

None.
