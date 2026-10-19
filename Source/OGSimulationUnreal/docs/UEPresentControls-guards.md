<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UEPresentControls.h` and `.cpp` — guards

Every prohibition that governs a line of `UEPresentControls.h` or `UEPresentControls.cpp`. Each entry has an **opaque,
stable id**. In the source a single line `// ⛔G-nn  docs/UEPresentControls-guards.md` sits directly above the statement
where the forbidden edit would be typed.

**There is no live entry.** The park snap is a parameter, so a caller that leaves it out does not compile; every other
field is filled from its console variable or is the constant 1.0. Why the files are as they are lives in
`UEPresentControls-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not
soften the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R
and its number is spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn`
tag.

---

## R. Retired ids

None.
