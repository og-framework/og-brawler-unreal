<!-- SPDX-License-Identifier: MPL-2.0 -->
# The comment rule — what goes in source, what goes in docs, and how the join is checked

> **Authoritative.** Enforced by `tools/lint/guard_tag_lint.ps1` (tags) and
> `tools/lint/doc_anchor_lint.ps1` (doc claims). Where a team's own notes disagree with this file,
> **this file wins** — fix the notes.
>
> **Scope:** every source file in this repository and its submodules. ⚠ **It travels with this
> repo.** A project scaffolded against a different root must bring its own copy; do not cite this
> path from anywhere that path will not resolve.
>
> ⛔ **NOTHING THAT SHIPS MAY CITE THIS FILE.** `og-simulation` is MPL-2.0 and ships standalone to
> consumers who have no `tools/` directory. This is a *process* rule; it is not a claim a shipped
> document makes. A `-rationale.md` or `-guards.md` that points here creates exactly the broken
> join this rule exists to remove — a predecessor shipped **15** such references and it is on record
> as a defect.

---

## 1. The rule

**A source file contains: the licence header, a two-line docs pointer, code, and one-line tags.
Nothing that explains.**

```cpp
#pragma once
// SPDX-License-Identifier: BUSL-1.1
// docs/CollisionCategoryConstants-rationale.md · docs/CollisionCategoryConstants-guards.md
```

⛔ The header is **two paths, not a précis**. A reader who wants the why follows them.

## 2. The order of preference — the most important section

**Ask these in order. Stop at the first that applies.**

1. ⭐⭐ **CAN THE COMPILER ENFORCE IT?** A `static_assert`, an `OG_CHECK`, or a type that makes the
   mistake unrepresentable ⇒ **write no comment and no doc entry at all.** It cannot be missed,
   cannot go stale, and creates no join.
   ⛔ **Answer by COMPILING, never by reasoning.** Reasoning was measured wrong in *both* directions
   three times on one file — including a construct that reads as expressible and is not.
   ⭐ Two conversions took **6 of 16** and **6 of 26** guards this way, and on both it was the
   highest-value change in the task.
   ⚠ **An assertion's message must name what it replaced** (e.g. `"Was fence T3-3"`), or a later
   deletion of the assertion passes every gate silently.
2. **Is it a PROHIBITION** — would its absence cause a *wrong edit*? ⇒ §3.
3. **Is it a DERIVATION a reader needs to CHANGE a specific line?** ⇒ §4.
4. **Everything else** ⇒ the rationale doc, no tag.

## 3. Guards — prohibitions

Two things: an entry in `docs/<File>-guards.md` with a stable id, and **a one-line tag at the site**:

```cpp
// ⛔G-07  docs/BrawlerMovementSimulation-guards.md
```

* ⭐ **The tag sits exactly where the wrong edit would be typed** — the declaration, the branch, the
  statement. **Position is the whole design.** A tag at the top of the file is not a tag at the site.
* Ids are **opaque, stable, and retired — never reused.** A reused id silently re-points every
  reference that ever named it.
* The entry carries the prohibition, its consequence, and **what breaks if it moves**.
* ⭐ **Extraction is a MOVE, not a rewrite.** The first version of an entry is the shipped bytes of
  the comment it replaces.

## 4. Derivations

⛔ **There is no third document.** A derivation lives in `docs/<File>-rationale.md`, in a section
whose heading **ends with its id**, and is reached by a tag on the expression it derives:

```cpp
// ∴D-05  docs/BrawlerMovementSimulation-rationale.md
```
```markdown
## 19. The dual-basis decomposition — task 57 / ruling #29 ∴D-01
```

⛔ **The bar is high: could someone change this expression correctly with only the identifiers in
front of them?** If yes, **no tag** — it is ordinary rationale.
⚠ **More `D` tags than `G` tags means the bar has been dropped.** Re-triage.

## 5. Rules that bind every tag

* ⛔ **ONE TAG PER SITE.** Stacked tags were the single largest cause of *"this would not have
  stopped me"* in every reader test run. If a `G` and a `D` want the same line, the `G` keeps the
  line and the `D` moves to the expression it actually derives; if they truly coincide, **merge the
  doc entries, not the tags.**
* ⛔ **A tag carries NO WORDS** — an id and a path. The moment one grows a sentence, this rule has
  failed.
* ⛔ **NEVER HALF-CONVERT A FILE.** A tag in a file with no docs **dangles and fails the gate**.
  Either convert a file fully, or leave it entirely in the old convention.

## 6. What may stay in source

1. The licence header.
2. The two-line docs pointer.
3. Tags.
4. `static_assert` / `OG_CHECK` messages and log strings — **these are code, and they are better
   than any comment.**
5. `} // namespace X`.
6. `/*paramName*/` annotations at a positional call. ⚠ These can go stale on a rename — accepted,
   because the alternative is a transposed-argument bug the compiler cannot see.

⛔ **Not** trailing labels. ⛔ **Not** orientation blocks. ⛔ **Not** section banners.

## 7. The gate

`guard_tag_lint.ps1`, per class (`G`, `D`):

| check | fails when |
|---|---|
| **1 — tag → doc** | a tag has no entry with that id |
| **2 — doc → tag** | an entry no source tag references (**orphan** — a guard lost its site) |
| **3 — uniqueness** | two sites share an id, or two entries do |
| **4 — retirement** | a retired id reappears |

⛔ **A hard gate, not a report.** ⭐ **CHECK 2 is the best property this design has:** delete a
guarded symbol and its tag goes with it, orphaning the entry and **failing the build** — a
protection a comment could never provide.

⛔⛔ **EVERY ARM NEEDS A POISON CASE THAT IS SEEN TO FAIL.** This project shipped **four** checks
that could not fail and caught two only after the fact — one because a class was routed by first
letter (`'Duplicate'.StartsWith('D')`), and both arms exited 1 either way, so the exit code hid it.
**An arm whose failure you have not witnessed is not a control.** Prefer a matrix that shows each
arm firing on *its own* check and leaving the others at zero.

## 8. Before you move anything

* ⛔ **VERIFY FIRST (R0).** Moving a false sentence into a doc **launders** it: the doc is read less
  often than the file, so it stays wrong longer. Expect **5-9 false claims per file**; an audit
  finding none has not been done. ⚠ **Existence checks verify existence, never truth.**
* ⚠ **FINDINGS DECAY.** One audit decayed **five distinct ways in a single day** — at scales down to
  three minutes — on a file that never changed. **Re-verify against the tree you are standing on,
  including against the audit itself.**
* ⛔ **SWEEP OUTWARD:** grep for prose in *other* files that quotes this one. One claim believed to
  be at 2 sites was at **11 across 6 files**.
* ⛔ **SWEEP INWARD:** find what *cites this file's declarations*. A citation naming a declaration
  whose comment became a doc entry lands on nothing unless that declaration carries a tag — **seven
  of nine inbound citations broke this way** on one file.
* ⛔ **COPIED-NOT-MOVED:** confirm the text is present in the doc **before** each deletion.
* ⚠ **A guard manifest alone does not prove nothing was lost** — a rewrite defeats text matching.
  Reconcile, and give each leg a poison arm.

## 9. Cost, and the honest record

**Convert ONE file, then judge.** Measured: a 197-line file took ~40 minutes; a 1,700-line file took
hours and removed **98.8%** of its comments.

The judgement is per guard, asked cold: *if I were about to make the edit this tag forbids, would it
stop me?* Across four files the answers ran **0/1/4 → 1/3/6 → 2/4/5 → 3/6/7** (yes / uncertain /
no) — improving, and **never yet a majority.**

⛔ **"Keep going" is one possible answer, not the expected one.** ⚠ And a fabricated identifier or
number appears about **once per 98 assertions** in converted prose — **every instance so far was
found by reading, none by a checker.**
