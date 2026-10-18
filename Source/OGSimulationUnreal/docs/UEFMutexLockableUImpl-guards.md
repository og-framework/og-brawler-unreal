<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UEFMutexLockableUImpl.h` — guards

Every prohibition that governs a line of `UEFMutexLockableUImpl.h`. Each entry has an **opaque, stable id**. In the
source a single line `// ⛔G-nn  docs/UEFMutexLockableUImpl-guards.md` sits directly above the statement where the
forbidden edit would be typed.

**There is no live entry.** The one rule this header carries, that the wrapper models og-simulation's
`BasicLockable`, is a `static_assert` in the header, so it needs no tag. Why the wrapper exists lives in
`UEFMutexLockableUImpl-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not
soften the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R
and its number is spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn`
tag.

---

## R. Retired ids

None.
