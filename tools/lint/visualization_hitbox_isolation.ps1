#Requires -Version 7.0
# SPDX-License-Identifier: MPL-2.0
<#
.SYNOPSIS
    R-UE1 visualization/hitbox isolation lint (Stage 5 deliverable D5.5).

.DESCRIPTION
    Structural enforcement of the mesh-only invariant documented in
    Plugins/OGBrawler/Source/OGBrawler/og-brawler/OGBrawler/VISUALIZATION_DISCIPLINE.md.

    The invariant: visualization code is STRUCTURALLY prevented from affecting sim
    state or hitbox calculation. The `visualize(...)` signature enforces the easy
    case by parameter types (sim-side parameters are `const&`; the only mutable
    parameter is the visualization `State&`). Per risks_and_plan R-UE1, the
    signature catches the easy case but NOT the side-channel case — a visualization
    component that reaches into the hit-resolution system directly, via a static, or
    via a service call, leaks visual-blended state into hitbox math despite the
    signature being clean.

    This lint is the automated arm of that mitigation: it fails CI if any
    visualization translation unit `#include`s a hitbox-RESOLUTION header. The
    complementary arms are the code-review checklist in VISUALIZATION_DISCIPLINE.md
    (side channels a grep cannot see) and the `const&` signature itself.

    SCOPE — BOTH halves of the invariant, engine-free and UE-side. The visualization
    surface spans two trees: the engine-free free functions in og-brawler's core, and
    the `Source/OGBrawlerUnreal` units that render them (a debug-draw helper, the
    input-history ring store, and `AOGBrawlerUEHUD`, the project's only screen-space
    drawing surface). Scanning the core alone left every UE-side unit unguarded while
    still reporting clean, so a green run said nothing about the UE half. Both roots
    are scanned; see the file-collection section for the root and glob contract.

    DESIGN — what counts as a "hitbox-resolution" header (the scoping decision):

      FLAGGED: headers that RESOLVE hits — that decide whether a hit landed and
      write the resulting derived state:
        BrawlerHitRouting*.h  (routeInbound / the hit-routing system; sets
                               wasHitThisTick on each character's derived state)
        BrawlerInboundHit.h   (the inbound-hit derived-state payload that routing
                               produces — the OUTPUT of hit resolution)

      NOT FLAGGED: sim-state TYPE headers that visualization legitimately reads
      through a `const&` parameter:
        DAttackMachineSimulation.h, DAttackRadialSimulation.h,
        DAttackGuardSimulation.h, BrawlerProjectileSimulation.h

      This distinction is load-bearing and deliberate. Three shipping visualization
      headers (DAttackTargetVisualization.h, DAttackTargetVisualizationTwo.h,
      DAttackVisualizationUtils.h) ALREADY include DAttackMachineSimulation.h to
      name the `const dAttackMachineSimulation::State&` parameter they render from.
      That is precisely the behaviour the invariant PERMITS — reading post-sim state
      immutably. Blacklisting every header matching the task's suggested discovery
      probe (grep -rln "wasHitThisTick|routeInbound") would flag those legitimate
      const reads and the lint could not run clean on the current tree. The rule the
      invariant actually needs is "visualization must not reach the hit RESOLVER",
      not "visualization must not name a sim type".

    Matching is on the include's LEAF filename, so every path spelling of the same
    header is caught:
        #include "BrawlerHitRoutingSystem.h"                                  FLAGGED
        #include "OGBrawler/BrawlerHitRoutingSystem.h"                        FLAGGED
        #include "Plugins/OGBrawler/Source/OGBrawler/BrawlerHitRouting.h"     FLAGGED
    Both quoted and angled include forms are matched. Comments and string literals
    are stripped before matching (shared approach with configurability_lint.ps1), so
    a commented-out include or a doc mention of a header name is never flagged.

    KNOWN LIMITATION (by design, and the reason the review checklist exists): this
    lint sees DIRECT INCLUDES ONLY. Two things are therefore invisible to it. First,
    a side channel that needs no include — a static/global a sim-side component later
    reads, a singleton lookup, a template parameter bound to a hit-resolving functor
    at the call site. Second, a TRANSITIVE reach: leaf matching runs over the text of
    each scanned file, so a scanned file that includes a header which itself includes
    a blacklisted one is not flagged, and cannot be. Both are the cases
    VISUALIZATION_DISCIPLINE.md's checklist exists to catch in review; §4's transitive
    ruling records the one such reach that exists in the tree today. A green run of
    this lint is NOT proof the invariant holds; it is proof the cheapest way to break
    it is blocked.

.PARAMETER RepoRoot
    Repository root. Defaults to two levels above this script (tools/lint/..).

.PARAMETER VisualizationRoot
    One or more directories holding visualization translation units, relative to
    RepoRoot. There is NO Visualization/ subdirectory in either — the files live
    alongside their sim or engine siblings, which is why this lint globs a FILENAME
    pattern rather than scanning a directory wholesale. Every configured root must
    exist and must match at least one file; see the file-collection section.

    One root over a scratch tree works under `-File`; for several, use
    `pwsh -Command "& ./script.ps1 -VisualizationRoot A,B"`.
    ⚠ `pwsh -File` cannot pass an ARRAY — `-VisualizationRoot A B` is a binding
    error (PositionalBinding is off), not two roots.

.PARAMETER FilePattern
    One or more filename globs identifying visualization translation units. See the
    SCOPE note in the file-collection section below for why each default is what it
    is, and for the contract a new presentation unit has to satisfy.

.EXAMPLE
    pwsh tools/lint/visualization_hitbox_isolation.ps1

.EXAMPLE
    pwsh tools/lint/visualization_hitbox_isolation.ps1 -Verbose

.OUTPUTS
    Exit 0 = clean. Exit 1 = at least one violation. Exit 2 = usage / IO error.
#>
# PositionalBinding is OFF deliberately. -VisualizationRoot and -FilePattern are both
# arrays; with positional binding on, `-VisualizationRoot A B` silently binds B to
# -FilePattern instead of adding a second root. A stray positional now fails loudly.
[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..' '..')).Path,
    [string[]]$VisualizationRoot = @(
        'Plugins/OGBrawler/Source/OGBrawler/og-brawler/OGBrawler',
        'Source/OGBrawlerUnreal'
    ),
    [string[]]$FilePattern = @('*Visualization*', '*UEHUD*')
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# HITBOX_RESOLUTION_BLACKLIST
#
# Schema — one hashtable per hitbox-resolution header family:
#   Header  : human-readable name of the guarded header (for messages).
#   Pattern : .NET regex applied to the LEAF filename of each #include target.
#             Anchored with ^...$ so it matches a whole leaf, not a substring.
#   Message : printed on a hit; names the header and references R-UE1.
#   Note    : (optional) rationale.
#
# CONTRACT: a new header that RESOLVES hits (decides hit/no-hit, writes inbound-hit
# derived state, or owns the routing pass) gets an entry here in the same change.
# A new header that merely DECLARES sim state types does NOT — visualization is
# allowed, by design, to read post-sim state through a const&.
# ---------------------------------------------------------------------------
$Blacklist = @(
    @{
        Header  = 'BrawlerHitRouting* (hit-routing system)'
        # Covers the shipping BrawlerHitRoutingSystem.h and any future sibling
        # spelling (BrawlerHitRouting.h, BrawlerHitRoutingTypes.h, ...). This is
        # the header that owns routeInbound and WRITES wasHitThisTick — the actual
        # hit resolver, and the thing visualization must never reach.
        Pattern = '^BrawlerHitRouting\w*\.h$'
        Message = 'visualization must not include the hit-routing system (BrawlerHitRouting*.h) — hitbox resolution runs against server-authoritative sim state only; visualization never contributes to hitbox math (R-UE1)'
    },
    @{
        Header  = 'BrawlerInboundHit.h (inbound-hit derived state)'
        # The OUTPUT payload of hit resolution (wasHitThisTick et al.). Including it
        # from visualization means viz is consuming, or worse constructing, hit
        # verdicts. Not included by any visualization file today.
        Pattern = '^BrawlerInboundHit\.h$'
        Message = 'visualization must not include the inbound-hit derived state (BrawlerInboundHit.h) — hit verdicts are sim-side output and must not be read or synthesised by visualization (R-UE1)'
    }
)

# ---------------------------------------------------------------------------
# Strip // comments, /* */ comments (incl. multi-line), and string literals from a
# single physical line. $InBlock carries block-comment state across lines.
#
# NOTE — divergence from configurability_lint.ps1's version, and why: that lint
# blanks string literals so a log message mentioning a field never matches. Here
# the payload we need to read (the include target) LIVES inside a string literal,
# so blanking would defeat the lint entirely. Instead the include target is
# extracted FIRST, from the comment-stripped line, and string blanking is skipped.
# Comment stripping is retained and is the part that matters: a commented-out
# `// #include "BrawlerHitRoutingSystem.h"` must not fail the build.
# ---------------------------------------------------------------------------
function Remove-Comments {
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

if ($VisualizationRoot.Count -eq 0) {
    Write-Host 'Visualization isolation lint: FAILED - no visualization root configured.'
    Write-Host 'An empty root set would pass vacuously, so this is treated as a configuration error.'
    exit 2
}

foreach ($root in $VisualizationRoot) {
    if (-not (Test-Path -LiteralPath (Join-Path $RepoRoot $root))) {
        Write-Host "Visualization isolation lint: FAILED - visualization root not found at $root (path moved?)."
        Write-Host 'Update -VisualizationRoot, or the default in this script, in the same change that moves the files.'
        exit 2
    }
}

# ---------------------------------------------------------------------------
# File collection.
#
# SCOPE NOTE - the glob set, and why each entry is there.
#
# `*Visualization*` rather than the narrower `*Visualization.*` the original task
# pinned: a strict SUPERSET, which additionally covers
#   DAttackVisualizationUtils.h/.cpp - shared visualization helpers, #included by
#       FOUR of the six originally pinned headers. Left unguarded it is a transitive
#       bypass: a hitbox include placed here reaches every visualization TU while the
#       narrow glob reports clean.
#   CharacterVisualizationData.h     - visualization state payload; same rationale.
#
# `*UEHUD*` because the UE half's presentation surface does NOT carry
# `Visualization` in its filename. `AOGBrawlerUEHUD` is the only screen-space
# drawing surface in the project and is a visualization translation unit in every
# sense the invariant cares about; scanning the UE root on the name glob alone would
# have covered the ring store and the debug-draw helper while leaving the file that
# actually draws unscanned - the same shape of gap this root list exists to close.
#
# CONTRACT: a new presentation translation unit is covered iff its filename matches
# a glob here. Name it `*Visualization*`, or add its glob in the same change.
#
# ROOT SET: `og-brawler`'s core holds the engine-free visualization free functions;
# `Source/OGBrawlerUnreal` holds the UE-side units that render them. Scanning only
# the first left every UE-side unit unguarded, so a green run said nothing about the
# UE half of the invariant. Test trees are deliberately NOT roots: a test may
# legitimately construct a hit verdict in order to prove visualization ignores it.
#
# Every configured root must match at least one file. A root that silently matches
# nothing - renamed away, moved out from under the lint - would let the scan shrink
# without the report changing, which is exactly how this gap went unnoticed.
# The scanned set is printed below so a reviewer can confirm the file set directly
# rather than infer it from the patterns.
# ---------------------------------------------------------------------------
$includeGlobs = @($FilePattern | ForEach-Object { "$_.h"; "$_.cpp" })

$files = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
$seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

foreach ($root in $VisualizationRoot) {
    $rootFull = Join-Path $RepoRoot $root
    $rootFiles = @(Get-ChildItem -Path $rootFull -File -Include $includeGlobs -Recurse | Sort-Object FullName)

    if ($rootFiles.Count -eq 0) {
        Write-Host ("Visualization isolation lint: FAILED - no files matched '{0}' under {1}." -f ($includeGlobs -join '|'), $root)
        Write-Host 'A configured root matching nothing would pass vacuously, so this is treated as a configuration error.'
        exit 2
    }

    foreach ($f in $rootFiles) {
        if ($seen.Add($f.FullName)) { $files.Add($f) }
    }
}

# Matches  #include "foo/bar.h"  and  #include <foo/bar.h>, capturing the target.
$includeRegex = '^\s*#\s*include\s*[<"]([^>"]+)[>"]'

$violations = [System.Collections.Generic.List[object]]::new()

foreach ($f in $files) {
    Write-Verbose ("scanning {0}" -f [System.IO.Path]::GetRelativePath($RepoRoot, $f.FullName))

    $inBlock = $false
    $lineNo = 0
    foreach ($line in [System.IO.File]::ReadLines($f.FullName)) {
        $lineNo++
        $code = Remove-Comments $line ([ref]$inBlock)
        if ([string]::IsNullOrWhiteSpace($code)) { continue }

        $m = [regex]::Match($code, $includeRegex)
        if (-not $m.Success) { continue }

        # Compare on the LEAF so every path spelling of the same header is caught.
        $leaf = [System.IO.Path]::GetFileName($m.Groups[1].Value.Replace('\', '/'))

        foreach ($entry in $Blacklist) {
            if ($leaf -match $entry.Pattern) {
                $rel = [System.IO.Path]::GetRelativePath($RepoRoot, $f.FullName)
                $violations.Add([pscustomobject]@{
                    File    = $rel
                    Line    = $lineNo
                    Include = $m.Groups[1].Value
                    Header  = $entry.Header
                    Message = $entry.Message
                })
            }
        }
    }
}

# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------
$docPath = 'Plugins/OGBrawler/Source/OGBrawler/og-brawler/OGBrawler/VISUALIZATION_DISCIPLINE.md'

if ($violations.Count -gt 0) {
    foreach ($v in $violations) {
        Write-Host ("{0}:{1}: #include '{2}' - {3}" -f $v.File, $v.Line, $v.Include, $v.Message)
    }
    Write-Host ''
    Write-Host ("Visualization isolation lint: FAILED ({0} violation(s) across {1} visualization file(s) scanned)" -f `
        $violations.Count, $files.Count)
    Write-Host "See $docPath for the invariant, the For Honor precedent, and the code-review checklist."
    exit 1
}
else {
    Write-Host ("Visualization isolation lint: clean ({0} visualization file(s) scanned)" -f $files.Count)
    foreach ($f in $files) {
        Write-Host ("  {0}" -f [System.IO.Path]::GetRelativePath($RepoRoot, $f.FullName))
    }
    exit 0
}
