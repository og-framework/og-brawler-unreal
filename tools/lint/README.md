<!-- SPDX-License-Identifier: MPL-2.0 -->
# Configurability lint (R-P1)

`configurability_lint.ps1` is the structural enforcement of the **configurability
rule** — Synthesis Addendum Correction 5, captured as risk **R-P1** and specified
in `OGBrawlerNetworkModelResearch/arch/risks_and_plan.md §6.1`.

> **The rule:** every tunable value named in the synthesis lives as a field in
> `TimeConfig` and is read from there at runtime. It must **not** be re-declared as
> a magic literal anywhere else — a second source of truth is how defaults silently
> drift apart.

This lint is one of three R-P1 mitigation arms:

| Arm | What it catches | Where |
|---|---|---|
| **This lint** (structural) | A config field re-declared with its magic literal outside its `TimeConfig` home | `tools/lint/configurability_lint.ps1` |
| **Default-drift gate** (runtime) | A `TimeConfig` default silently changed | `TimeConfigDefaultsTest.cpp` (Task 6) |
| **Quarterly audit** (procedural) | New fields added without lint coverage | risks_and_plan §6.3 |

The lint guards against literals leaking *out* of `TimeConfig`; the gate guards
against the defaults *inside* `TimeConfig` silently changing. Both are required.

## Running locally

Requires **PowerShell 7+** (`pwsh`).

```pwsh
pwsh tools/lint/configurability_lint.ps1
```

Options:

| Parameter    | Default                     | Purpose                                  |
| ------------ | --------------------------- | ---------------------------------------- |
| `-RepoRoot`  | two levels above the script | Repo root to resolve scan paths against. |
| `-ScanRoots` | `Plugins`, `Source`         | Top-level dirs to scan (relative).       |

Exit codes: **0** = clean · **1** = one or more violations · **2** = usage/IO error.

On a hit the output is one line per violation, machine-greppable:

```
<file>:<line>: <message> (Synthesis Addendum Correction 5 binding rule)
```

## What it scans

- File types: `*.h`, `*.cpp` under each `ScanRoot`.
- Excluded paths: `Binaries/`, `Intermediate/`, `ThirdParty/`, `glm/`, `.git/`,
  `.vs/`, and any `build*` directory (covers `extern/*/build*` and CMake `build/`
  trees). Task 5 names `Binaries / Intermediate / extern/*/build* / glm`; `ThirdParty/`
  (vendored Jolt) is excluded on the same "vendored code" rationale as `glm`.
- Before matching, each line is **stripped** of `//` and `/* */` comments (including
  multi-line) and `"string"` / `'char'` literals, so documentation that says
  `"100 Hz"` or a log string containing `=15` is never flagged — only *code* matches.

## Design decision — "field-anchored" matching

> Decided 2026-06-20 in discussion between the implementer and the user (project
> owner), who approved the field-anchored design over the alternatives below.

A **violation** is a config *field name* used as a **non-member lvalue** assigned its
magic literal:

```cpp
int32_t rollbackWindowTicks = 12;   // FLAGGED — a second source of truth
redundancyDepthTicks        = 5;    // FLAGGED
```

Legitimate **uses** of the config are *not* flagged, because they assign through a
member access on a `TimeConfig` instance (a negative lookbehind for `.` / `->`
excludes them):

```cpp
cfg.tickFrequency          = 60.0;  // OK — configuring an instance (e.g. a test fixture)
config.rollbackWindowTicks = clamp; // OK
if (drift <= m_config.hardResyncThresholdTicks) ...  // OK
```

This is why the lint needs almost no allow-list: every legitimate runtime/test
assignment to a `TimeConfig` value already goes through `.`/`->`, so it is excluded
generically rather than file by file.

### Known limitation (intentional)

A **keyword-free** magic number is **not** flagged:

```cpp
if (depth > 12) { ... }   // NOT flagged
int32 dummy = 12;         // NOT flagged
```

This is a deliberate trade-off, not an oversight. The R-P1 failure mode most worth
catching *is* the keyword-free literal — but a bare-value lint **cannot run clean on
this tree**. The guarded magic numbers (`12, 20, 60, 100, 5, 3`) appear constantly in
unrelated code: `LatSegs = 12`, `MaxWalkSpeed = 100.f`, `NetUpdateFrequency = 100.0f`,
`StateBufferSize = 60`, dozens of Jolt geometry `= 3`, and so on. A bare-value matcher
produces ~120 false positives on an unmodified checkout, making the "runs clean on
HEAD" criterion impossible. The keyword-free case is instead covered by **code review**
and the **default-drift gate**.

### Alternatives considered (and rejected)

| Option | Why not |
|---|---|
| **Bare-value match** (flag any `= 12` / `= 60` …) | ~120 false positives on clean HEAD; unusable. |
| **Scoped bare-value + allowlist** (netcode dirs only, allowlist legit lines) | Still flags `LatSegs = 12`, `MaxWalkSpeed = 100`; needs a fragile line-level allowlist of *non-config* code that breaks on every unrelated edit. |
| **Hybrid** (field-anchored + scoped bare-value pass) | Best coverage but carries the allowlist-maintenance burden of the scoped pass; deferred unless a real keyword-free regression appears. |

> Note: Task 5's acceptance criterion gives `int32 dummy = 12;` as an example that
> "must fail". Taken literally that requires bare-value matching, which contradicts
> the "runs clean on HEAD" criterion given this codebase. The field-anchored design
> honours the *intent* (a config value re-declared outside `TimeConfig` fails) while
> staying usable; the literal `dummy = 12` example is consciously out of scope.

## BLACKLIST schema — and the contract

The blacklist is the `$Blacklist` array near the top of the script, **one entry per
guarded `TimeConfig` field**:

```pwsh
@{
    Field   = 'rollbackWindowTicks'                              # the field this guards
    Pattern = '(?<![\w.>])rollbackWindowTicks\s*=\s*12\b'       # field-anchored regex
    Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')   # legitimate homes (leaf names)
    Message = 'rollbackWindowTicks default (12) must live only in TimeConfig; ... (R-P1)'
}
```

| Key       | Meaning                                                                  |
| --------- | ----------------------------------------------------------------------- |
| `Field`   | The `TimeConfig` field guarded (for humans + diagnostics).              |
| `Pattern` | .NET regex vs each stripped line. `(?<![\w.>])` excludes member access. |
| `Allowed` | File **leaf names** where the declaration is legitimate. Empty => flagged everywhere. |
| `Message` | Printed on a hit. Names the field and references `R-P1`.                |

### Current entries

| Field                      | Value(s) caught | Allowed homes |
| -------------------------- | --------------- | ------------- |
| `rollbackWindowTicks`      | `12`            | `TimeConfig.h`, `TimeConfigDefaultsTest.cpp` |
| `rollbackWindowHardCap`    | `20`            | `TimeConfig.h`, `TimeConfigDefaultsTest.cpp` |
| `redundancyDepthTicks`     | `3` / `5`       | `TimeConfig.h`, `TimeConfigDefaultsTest.cpp`, `InputRedundancyBundle.h`, `FInputRedundancyBundle.h` |
| `tickFrequency`            | `60` / `100`    | `TimeConfig.h`, `NetworkTimeEstimator.cpp` |
| `hardResyncThresholdTicks` | `15` (old default — must not reappear; current is `21`) | *(none)* |

> The `hardResyncThresholdTicks` entry has an **empty** `Allowed` list on purpose, so
> a revert of the `15 → 21` bump (R-D3) is caught even inside `TimeConfig.h`. Test
> fixtures that set `cfg.hardResyncThresholdTicks = 15` are member-access and excluded.

### The contract (READ THIS before changing TimeConfig)

> **Every PR that adds, removes, or renames a `TimeConfig` field MUST update the
> `$Blacklist` in `configurability_lint.ps1` in the same change** — alongside the
> matching assertion in `TimeConfigDefaultsTest.cpp`.

This mirrors risks_and_plan §6.4: every later stage's deliverables must include
*"All new constants are `TimeConfig` fields, not hardcoded literals; lint + Catch2
default-drift updated."*

### Stage-specific extension points

- **Task 7 (`FInputRedundancyBundle`)** — the `redundancyDepthTicks` entry already
  allow-lists `InputRedundancyBundle.h` / `FInputRedundancyBundle.h`; T7 should keep
  whichever basename it ships and drop the other.
- **Task 14 (Stage 2 100→60 Hz audit)** — if the audit finds the async-physics tick
  setup hard-codes an Hz literal in a file, add that file to the `tickFrequency`
  entry's `Allowed` list (the member-access derive site
  `config.tickFrequency = 1.0/GetAsyncDeltaTime()` is already auto-excluded).
- **Task 15 (flip default 5→3)** — no lint change needed; both `3` and `5` are
  already covered by the `redundancyDepthTicks` entry.

## CI integration

Both lints in this directory — R-P1 configurability and R-UE1 visualization
isolation (see next section) — are wired into a **single CI step**, `Lint (R-P1
configurability + R-UE1 visualization isolation)`, placed **before** the build:
neither needs submodules fetched or a compiler, just the source tree.

```yaml
      - name: Lint (R-P1 configurability + R-UE1 visualization isolation)
        shell: pwsh
        run: |
          $failed = 0
          pwsh tools/lint/configurability_lint.ps1
          if ($LASTEXITCODE -ne 0) { $failed = 1 }
          pwsh tools/lint/visualization_hitbox_isolation.ps1
          if ($LASTEXITCODE -ne 0) { $failed = 1 }
          exit $failed
```

> The `$failed` accumulator is load-bearing. A bare two-line `run:` block exits with
> the status of the **last** command, so a configurability failure followed by a
> clean visualization run would pass the step silently. Both lints always run (so
> one report shows every violation), and the step fails if either did.

`windows-latest` ships PowerShell 7 (`pwsh`) by default. On a Linux runner, install
PowerShell first or run via the `mcr.microsoft.com/powershell` container. The step
fails the job on exit code `1`, surfacing the offending `file:line` in the CI log.

> ⚠ **CI is currently PARKED.** `.github/workflows/standalone-tests.yml` is renamed
> `.disabled` because the default `GITHUB_TOKEN` cannot fetch the private sibling
> submodules (see that file's header). The step above **is already present in the
> parked workflow** and becomes live the moment the file is renamed back — but until
> then both lints run **manually** / as a local pre-commit step. Run them before any
> PR touching `TimeConfig`, its consumers, or a visualization component.

---

# Visualization/hitbox isolation lint (R-UE1)

`visualization_hitbox_isolation.ps1` is the Stage 5 / D5.5 automated arm of risk
**R-UE1** — *"presentation-only invariant" code-discipline leak into hitbox
calculation* (`risks_and_plan.md` R-UE1; proposal §7.4).

> **The rule:** visualization code never contributes to hitbox math. Hit resolution
> runs against server-authoritative sim state on the server only.

```pwsh
pwsh tools/lint/visualization_hitbox_isolation.ps1
```

It globs `*Visualization*` and `*UEHUD*` (`.h|.cpp`) under **two** roots —
`Plugins/OGBrawler/Source/OGBrawler/og-brawler/OGBrawler/` and
`Source/OGBrawlerUnreal/` — and fails on any `#include` of a hitbox-**resolution**
header. Neither root isolates its visualization units in a `Visualization/`
directory, so a filename pattern is the available handle; `*UEHUD*` is there because
the UE half's only screen-space drawing surface does not carry `Visualization` in its
name. 23 files today, all printed on every run.

> ⚠ **The UE root was added on 2026-08-25.** Before that the lint scanned the
> og-brawler core alone, so a green run said nothing about any UE-side visualization
> unit even when the unit's own name matched the glob. Every configured root must now
> exist **and match at least one file**; a root that matches nothing is exit 2, not a
> quiet pass.

Both parameters are arrays. `pwsh -File` cannot pass one: use
`pwsh -Command "& ./tools/lint/visualization_hitbox_isolation.ps1 -VisualizationRoot A,B"`
for several roots; a single root over a scratch tree works under `-File` as before.

Like R-P1, this is one of three mitigation arms, and the lint is the weakest:

| Arm | What it catches | Where |
|---|---|---|
| **`const&` signature** (structural) | Direct mutation of sim state from `visualize(...)` | the type system, at compile time |
| **This lint** (structural) | A visualization TU including the hit resolver | `tools/lint/visualization_hitbox_isolation.ps1` |
| **Review checklist** (procedural) | Side channels — statics, singletons, injected functors | `VISUALIZATION_DISCIPLINE.md` §3 |

The lint sees **direct** `#include` lines only — a transitive include is invisible to
leaf matching by construction — so **a green run is not proof the invariant holds**. The full invariant, the For Honor precedent, the permitted-vs-forbidden
boundary, and the review checklist live in
`Plugins/OGBrawler/Source/OGBrawler/og-brawler/OGBrawler/VISUALIZATION_DISCIPLINE.md`.

**Contract, two halves.** A new header that **resolves** hits gets a blacklist entry in
the same change; a new header that merely **declares** sim state types does not —
visualization reading post-sim state through a `const&` is the design, not an
exception. And a new presentation translation unit is covered **iff its filename
matches a glob**: name it `*Visualization*`, or add its glob in the same change.

---

# Doc-anchor lint (M1)

`doc_anchor_lint.ps1` resolves every machine-checkable claim a **docs tier**
makes about the tree, and reports the ones that do not resolve.

> **The rule:** a document that names a file, a `Class::member`, an `m_member` or
> an identifier is **asserting that thing exists**. Every such assertion must
> resolve against the tree, or be **declared** as an intentional exception with a
> reason.

Ruled in at gate **G2** of the `og-source-doc-extraction` initiative (2026-08-21),
as mechanism **M1**. It exists because the six recorded staleness defects in
`og-netcode-v2-input-relay` were all **broken joins**: one fact asserted in two
places, where the edit that broke it touched one place and could not see the other.

## Running locally

Requires **PowerShell 7+** (`pwsh`).

```pwsh
pwsh tools/lint/doc_anchor_lint.ps1
```

With no arguments it lints **every docs tier** against `Plugins`, `Source`,
`Config`, `docs` and `tools`.

### There is more than one tier, and that is a licence fact

| tier | licence | travels with |
|---|---|---|
| `Plugins/OGSimulation/…/OGSimulation/docs` | MPL-2.0 | the `og-simulation` submodule |
| `Source/OGBrawlerUnreal/docs` | BUSL-1.1 | this repository |

Decision **D9** (2026-08-23) ruled the second tier in: `OGBrawlerUnreal`
documentation cannot live in the MPL-2.0 tier, because that tier ships with a
different repository under a different licence.

> **Why they are both in the DEFAULT and not left to the caller.** A tier nobody
> names on the command line is not reported as unlinted — it is never opened, and
> the run still prints `RESULT: CLEAN` and exits 0. Silence that reads as success
> is this project's signature defect. So every defaulted run prints one
> `present` / `EMPTY` / `ABSENT` line per tier before the counts, and a run where
> **no** tier holds a document is exit 2, not a clean zero.

| Parameter             | Default                          | Purpose |
| --------------------- | -------------------------------- | ------- |
| `-DocPaths`           | every `.md` in every tier below  | Documents to lint. |
| `-DefaultDocDirs`     | the two tiers above              | Tiers discovered when `-DocPaths` is absent; each one's state is printed. |
| `-ScanRoots`          | `Plugins Source Config docs tools` | Where files and symbols are resolved. |
| `-RepoRoot`           | two levels above the script      | Root the relative paths resolve against. |
| `-PathAlias`          | `<core>` → core headers, `<brawler>` → `Source/OGBrawlerUnreal/` | Prefix aliases used inside anchors. |
| `-ExternalNamespaces` | `std Chaos FMath UE TArray …`    | Qualified roots skipped **and enumerated**. |
| `-NoBareIdentifiers`  | off                              | Turns off the identifier/type/snippet rules. |
| `-NoCommentStrip`     | off                              | **Negative control only** — see below. |

Exit codes: **0** = clean · **1** = an unresolved anchor or an unused escape ·
**2** = usage/IO error.

## The anchor grammar

| shape | example | resolves when |
|---|---|---|
| FILE | `` `Network/RemoteInputCache.h` `` | a file whose path suffix matches exists under a scan root |
| FILE:LINE | `` `ResimGatePolicy.h:86` `` | …and the line (or `86-92` range) is inside it |
| PAIR | `` `File.h` :: `symbol` ``, or two adjacent table cells | the symbol occurs in **that** file, on a non-comment line |
| QSYM | `` `StateCorrectionCache::pushPredictionTick` `` | a file declares that type and carries the member in code |
| CASE | `` `Panel.OneFactorScalesBothTheGeometryAndTheText` ``, `` `OGBrawler.InputHistoryDisplay` `` | the exact name appears **inside double quotes** in non-comment code — see below |
| MEMBER | `` `m_pendingResimAnchorTick` `` | it occurs in non-comment code somewhere |
| IDENT / TYPE | `` `collectInputAll` ``, `` `ResimSweepDiagnostics` `` | same, for lowerCamelCase and PascalCase tokens |
| SNIPPET | `` `if (!withinDepth) { ++x; }` `` | every identifier inside the fragment resolves |

Everything else inside backticks is **counted and printed as UNCHECKED**. That
number is part of the output on every run, clean or not: a lint that hides what it
could not parse is the failure this project keeps repeating, and "clean" must never
be readable as "verified".

## Dotted registered names — Catch2 case names and console variables

A test-case name and a console variable are **not identifiers**. They exist only as
string literals, so every symbol rule above is structurally blind to them, and until
2026-08-27 a document citing one was **counted as UNCHECKED while the run reported
CLEAN**. That mattered here more than anywhere: this repository's rationale documents
cite case names as their *evidence*, so the sentence "a test pins this" was the one
claim the lint could not check.

The CASE arm closes it. A token resolves when **every segment is PascalCase** and the
**exact name, double quotes included**, occurs in non-comment code:

```
`Panel.OneFactorScalesBothTheGeometryAndTheTextAndTheyCannotDriftApart`
    -> TEST_CASE("Panel.OneFactorScalesBothTheGeometryAndTheTextAndTheyCannotDriftApart", ...)
`OGBrawler.InputHistoryDisplay`
    -> TEXT("OGBrawler.InputHistoryDisplay") in the FAutoConsoleVariableRef registration
```

Three properties are deliberate:

- **The quotes are required.** A bare substring search would pass a citation of
  `Panel.EveryArrowIsTheScreenImageOfTheAngle` against a test actually named
  `…OfTheAngleTheMatcherTested` — and appending words is the most common rename.
- **Comment stripping applies.** A deleted test whose name survives in a HISTORY
  comment does not certify the citation, the same discipline the symbol rules use.
- **A lowercase segment is not a name.** `storage.add`, `decision.landed` and
  `m_reconciliation.wipeAllForResync` are member access written out in prose. They
  stay UNCHECKED, because their owning expression is not something the tree declares.
  Widening the arm to swallow them would make it fire on ordinary prose.

FILE is tried first, so `` `src/CaseNames.cpp` `` is still a path anchor, not a name.
A name that is *supposed* not to resolve takes a `lint-external-ref` declaration like
any other anchor.

## Comment-stripping is load-bearing

Symbol resolution runs against source text with `//`, `/* */` and (in `.ini`) `;`
comments removed — the same discipline as `configurability_lint.ps1`.

Measured, not assumed: `SimulationNetSync::collectInputAll` is a **dead owner** —
the method moved to `SimulationInputResolution` — yet `SimulationNetSync.h` still
mentions `collectInputAll` four times, **all four in comments**. Without the strip,
that anchor resolves and the lint silently passes the single defect the initiative
most wants caught. `-NoCommentStrip` exists **only** to demonstrate that false
negative; the Pester spec pins it.

## Escapes — a lint reads names, not sentences

A document that says *"`m_isResimulated` was retired at item 45"* is correct
**precisely because** the name does not resolve. Three markers declare that:

```markdown
<!-- lint-external-ref: TOKEN -- reason -->            (document-scoped, one token)
<!-- lint-anchor-ignore: reason -->                    (the line it appears on)
<!-- lint-anchor-ignore-begin: reason --> … <!-- lint-anchor-ignore-end -->
```

**Every escape is enumerated with its reason in the output**, and **an escape that
suppresses nothing is itself a violation** — a pattern matching nothing passes every
check, and a stale exemption must not rot quietly. The escape is an attack surface
by construction (a lazy author can silence a real hit with it), which is why the
reasons are printed rather than merely honoured.

**Contract:** a doc that gains an intentionally unresolvable reference gains its
declaration in the same change, and the reason says *why* it cannot resolve.

## Known limitations (intentional)

- **It checks that a name EXISTS; it never checks that a sentence is TRUE.** A
  resolvable anchor on a false claim passes. Live instance on 2026-08-21:
  `SimulationManager.h:26-37`, every symbol resolving, the sentence wrong.
- It cannot see an **omission**, a doc **contradicting itself**, or a move that
  silently **edited** what it copied.
- **String literals are not stripped**, only comments — a symbol surviving only
  inside a log format string resolves.
- A `file:NNN` anchor is checked for **range, not content**: `:371` proves the file
  has 371 lines, not that line 371 still says what the doc claims.
- **A dotted name with a lowercase segment stays UNCHECKED.** `storage.add`,
  `decision.landed`, `m_reconciliation.findInputCache` — 25 distinct such tokens are
  live in the two tiers today. Naming a method through its owner is therefore *not*
  a checked claim; cite the method on its own, or as `Owner::method`, to get one.
- **The FILE arm's extension set is `h hpp cpp ini cs md yml disabled`.** A backticked
  `` `doc_anchor_lint.ps1` `` or `` `CMakeLists.txt` `` matches no arm and is UNCHECKED,
  so a renamed script or build file in a doc is not caught. Both of this directory's
  own lint scripts are cited that way today.

## Tests

`tools/lint/tests/doc_anchor_lint.Tests.ps1` (Pester 5, pattern per
`tools/og-tools/tests/*.Tests.ps1`). Every context pairs a must-be-silent case with
a must-fire case over the same fixture, so the lint's ability to fail is a test
rather than a claim. The known-bad fixture seeds four real historical defects.

```pwsh
pwsh -NoProfile -Command "Invoke-Pester tools/lint/tests -Output Detailed"
```

---

# Palette-legend lint

`palette_legend_lint.ps1` checks that
`docs/InputHistoryDisplay-rationale.md` §7.11's colour-legend tables agree with the
`switch` arms that actually bind a lane colour to its meaning —
`provenanceCellStyleOf`, `machineCellStyleOf` and `delayVerdictStyleOf` in
`BrawlerInputHistoryVisualizationBars.h`.

> **The rule:** those three switches are the ONLY place a name is bound to an RGB
> triple. Nothing else in the tree maps a colour on the frame-meter bars to what it
> means, so §7.11's tables are the human-readable copy of that binding — and a
> hand-typed copy of tuned colour values goes stale silently the first time anyone
> retunes a hue.

```pwsh
pwsh tools/lint/palette_legend_lint.ps1
```

It fails on: an enumerator with a switch arm but no table row; a table row naming an
enumerator no switch has an arm for; an RGB mismatch; a `LaneCellFill` mismatch (a
hole documented as a colour, or a colour documented as a hole); and — new — a mismatch
between a row's `On screen (sRGB hex)` cell and the hex re-derived from that same row's
own RGB floats. Each of the three enumerator tables' row COUNT is checked against that
enumerator's own count constant — `kRowProvenanceSummaryCount`, `kMachineStateCellCount`,
`kInputDelayVerdictCount`, read from their declaring headers, never a literal — so an
enumerator added without a table row fails the same way the existing palette
distinctness sweeps in the Catch2 suite do. A fourth table covers `kLaneElisionColor`
and `kUnnamedLaneColor`, the two colours drawn on the bars that belong to no enumerator
at all; they are checked by value (RGB and hex) but deliberately not counted against
any of the three constants.

**The `On screen (sRGB hex)` column and the `-EmitHex` mode.** §7.11's RGB floats are
LINEAR, not what the screen shows — `meterCellColor` builds an `FLinearColor` straight
from them and lets Unreal gamma-encode it for display, so reading a float as an sRGB
byte gives a materially wrong, always-darker colour. The hex column is the byte value
that actually renders, computed per channel by `s = 12.92·L` when `L ≤ 0.0031308`, else
`s = 1.055·L^(1/2.4) − 0.055`, then `round(s × 255)` clamped to `0..255`. Run

```pwsh
pwsh tools/lint/palette_legend_lint.ps1 -EmitHex
```

to print the GENERATED hex for every switch arm and non-enumerator colour, computed
from `-BarsHeaderPath`'s own floats — this is the source a human pastes the column
FROM; it reads only the header, never the doc, and checks nothing. The ordinary
(non-`-EmitHex`) run then independently re-derives the same hex from the same floats
and compares it against what the doc has written down. Both paths call the SAME
`ConvertTo-SRGB8` / `ConvertTo-SRGBHex` functions — there is exactly one implementation
of the conversion, so it cannot drift from itself. Rounding is round-half-AWAY-FROM-ZERO
(`floor(x + 0.5)`), pinned explicitly because PowerShell's `[math]::Round` defaults to
banker's rounding (half-to-even) instead.

| Parameter          | Default                                                  | Purpose |
| ------------------- | --------------------------------------------------------- | ------- |
| `-RepoRoot`         | two levels above the script                                | Root relative paths resolve against. |
| `-BarsHeaderPath`   | `BrawlerInputHistoryVisualizationBars.h`                    | The header carrying the three switches and the two non-enumerator colours. |
| `-CoreHeaderPath`   | `BrawlerInputHistoryVisualization.h`                        | Declares `kRowProvenanceSummaryCount`. |
| `-LanesHeaderPath`  | `BrawlerInputHistoryVisualizationLanes.h`                   | Declares `kMachineStateCellCount` and `kInputDelayVerdictCount`. |
| `-DocPath`          | `Source/OGBrawlerUnreal/docs/InputHistoryDisplay-rationale.md` | The doc carrying §7.11's four tables. |
| `-EmitHex`          | off                                                        | Prints the generated hex table from the header and exits 0; never opens `-DocPath`'s content. |

Every path is an ordinary parameter (relative paths resolve against `-RepoRoot`; an
absolute path is used as-is), which is what lets a RED probe point the lint at a
SCRATCH COPY without ever seeding a defect in place on a shipped file.

**What this lint does NOT check.** It can verify that a name, an RGB triple, the
derived sRGB hex and a fill kind agree between the header and §7.11. It **cannot**
verify that a row's MEANING sentence, or its plain-English `Colour` word, are true —
both are prose, and nothing here reads either of them. A clean run is not a claim that
any description or colour name in §7.11 is accurate, only that the names, RGB values,
hex and fill kinds it can parse agree with the shipped header. §7.11 states this
plainly too, so a green lint is never read as blessing more than it establishes.

Exit codes: **0** = clean (or, with `-EmitHex`, the generated table printed) · **1** =
at least one mismatch · **2** = usage/IO error (a configured path does not exist, or a
table the lint needs is missing from the doc entirely — never a silent zero).

## Tests

`tools/lint/tests/palette_legend_lint.Tests.ps1` (Pester 5, same shape as
`doc_anchor_lint.Tests.ps1`): a clean fixture pair (a small header + a matching §7.11
fixture, both carrying the hex and Colour columns) alongside the RED mutations this
lint's acceptance criteria name — a perturbed RGB channel, a deleted table row, a table
row naming no arm, the `NoVerdict` hole documented as a colour, a perturbed hex digit,
a deleted hex column, and a linear float changed without regenerating its hex (the
drift case the hex arm exists for) — plus two usage-error cases, an `-EmitHex` output
check, and a round-trip test proving the emitter and the checker call the same
conversion function. Fixtures only; never a shipped file.

```pwsh
pwsh -NoProfile -Command "Invoke-Pester tools/lint/tests/palette_legend_lint.Tests.ps1 -Output Detailed"
```

---

# Bandwidth measurement (`measure_bandwidth.ps1`)

`measure_bandwidth.ps1` is the Phase 2a (`og-netcode-v1-impl` Task 19) measurement
harness — a sibling of `configurability_lint.ps1` in the same `tools/lint/` tree
(parent repo, not a submodule). It drives **one** `MaxClientRate` cap-ramp run at
one of the four T18-locked product-target topologies, on the **post-Stage-2 60 Hz
runtime**, and captures the engine network/timing stats for that run.

> Requires **PowerShell 7+** (`pwsh`) and a built UE 5.6 editor at `C:\dev\UnrealEngine`
> (override with `-EngineDir`). It launches `UnrealEditor.exe -game` clients against
> a `UnrealEditor.exe -server`, the same launch model as `playtest_client.bat` /
> `playtest_server.bat`.

## What the script does

```pwsh
pwsh tools/lint/measure_bandwidth.ps1 -MaxClientRate 100000 -Topology 1x3 `
    -OutputPath impl/ramp_100000_1x3.txt -DurationSec 60
```

| Parameter        | Required | Default              | Purpose |
| ---------------- | -------- | -------------------- | ------- |
| `-MaxClientRate` | yes      | —                    | Per-client server-side cap (B/s) written ephemerally to `[/Script/OnlineSubsystemUtils.IpNetDriver]` (the project's concrete net-driver class — see binding-section note). |
| `-Topology`      | yes      | —                    | One of `1x3`, `3x1`, `2x3`, `6x1` (validated set). See matrix below. |
| `-OutputPath`    | yes      | —                    | File the run capture is written to (parent dir must exist). |
| `-NetworkProfile`| no       | `none`               | `none` = no link emulation (T19 behaviour). `cellular` = prepend UE net-emulation to the client `-ExecCmds` (see "Cellular profile" below). |
| `-DurationSec`   | no       | `60`                 | Seconds to hold under load before teardown. |
| `-RepoRoot`      | no       | two levels above script | Repo root used to resolve the INI / project / `Saved/`. |
| `-EngineDir`     | no       | `C:\dev\UnrealEngine`| UE 5.6 source-build root. |
| `-Port`          | no       | `7777`               | Dedicated-server UDP port. |

Run flow: snapshot the INI → ephemeral edit → launch 1 dedicated server +
*clients* clients (each spawning *LPs* local players) → hold `DurationSec` → tear
the processes down → assemble the capture file → **restore the INI (always)**.

Extra local players per client are added via `JoinLocalPlayer` — the
`UFUNCTION(Exec)` on `AOGBrawlerPlayerController` from the
OGBrawlerMultiPlayerPerClient initiative — issued `LPs - 1` times through the
client's startup `-ExecCmds`. (The generic engine `debug.CreatePlayer` is **not**
used; `OGBrawlerUEGameMode` force-disables splitscreen, which no-ops it.)

Exit codes: **0** = run done + INI restored & verified · **1** = run error (INI
still restored in `finally`) · **2** = usage/IO error *or* — critically — restore
verification failed and the `.bak` was left for manual recovery.

## `-Topology` argument matrix (the four T18-locked scenarios)

These are the **fixed scenario set** locked in T18 §1 — do not vary them.

| `-Topology` | clients × LPs | Tier | Stress axis |
| ----------- | ------------- | ---- | ----------- |
| `1x3` | 1 client × 3 LPs   | Tier 1 | densest (one connection carries all 3 chars) |
| `3x1` | 3 clients × 1 LP   | Tier 1 | most-connections |
| `2x3` | 2 clients × 3 LPs  | Tier 2 | densest |
| `6x1` | 6 clients × 1 LP   | Tier 2 | most-connections |

T19 measures **6 caps × 4 topologies = 24 runs** at 60 Hz LAN; T20 extends the same
script with `-NetworkProfile cellular` for the cellular matrix.

## Cellular profile (`-NetworkProfile cellular`) — T20

`-NetworkProfile cellular` measures the same cap × topology matrix under an emulated
Android **cellular** link instead of the LAN profile. When set, the script **prepends**
UE's built-in network-emulation console commands to **every client's** `-ExecCmds`
(ahead of the `JoinLocalPlayer` / `open` commands), leaving the dedicated server
un-emulated:

```text
NetEmulation.PktLag 150, NetEmulation.PktLagVariance 30, NetEmulation.PktLoss 5, NetEmulation.PktIncomingLoss 5
```

| `NetEmulation.*` command | Value | Direction | Meaning (UE units)            | C.2 tier-3 cellular |
| ------------------------ | ----- | --------- | ----------------------------- | ------------------- |
| `PktLag`                 | 150   | outgoing  | one-way added latency, **ms** | ~150 ms RTT (R-A1)  |
| `PktLagVariance`         | 30    | outgoing  | latency jitter, **ms**        | 30 ms jitter (R-A2) |
| `PktLoss`                | 5     | outgoing  | uplink packet drop, **%**     | 5 % loss (R-A3)     |
| `PktIncomingLoss`        | 5     | incoming  | downlink packet drop, **%**   | 5 % loss (R-A3)     |

These numbers are the **C.2 tier-3 cellular profile** per
`OGBrawlerNetworkModelResearch/arch/risks_and_plan.md §1` (R-A1 = 150 ms RTT,
R-A2 = 30 ms jitter, R-A3 = 5 % packet loss).

> **⚠ Two mechanism corrections vs the Backlog literal `Net PktLag=150 ...`** (validated
> empirically in T20 — see `impl/impl_notes_phase2a_task20.md`):
>
> 1. **`NetEmulation.*`, not `Net Pkt*`.** The UE4-era `Net Pkt*` exec form routes through
>    `UNetDriver::Exec` and needs a *live* net driver; it fires from the client's frame-1
>    `-ExecCmds` ~30 frames before the connection exists (and the multi-LP densest
>    topologies boot *standalone*, with no client net driver until the `open` travel), so it
>    **silently no-ops** (a validation `1x3` run with the literal form read 0 % loss / 10 ms
>    ping, identical to LAN). The modern `NetEmulation.*` commands store into the engine-global
>    **`PersistentPacketSimulationSettings`** (`DO_ENABLE_NET_TEST` dev builds), which
>    `UNetDriver` re-applies to every connection created later — so emulation set at frame 1
>    survives the standalone→`open` travel.
> 2. **`PktIncomingLoss` added for the downlink.** `PktLag`/`PktLoss` are *outgoing-only*
>    (the connection send path), i.e. on a client they degrade only the **uplink**. The task
>    measures the cellular **downstream** ceiling (the bandwidth-heavy server→client
>    broadcast), so a 4th knob `PktIncomingLoss 5` degrades the **downlink** by 5 % too. Lag
>    is kept one-way (RTT ≈ 150 ms per R-A1, not doubled); loss is applied to **both**
>    directions at 5 % each (R-A3) — one bad cellular link modelled entirely client-side.

Because emulation runs **client-side**, the client CSV's `Replication/InPacketsLost` /
`InLostPacketsFoundPerFrame` counters become meaningful (they read ~0 on the lossless LAN
profile), and the **client-received** downstream (`Replication/InRate`) can be compared
against the **lossless LAN demand** (the T19 plateau at the same cap × topology, i.e. what
the server attempts to send) — the gap is the cellular link's downlink drop.

The cellular matrix is **5 caps × 4 topologies = 20 runs**
(caps `{15000, 60000, 120000, 250000, 500000}` — `30000` is dropped vs T19's six;
interpolation is sufficient at that low end). Output capture files are named
`impl/ramp_<cap>_<topology>_cellular.txt`. The ephemeral-cap edit + SHA256 restore core
is **identical** to the LAN profile — `-NetworkProfile` only changes the client
`-ExecCmds`, never the `DefaultEngine.ini` edit/restore path.

## Restore-at-end discipline (the safety contract)

The script makes the **only** `DefaultEngine.ini` state change internal to itself
and reverts it before returning:

1. **Before any edit**, the original file's SHA256 is captured into a script-local
   variable **and** the verbatim original bytes are written to
   `Config/DefaultEngine.ini.measure_bandwidth.bak`. The backup is itself
   hash-checked against the original before proceeding.
2. The `[/Script/OnlineSubsystemUtils.IpNetDriver]` section is **appended** with the
   cap (the project ships no such section — see
   `research/spike_max_client_rate_ceiling.md` §4). If a section already exists the
   script aborts rather than guess at merge semantics.
   **⚠ Binding-section correction (T19):** the cap MUST go in the `IpNetDriver`
   class section, NOT the `[/Script/Engine.GameNetDriver]` *DefName* section the
   Backlog literal named — the latter is inert (there is no `UGameNetDriver` class),
   so a cap written there does not bind (proven: a 15000-cap run still pushed
   ~40 KB/s). The concrete driver is `UIpNetDriver`, which reads `MaxClientRate`
   from its own class section.
3. The run happens inside a `try`.
4. A `finally` block (runs on success, PIE crash, Ctrl-C, or any throw) copies the
   `.bak` back over the INI and recomputes the SHA256:
   - **match** → the `.bak` is deleted; the tree is provably clean.
   - **mismatch** → the script **aborts loudly (exit 2) and LEAVES the `.bak` in
     place** for manual recovery — it never deletes a backup it could not verify.

## ⚠ Measurement-mechanism note (read before trusting the capture)

`stat net` and `stat unit` render to the **in-viewport overlay** — they do **not**
print their KB/s / frame-ms numbers to stdout or the log. The dispatch-prescribed
`-ExecCmds="stat unit, stat net"` is still issued so the overlay is on screen for
manual observation, but to make the capture file carry real numbers the script also:

- enables `-LogCmds="LogNet Verbose, LogNetTraffic Verbose"` (per-bunch byte counts
  land in the log/stdout), and
- starts the **CSV profiler** (`csvprofile start`), whose Net category emits
  parseable `InBytes`/`OutBytes`/`Ping` columns to `Saved/Profiling/CSV/*.csv` (the
  capture header points at that directory).

The per-client downstream/upstream/server-aggregate KB/s for the ramp tables are
extracted from the **CSV profiler Net columns**, not from the `stat net` overlay.
This is flagged in `impl/impl_notes_phase2a_task19.md` for user review before the
full 24-run matrix is authorised.

## ⚠ No-git rule (initiative-binding)

`measure_bandwidth.ps1` edits `Config/DefaultEngine.ini` **ephemerally** and restores
it. **Implementers MUST NOT commit any `Config/DefaultEngine.ini` change** — the file
must be in its pre-task state (and the `.bak` gone) at end of task. Per the initiative
git policy, the user owns all state-changing git operations; agents only modify the
working tree. If a run aborts and leaves a `.bak`, recover the INI from it manually and
do **not** stage either file.
