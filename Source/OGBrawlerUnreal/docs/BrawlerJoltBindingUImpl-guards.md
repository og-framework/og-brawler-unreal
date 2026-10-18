<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerJoltBindingUImpl` — guards

Every prohibition that governs a line of `BrawlerJoltBindingUImpl.h` and `BrawlerJoltBindingUImpl.cpp`. Each entry has an **opaque, stable id**. In the
source a single line `// ⛔G-nn  docs/BrawlerJoltBindingUImpl-guards.md` sits directly above the statement where the
forbidden edit would be typed.

**There is no live entry.** What the binding must provide is the `BrawlerEngineBinding` concept, which
`PhysicsBackendUImpl.h` asserts; the moved statements carried no guard. The capsule size contract is
`OGBrawlerUECharacter-guards.md` G-01, whose site is the capsule size, not this check. Why each member is what it is
lives in `BrawlerJoltBindingUImpl-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not
soften the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R
and its number is spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn`
tag.

---

## R. Retired ids

None.
