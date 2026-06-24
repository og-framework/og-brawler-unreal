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
