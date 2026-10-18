<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UETickDriverHost.h` and `.cpp` — guards

Every prohibition that governs a line of `UETickDriverHost.h` or `UETickDriverHost.cpp`. Each entry has an **opaque,
stable id**. In the source a single line `// ⛔G-nn  docs/UETickDriverHost-guards.md` sits directly above the statement
where the forbidden edit would be typed.

**There is no live entry.** What the files hold is checked by the compiler or at run time, and needs no entry: the
`static_assert` that the engine models og-simulation's `TickDriverEngine`; the two tick functions' `check`s that
their entry points are set; the
`checkf` that the host is begun once; and the `checkf` that a wait gets a valid event. Why the files are as they are
lives in `UETickDriverHost-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not
soften the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R
and its number is spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn`
tag.

---

## R. Retired ids

None. The predecessor's ids (the brawler's frame host, before og-tickdriver task 11) were all retired by og-tickdriver
task 8 to og-simulation's `SimulationTickDriver-guards.md`; they belong to that deleted document, not to this one.
