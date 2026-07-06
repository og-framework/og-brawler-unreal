#Requires -Version 7.0
# SPDX-License-Identifier: MPL-2.0
<#
.SYNOPSIS
    R-P1 configurability lint (Synthesis Addendum Correction 5 binding rule).

.DESCRIPTION
    Scans the C++ source tree for TimeConfig-governed tuning values that have been
    re-declared (a second source of truth) outside their TimeConfig home. Per
    proposal §11 + risks_and_plan §6.1, every tuning parameter named in the
    synthesis MUST live as a `TimeConfig` field and be read from there at runtime.
    This lint is the structural enforcement arm of that rule; the Catch2
    default-drift gate (TimeConfigDefaultsTest.cpp, Task 6) is the complementary
    runtime arm.

    DESIGN — "field-anchored" matching (user-approved 2026-06-20; rationale and the
    rejected alternatives are documented in tools/lint/README.md "Design decision"):
      A violation is a config FIELD NAME used as a NON-member lvalue assigned its
      magic literal, e.g.

          int32_t rollbackWindowTicks = 12;   // FLAGGED — second source of truth
          redundancyDepthTicks        = 5;     // FLAGGED

      Legitimate USES of the config are NOT flagged, because they go through a
      member access on a TimeConfig instance, which a negative lookbehind excludes:

          cfg.tickFrequency            = 60.0; // OK — configuring an instance
          config.rollbackWindowTicks   = ...;  // OK
          if (drift <= m_config.hardResyncThresholdTicks) ... // OK

    To keep the false-positive rate at zero this lint also strips // line comments,
    /* block */ comments, and "string"/'char' literals before matching, so docs or
    log strings that mention a field name or "100 Hz" are never flagged.

    KNOWN LIMITATION (by design): a keyword-free magic number — e.g. `if (depth > 12)`
    or `int32 dummy = 12;` — is NOT flagged. A bare-value lint cannot run clean on
    this tree (12/20/60/100/5/3 appear constantly in unrelated code: LatSegs = 12,
    MaxWalkSpeed = 100.f, StateBufferSize = 60, Jolt geometry, ...). Those cases are
    covered by code review and the default-drift gate. See README for the full
    discussion.

    THE BLACKLIST IS A CONTRACT: every PR that adds/changes a TimeConfig field MUST
    add/update a matching entry here in the same change. See tools/lint/README.md.

.PARAMETER RepoRoot
    Repository root. Defaults to two levels above this script (tools/lint/..).

.PARAMETER ScanRoots
    Top-level directories to scan (relative to RepoRoot). Defaults to Plugins, Source.

.EXAMPLE
    pwsh tools/lint/configurability_lint.ps1

.OUTPUTS
    Exit 0 = clean. Exit 1 = at least one violation. Exit 2 = usage / IO error.
#>
[CmdletBinding()]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..' '..')).Path,
    [string[]]$ScanRoots = @('Plugins', 'Source')
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# CONFIG_LITERAL_BLACKLIST
#
# Schema — one hashtable per TimeConfig field that has a synthesis-named default:
#   Field   : the TimeConfig field this entry guards (for humans / messages).
#   Pattern : a .NET regex applied to each comment/string-stripped source line.
#             Field-anchored: (?<![\w.>]) excludes member access (foo.field /
#             foo->field) and substrings (myfield); '=' (not '==') matches a
#             declaration/assignment, not a comparison.
#   Allowed : file LEAF names (case-insensitive) where the declaration is the
#             legitimate source of truth. Empty => flagged everywhere.
#   Message : printed on a hit; names the field and references R-P1.
#   Note    : (optional) rationale for any non-obvious Allowed entry.
#
# CONTRACT: adding a TimeConfig field => add an entry here in the same PR.
# ---------------------------------------------------------------------------
$Blacklist = @(
    @{
        Field   = 'predOffsetFloorTicks'
        # Netcode Fix A (Task 23): minimum floor for getPredictionOffsetTicks().
        # TimeConfig.h is the source of truth; TimeConfigDefaultsTest.cpp asserts
        # the default. NetworkTimeEstimator.cpp consumes it via member access
        # (m_config.predOffsetFloorTicks), which the (?<![\w.>]) lookbehind auto-excludes.
        Pattern = '(?<![\w.>])predOffsetFloorTicks\s*=\s*4\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'predOffsetFloorTicks default (4) must live only in TimeConfig; read config.predOffsetFloorTicks instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'rollbackWindowTicks'
        Pattern = '(?<![\w.>])rollbackWindowTicks\s*=\s*12\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'rollbackWindowTicks default (12) must live only in TimeConfig; read config.rollbackWindowTicks instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'rollbackWindowHardCap'
        Pattern = '(?<![\w.>])rollbackWindowHardCap\s*=\s*20\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'rollbackWindowHardCap default (20) must live only in TimeConfig; read config.rollbackWindowHardCap instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'redundancyDepthTicks'
        # Catch the field and any sibling redundancyDepth* spelling assigned 3 or 5.
        Pattern = '(?<![\w.>])redundancyDepth\w*\s*=\s*[35]\b'
        # T7 adds the FInputRedundancyBundle header; allow both candidate basenames
        # (a dedicated header vs. SyncedSimulationStateBuffer.h) until T7 fixes the name.
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp', 'InputRedundancyBundle.h', 'FInputRedundancyBundle.h')
        Message = 'redundancyDepthTicks default (5 @100Hz interim / 3 @60Hz target) must live only in TimeConfig; read config.redundancyDepthTicks instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'tickFrequency'
        Pattern = '(?<![\w.>])tickFrequency\s*=\s*(60|100)\b'
        # NetworkTimeEstimator.cpp legitimately consumes the configured value. The
        # async-physics derive site (config.tickFrequency = 1.0/GetAsyncDeltaTime())
        # is member-access and auto-excluded; T14's Stage 2 audit adds its owning file
        # here only if that audit surfaces a literal Hz constant.
        Allowed = @('TimeConfig.h', 'NetworkTimeEstimator.cpp')
        Message = 'tickFrequency (60 ratified target / 100 interim) must be configured via TimeConfig and derived from solver->GetAsyncDeltaTime(); do not re-declare the Hz literal (R-P1)'
    },
    @{
        Field   = 'hardResyncThresholdTicks'
        # Old-value-must-not-reappear guard: bumped 15 -> 21 for R-D3 (failsafe must
        # stay strictly > rollbackWindowHardCap=20). Allowed is EMPTY so a revert to
        # 15 is caught even inside TimeConfig.h. Test fixtures that set
        # `cfg.hardResyncThresholdTicks = 15` are member-access and auto-excluded.
        Pattern = '(?<![\w.>])hardResyncThresholdTicks\s*=\s*15\b'
        Allowed = @()
        Message = 'hardResyncThresholdTicks old default 15 must not reappear — current default is 21 (R-P1 / R-D3)'
    },

    # -----------------------------------------------------------------------
    # Product-target session constants (og-netcode-v1-impl Phase 2a, Task 18).
    #
    # NOT TimeConfig fields — these live in the og-brawler core header
    # SessionConstants.h (namespace og::brawler::session) and define the ratified
    # player-count envelope (Tier 1 = 3, Tier 2 = 6, up to 3 LPs/client). Same
    # "single source of truth" rule as the TimeConfig entries above: the magic
    # 3/6 player-count literals must be read from SessionConstants.h, never
    # re-declared in game/UE/test code. Member-access reads
    # (og::brawler::session::maxPlayersPerServer) and qualified uses are auto-
    # excluded by the (?<![\w.>]) lookbehind; only a non-member lvalue assigned
    # the magic literal is flagged. CONTRACT: a future server-session gate that
    # legitimately re-houses one of these values adds its file leaf to Allowed in
    # the same change. A test fixture that exercises a literal cap adds itself too.
    # -----------------------------------------------------------------------
    @{
        Field   = 'maxPlayersPerServer'
        Pattern = '(?<![\w.>])maxPlayersPerServer\s*=\s*3\b'
        Allowed = @('SessionConstants.h')
        Message = 'maxPlayersPerServer Tier 1 default (3) must live only in SessionConstants.h; read og::brawler::session::maxPlayersPerServer instead of re-declaring it (R-P1 / Phase 2a)'
    },
    @{
        Field   = 'maxPlayersPerServerTier2'
        Pattern = '(?<![\w.>])maxPlayersPerServerTier2\s*=\s*6\b'
        Allowed = @('SessionConstants.h')
        Message = 'maxPlayersPerServerTier2 Tier 2 stretch target (6) must live only in SessionConstants.h; read og::brawler::session::maxPlayersPerServerTier2 instead of re-declaring it (R-P1 / Phase 2a)'
    },
    @{
        Field   = 'maxLocalPlayersPerClient'
        Pattern = '(?<![\w.>])maxLocalPlayersPerClient\s*=\s*3\b'
        Allowed = @('SessionConstants.h')
        Message = 'maxLocalPlayersPerClient (3 LPs/client) must live only in SessionConstants.h; read og::brawler::session::maxLocalPlayersPerClient instead of re-declaring it (R-P1 / Phase 2a)'
    },
    @{
        # Generic catch for hardcoded player-count caps that DON'T use the
        # SessionConstants names above (e.g. player_count = 3, maxPlayers = 6).
        # Overshoot is intentional per the Task 18 spec — any false positive is
        # surfaced for manual review rather than silently passing. The name
        # alternation does NOT match the SessionConstants fields (they have
        # suffixes after 'maxPlayers'), so the source-of-truth header is not hit.
        Field   = 'player-count literal (generic)'
        Pattern = '(?<![\w.>])(player_count|playerCount|PlayerCount|maxPlayers|numPlayers|NumPlayers)\s*[=:]\s*[36]\b'
        Allowed = @('SessionConstants.h')
        Message = 'hardcoded player-count literal (3 = Tier 1, 6 = Tier 2) must read from og::brawler::session in SessionConstants.h, not a magic 3/6 (R-P1 / Phase 2a)'
    },

    # -----------------------------------------------------------------------
    # Projectile pool size (OGBrawlerHadouken Phase 2, Task 12 — R-P1 config move).
    #
    # NOT a TimeConfig field — the runtime pool size lives in the og-brawler core
    # header BrawlerProjectileSimulation.h as brawlerProjectileSimulation::StaticData
    # ::projectilePoolSize (default 3), the single wire-affecting sizing knob read at
    # runtime via sd.projectilePoolSize. The compile-time CAPACITY constant
    # kMaxProjectilePoolSize (also 3) is intentionally NOT guarded here — the R-P1 lint
    # guards tuning values, not template-instantiation/array/SIM_VECTOR capacities.
    # Member-access reads (sd.projectilePoolSize) and the runtime-cap test fixture
    # (sd.projectilePoolSize = 1) are auto-excluded by the (?<![\w.>]) lookbehind and the
    # value anchor; only a non-member lvalue assigned the magic default 3 is flagged.
    # CONTRACT: if T15 re-houses this value onto simulatableBrawler::StaticData under a
    # different name, that file leaf is added to Allowed in the same change.
    # -----------------------------------------------------------------------
    @{
        Field   = 'projectilePoolSize'
        Pattern = '(?<![\w.>])projectilePoolSize\s*=\s*3\b'
        Allowed = @('BrawlerProjectileSimulation.h')
        Message = 'projectilePoolSize default (3) must live only in BrawlerProjectileSimulation.h StaticData; read sd.projectilePoolSize instead of re-declaring it (R-P1)'
    }
)

# ---------------------------------------------------------------------------
# Path exclusions: build output, engine intermediate, and vendored third-party.
# Task 5 spec names Binaries / Intermediate / extern/*/build* / glm; ThirdParty
# (vendored Jolt) is excluded on the same "vendored code" rationale as glm.
# ---------------------------------------------------------------------------
$ExcludeRegex = '[\\/](Binaries|Intermediate|ThirdParty|glm|\.git|\.vs)[\\/]|[\\/]build([\\/_]|$)'

# ---------------------------------------------------------------------------
# Strip // comments, /* */ comments (incl. multi-line), and string/char literals
# from a single physical line. $InBlock carries block-comment state across lines.
# ---------------------------------------------------------------------------
function Remove-CommentsAndStrings {
    param(
        [string]$Line,
        [ref]$InBlock
    )

    $s = $Line

    if ($InBlock.Value) {
        $idx = $s.IndexOf('*/')
        if ($idx -ge 0) {
            $s = $s.Substring($idx + 2)
            $InBlock.Value = $false
        }
        else {
            return ''
        }
    }

    # Collapse inline /* ... */ blocks; open-ended /* flips block state.
    while ($true) {
        $open = $s.IndexOf('/*')
        if ($open -lt 0) { break }
        $close = $s.IndexOf('*/', $open + 2)
        if ($close -ge 0) {
            $s = $s.Substring(0, $open) + ' ' + $s.Substring($close + 2)
        }
        else {
            $s = $s.Substring(0, $open)
            $InBlock.Value = $true
            break
        }
    }

    # Blank out string and char literals (so "tick=60" or '1' never match).
    $s = [regex]::Replace($s, '"(\\.|[^"\\])*"', '""')
    $s = [regex]::Replace($s, "'(\\.|[^'\\])*'", "''")

    # Drop trailing // line comment.
    $li = $s.IndexOf('//')
    if ($li -ge 0) {
        $s = $s.Substring(0, $li)
    }

    return $s
}

# ---------------------------------------------------------------------------
# Main scan
# ---------------------------------------------------------------------------
if (-not (Test-Path -LiteralPath $RepoRoot)) {
    Write-Error "RepoRoot not found: $RepoRoot"
    exit 2
}

$files = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
foreach ($root in $ScanRoots) {
    $full = Join-Path $RepoRoot $root
    if (Test-Path $full) {
        Get-ChildItem -Path $full -Recurse -File -Include *.h, *.cpp |
            ForEach-Object { $files.Add($_) }
    }
}

$scanned = 0
$violations = [System.Collections.Generic.List[object]]::new()

foreach ($f in $files) {
    if ($f.FullName -match $ExcludeRegex) { continue }
    $scanned++
    $base = $f.Name

    # Pre-compute which blacklist entries are active for this file.
    $active = $Blacklist | Where-Object { $_.Allowed -notcontains $base }
    if ($active.Count -eq 0) { continue }

    $inBlock = $false
    $lineNo = 0
    foreach ($line in [System.IO.File]::ReadLines($f.FullName)) {
        $lineNo++
        $code = Remove-CommentsAndStrings $line ([ref]$inBlock)
        if ([string]::IsNullOrWhiteSpace($code)) { continue }
        foreach ($entry in $active) {
            if ($code -match $entry.Pattern) {
                $rel = [System.IO.Path]::GetRelativePath($RepoRoot, $f.FullName)
                $violations.Add([pscustomobject]@{
                    File    = $rel
                    Line    = $lineNo
                    Field   = $entry.Field
                    Message = $entry.Message
                })
            }
        }
    }
}

# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------
if ($violations.Count -gt 0) {
    foreach ($v in $violations) {
        Write-Host ("{0}:{1}: {2} (Synthesis Addendum Correction 5 binding rule)" -f `
            $v.File, $v.Line, $v.Message)
    }
    Write-Host ''
    Write-Host ("Configurability lint: FAILED ({0} violation(s) across {1} files scanned)" -f `
        $violations.Count, $scanned)
    Write-Host 'See tools/lint/README.md — every TimeConfig value must be read from TimeConfig, not re-declared.'
    exit 1
}
else {
    Write-Host ("Configurability lint: clean ({0} files scanned)" -f $scanned)
    exit 0
}
