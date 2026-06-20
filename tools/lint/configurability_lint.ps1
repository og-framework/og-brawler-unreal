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
