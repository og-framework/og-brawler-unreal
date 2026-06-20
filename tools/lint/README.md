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

There is **no active CI** wiring yet. `.github/workflows/standalone-tests.yml` is
parked as `.disabled` because the default `GITHUB_TOKEN` cannot fetch the private
sibling submodules (see that file's header). Until that is resolved, this lint runs
**manually** / as a local pre-commit step — run it before any PR that touches
`TimeConfig` or its consumers.

When CI is re-enabled (a later stage), add a step **before** the build (it needs no
submodules or compiler — just the source tree):

```yaml
      - name: Configurability lint (R-P1)
        shell: pwsh
        run: pwsh tools/lint/configurability_lint.ps1
```

`windows-latest` ships PowerShell 7 (`pwsh`) by default. On a Linux runner, install
PowerShell first or run via the `mcr.microsoft.com/powershell` container. The step
fails the job on exit code `1`, surfacing the offending `file:line` in the CI log.
