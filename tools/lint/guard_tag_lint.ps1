#Requires -Version 7.0
# SPDX-License-Identifier: MPL-2.0
<#
.SYNOPSIS
    Guard-tag lint - the join between a `# ⛔G-nn` tag in source and its entry in
    a `*-guards.md` document, checked in BOTH directions, as a HARD GATE.

.DESCRIPTION
    CommentExtractionRule v2 (2026-09-11) moves every fence out of source and
    leaves one line behind at the site:

        // ⛔G-07  docs/CollisionCategoryConstants-guards.md

    That trade is only survivable because the join it creates is machine-checkable,
    which no join this initiative had created before was. This is that check.

    ------------------------------------------------------------------------
    WHY A COMPANION AND NOT PART OF doc_anchor_lint.ps1
    ------------------------------------------------------------------------
    Four reasons, and the last is the load-bearing one:

    1. DIRECTION. doc_anchor_lint asserts doc -> tree: a name a document uses must
       exist. Half of this check runs the other way (source -> doc), and the other
       half (orphan detection) has no analogue there at all.
    2. THE COMMENT STRIP. doc_anchor_lint resolves symbols against source with
       comments REMOVED, and its own header calls that strip load-bearing and says
       not to remove it. A guard tag lives in a comment. The two tools want opposite
       things from the same bytes.
    3. EXIT CODES. "a document named a symbol that does not exist" and "a fence lost
       its site" are different failures and a caller must be able to tell them apart.
    4. FAILURE INDEPENDENCE. A gate that shares a process with another gate can be
       switched off by that other gate's bug. These four checks are the only thing
       standing between v2 and a silently broken fence, so they get their own exit
       path, their own corpus walk and their own poison arms.

    ------------------------------------------------------------------------
    THE GRAMMAR - deliberately narrow
    ------------------------------------------------------------------------
    A TAG is a source line matching

        //  <optional whitespace>  ⛔G-nn  <whitespace>  <path ending in .md>

    Nothing else is a tag. In particular a bare `G-04` inside a string literal, a
    `static_assert` message or prose is NOT a tag - it is a NAMING, which v2
    explicitly permits for a retired id. That distinction is what lets a retired
    fence's id live on as a cross-reference in the compile-time check that replaced
    it without tripping the retirement rule, and the shipped tree exercises it: the
    subject header names G-04 and G-07 in two `static_assert` messages and carries
    neither as a tag.

    A LIVE ENTRY in a `*-guards.md` is a line `## G-nn — title`.
    A RETIRED ENTRY is a line `### G-nn — RETIRED...` under the `## §R Retired ids`
    heading. Retired ids are spent forever: reusing one silently re-points every
    reference that ever named it.

    ------------------------------------------------------------------------
    THE FOUR CHECKS
    ------------------------------------------------------------------------
      1 TAG -> DOC      every tag resolves to a LIVE entry in the doc it names
      2 DOC -> TAG      every live entry is referenced by at least one tag
                        (an entry nothing points at is an ORPHAN: the fence lost
                        its site, which is the exact failure v2 trades away
                        positionality to avoid)
      3 UNIQUENESS      no id is a live entry twice, a retired entry twice, both
                        live and retired, or carried by two source sites
      4 RETIREMENT      no tag names a retired id

    ------------------------------------------------------------------------
    WHAT THIS CANNOT DO - stated, not hidden
    ------------------------------------------------------------------------
    It checks that an entry EXISTS. It never checks that the entry's text is TRUE,
    that it still describes the declaration it sits on, or that the declaration is
    still the place the forbidden edit would be typed. R0 (verify every claim
    against the tree before it moves) is unchanged and still the only thing that
    reaches truth.

.PARAMETER Only
    One or more PATH FRAGMENTS. When given, BOTH the source scan and the guards-doc
    set are restricted to files whose repo-relative path contains one of them, so a
    conversion in flight can gate ITS OWN file and doc without the rest of the tree
    joining in.

    Why it exists: this is a REPO-WIDE hard gate, and a conversion writes its doc
    entries BEFORE it places its tags. In that window every one of its entries is an
    orphan and the whole tree goes red - for every other worker too. Task 72's review
    measured its own window at 5 seconds and predicted that a 26-fence file's window
    would not be, and routed this as a blocker for task 68.

        guard_tag_lint.ps1 -Only BrawlerMovementSimulation

    (c) It NARROWS, it never widens. A scoped run proves nothing about the rest of
    the tree; run it unscoped before claiming the gate is green.

.PARAMETER Poison
    Seed one defect and require the run to FAIL. The poison is applied to the text
    AFTER it is read from disk and BEFORE it is parsed, so the parser and all four
    checks run on it; NOTHING ON DISK IS MODIFIED. A check that cannot be made to
    fail proves nothing, and this initiative has shipped two vacuous controls.

      UnresolvedTag  adds a tag naming an id no doc defines        -> check 1
      Orphan         removes one tag from the source text          -> check 2
      Duplicate      adds a second site carrying an existing id    -> check 3
      RetiredReuse   adds a tag naming a RETIRED id                -> check 4

.OUTPUTS
    Exit 0 = every tag resolves, no orphan, no duplicate, no retired id reused.
    Exit 1 = at least one violation.
    Exit 2 = usage / IO error, AND the instrument-failure case: under -Poison, the
             seeded defect did NOT fire.

    (c) 1 and 2 are DELIBERATELY DIFFERENT. Both used to be 1, so a caller checking
    "exit 1 means the poison arm fired" could not distinguish a working instrument
    from a vacuous one - which is exactly the shape of the vacuous controls this
    initiative has already shipped. A poison arm that fires is a PASS for the
    instrument; one that does not is a fault in the instrument, not a dirty tree.
#>
[CmdletBinding()]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..' '..')).Path,
    [string[]]$SourceRoots = @('Plugins', 'Source'),
    [string[]]$DocDirs = @(
        'Plugins/OGSimulation/Source/OGSimulation/og-simulation/OGSimulation/docs',
        'Plugins/OGBrawler/Source/OGBrawler/og-brawler/OGBrawler/docs',
        'Source/OGBrawlerUnreal/docs'
    ),
    [string[]]$SourceExtensions = @('.h', '.cpp', '.hpp', '.inl'),
    [ValidateSet('None', 'UnresolvedTag', 'Orphan', 'Duplicate', 'RetiredReuse',
                 'DUnresolvedTag', 'DOrphan', 'DDuplicate', 'DRetiredReuse')]
    [string]$Poison = 'None',
    [string[]]$Only = @(),
    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'
$GLYPH = [char]0x26D4          # ⛔
$TAG_RX = [regex]::new("//\s*$GLYPH(G-\d{2})\s+(\S+\.md)")
$LIVE_RX = [regex]::new('^##\s+(G-\d{2})\s+\u2014')
$RETIRED_RX = [regex]::new('^###\s+(G-\d{2})\s+\u2014\s+RETIRED')

# ---- the DERIVATION class (v2.1, task 82). Glyph chosen BY TEST, not by taste;
# the five-instrument matrix is in impl_notes_seam_82.md 2. U+2234 THEREFORE is BMP
# (so [char] can hold it - U+1F4D0 PROVABLY cannot, and this line is why), occurs
# ZERO times in the tree, and is NOT the prohibition glyph, whose 2088 occurrences
# are the reason diluting it would weaken every real fence.
$D_GLYPH = [char]0x2234        # ∴
$D_TAG_RX = [regex]::new('//\s*' + $D_GLYPH + '(D-\d{2})\s+(\S+\.md)')
# A LIVE derivation entry is an EXISTING rationale-doc heading with the tag APPENDED:
#     ## 19. The dual-basis decomposition - task 57 / ruling #29 ∴D-01
# Appending to a heading that already exists, rather than adding an entry of its own,
# is the load-bearing choice: the entry then IS the section that already holds the
# derivation, so there is NO SECOND COPY to drift out of step with the first. It also
# means the SAME grep finds both ends of the join - `grep -rn ∴D-01` returns the
# source site and the doc section, which is the greppability property v2 2 claims for
# the guard glyph and which a `N` pointer would not have.
#
# Levels 2-4 are all accepted because a cited FIELD needs its own anchor INSIDE a
# section: `7 StaticData - the authored constants, field by field` is one section
# holding many fields, and two inbound citations name two different fields in it.
#
# LIVE vs RETIRED is decided by WHERE THE ID SITS, and the two are disjoint:
#   LIVE     the id is the LAST thing on the heading line
#   RETIRED  the id is the FIRST thing, followed by an em dash and RETIRED
$D_LIVE_RX = [regex]::new('^#{2,4}\s.*' + $D_GLYPH + '(D-\d{2})\s*$')
$D_RETIRED_RX = [regex]::new('^#{2,4}\s+' + $D_GLYPH + '(D-\d{2})\s+\u2014\s+RETIRED')

function Write-Info { param($m) if (-not $Quiet) { Write-Host $m } }

# -Only: a repo-relative path is IN SCOPE when it contains any of the fragments.
# No fragments given = the whole tree, which is the gate's normal posture.
function Test-InScope {
    param([string]$RelPath)
    if ($Only.Count -eq 0) { return $true }
    foreach ($f in $Only) {
        if ($RelPath.Replace('\\', '/').ToLower().Contains($f.Replace('\\', '/').ToLower())) { return $true }
    }
    return $false
}
if ($Only.Count -gt 0) {
    Write-Info ('  SCOPED to: {0}' -f ($Only -join ', '))
    Write-Info '  (a scoped run narrows the gate; it proves nothing about the rest of the tree)'
}

# ================================================================= COLLECTION
# Both classes are collected by the SAME two functions, parameterised by the
# regexes at the top. That is what makes this ONE instrument rather than two:
# a bug in the corpus walk, the scope filter or the suffix resolution is a bug
# in BOTH classes at once and cannot be fixed for `G` while `D` silently keeps
# it. v2.1 2.1: "Reuse the instrument. Do not write a second one."

function Get-DocEntries {
    param([string]$Filter, [regex]$LiveRx, [regex]$RetiredRx, [ref]$FileCount)
    $result = @{}
    $files = @()
    foreach ($d in $DocDirs) {
        $full = Join-Path $RepoRoot $d
        if (-not (Test-Path $full)) { continue }
        $files += @(Get-ChildItem -Path $full -Filter $Filter -File -ErrorAction SilentlyContinue |
            Where-Object { Test-InScope ([IO.Path]::GetRelativePath($RepoRoot, $_.FullName)) })
    }
    $FileCount.Value = $files.Count
    foreach ($f in $files) {
        $rel = [IO.Path]::GetRelativePath($RepoRoot, $f.FullName).Replace('\', '/')
        $text = [IO.File]::ReadAllText($f.FullName)
        $live = [ordered]@{}; $retired = [ordered]@{}
        $dupLive = @(); $dupRetired = @()
        $n = 0
        foreach ($line in ($text -split "\r?\n")) {
            $n++
            $m = $LiveRx.Match($line)
            if ($m.Success) {
                $id = $m.Groups[1].Value
                if ($live.Contains($id)) { $dupLive += "$id (lines $($live[$id]) and $n)" }
                else { $live[$id] = $n }
                continue
            }
            $m = $RetiredRx.Match($line)
            if ($m.Success) {
                $id = $m.Groups[1].Value
                if ($retired.Contains($id)) { $dupRetired += "$id (lines $($retired[$id]) and $n)" }
                else { $retired[$id] = $n }
            }
        }
        $result[$rel] = @{ Live = $live; Retired = $retired; DupLive = $dupLive; DupRetired = $dupRetired }
    }
    return $result
}

function Get-SourceTags {
    param([regex]$TagRx, [ref]$ScannedCount)
    $found = @()
    $n = 0
    foreach ($r in $SourceRoots) {
        $full = Join-Path $RepoRoot $r
        if (-not (Test-Path $full)) { continue }
        Get-ChildItem -Path $full -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $SourceExtensions -contains $_.Extension.ToLower() -and
                $_.FullName -notmatch '\\(Intermediate|Binaries|Saved|DerivedDataCache|\.git)\\' -and
                (Test-InScope ([IO.Path]::GetRelativePath($RepoRoot, $_.FullName)))
            } | ForEach-Object {
                $n++
                $rel = [IO.Path]::GetRelativePath($RepoRoot, $_.FullName).Replace('\', '/')
                $lines = [IO.File]::ReadAllLines($_.FullName)
                for ($i = 0; $i -lt $lines.Count; $i++) {
                    $m = $TagRx.Match($lines[$i])
                    if ($m.Success) {
                        $found += [pscustomobject]@{
                            Id      = $m.Groups[1].Value
                            DocPath = $m.Groups[2].Value
                            File    = $rel
                            Line    = $i + 1
                            Text    = $lines[$i].Trim()
                        }
                    }
                }
            }
    }
    $ScannedCount.Value = $n
    return $found
}

# A tag names a path SUFFIX ("docs/Foo-guards.md"); match it against the
# discovered docs by suffix, which is how doc_anchor_lint resolves FILE anchors.
function Resolve-DocIn {
    param($TagDocPath, $DocSet)
    foreach ($k in $DocSet.Keys) { if ($k.EndsWith($TagDocPath)) { return $k } }
    return $null
}

$gDocCount = 0; $dDocCount = 0; $scanned = 0; $dScanned = 0
$docs  = Get-DocEntries -Filter '*-guards.md'    -LiveRx $LIVE_RX   -RetiredRx $RETIRED_RX   -FileCount ([ref]$gDocCount)
$ddocs = Get-DocEntries -Filter '*-rationale.md' -LiveRx $D_LIVE_RX -RetiredRx $D_RETIRED_RX -FileCount ([ref]$dDocCount)
$tags  = @(Get-SourceTags -TagRx $TAG_RX   -ScannedCount ([ref]$scanned))
$dtags = @(Get-SourceTags -TagRx $D_TAG_RX -ScannedCount ([ref]$dScanned))

Write-Info ("  guards docs    (G-nn) : {0}" -f $gDocCount)
Write-Info ("  rationale docs (D-nn) : {0}" -f $dDocCount)

# ==================================================================== POISON
# Applied to the in-memory tag/doc model AFTER reading and BEFORE checking, so
# the parser and all four checks run on it. NOTHING ON DISK IS MODIFIED.
# A check that cannot be made to fail proves nothing, and this initiative has
# shipped three vacuous controls and retired a fourth in place.
$poisonDescription = ''
$poisonClass = ''
if ($Poison -ne 'None') {
    $poisonClass = if ($Poison.StartsWith('D')) { 'D' } else { 'G' }
    if ($poisonClass -eq 'G') { $victimSet = $tags;  $victimDocs = $docs }
    else                      { $victimSet = $dtags; $victimDocs = $ddocs }
    if ($victimSet.Count -eq 0) { Write-Error "POISON: no $poisonClass tags found to poison"; exit 2 }
    $victim = $victimSet[0]
    $glyphFor = if ($poisonClass -eq 'G') { $GLYPH } else { $D_GLYPH }
    $badId    = if ($poisonClass -eq 'G') { 'G-99' } else { 'D-99' }
    switch -Wildcard ($Poison) {
        '*UnresolvedTag' {
            $new = [pscustomobject]@{ Id = $badId; DocPath = $victim.DocPath
                File = $victim.File; Line = 9999; Text = "// $glyphFor$badId $($victim.DocPath)" }
            if ($poisonClass -eq 'G') { $tags += $new } else { $dtags += $new }
            $poisonDescription = "added a $poisonClass tag naming $badId, which no doc defines"
        }
        '*Orphan' {
            $drop = $victimSet[-1]
            $kept = @($victimSet | Where-Object { -not ($_.Id -eq $drop.Id -and $_.Line -eq $drop.Line) })
            if ($poisonClass -eq 'G') { $tags = $kept } else { $dtags = $kept }
            $poisonDescription = "removed the tag for $($drop.Id) from the source text"
        }
        '*Duplicate' {
            $new = [pscustomobject]@{ Id = $victim.Id; DocPath = $victim.DocPath
                File = $victim.File; Line = 9998; Text = $victim.Text }
            if ($poisonClass -eq 'G') { $tags += $new } else { $dtags += $new }
            $poisonDescription = "added a SECOND site carrying $($victim.Id)"
        }
        '*RetiredReuse' {
            # (c) THE TAG MUST NAME THE DOC THAT RETIRES THE ID. Borrowing $victim.DocPath was
            # harmless while exactly one guards doc existed and silently broke the arm the moment
            # a second one landed (task 74). Derive the path from the doc instead.
            $anyRetired = $null; $victimDoc = $null
            foreach ($k in $victimDocs.Keys) {
                if ($victimDocs[$k].Retired.Count -gt 0) {
                    $anyRetired = @($victimDocs[$k].Retired.Keys)[0]; $victimDoc = $k; break
                }
            }
            if (-not $anyRetired) {
                # (c) THE ARM MUST NOT DEPEND ON THE TREE HAPPENING TO CARRY A RETIRED ID.
                # The D class ships with ZERO retired ids - retirement is a rule about the
                # FUTURE - so an arm that needed one on disk would be VACUOUS on the very tree
                # that introduces the class: exactly the shape of the vacuous controls this
                # initiative has already shipped. Synthesise the retired entry in memory so the
                # arm tests CHECK 4's LOGIC, which is what it is for, rather than testing
                # whether anyone has happened to retire an id yet.
                $victimDoc = (Resolve-DocIn $victim.DocPath $victimDocs)
                if (-not $victimDoc) { Write-Error 'POISON: cannot resolve a doc to synthesise a retired id in'; exit 2 }
                $anyRetired = if ($poisonClass -eq 'G') { 'G-98' } else { 'D-98' }
                $victimDocs[$victimDoc].Retired[$anyRetired] = 9996
                $poisonDescription = "SYNTHESISED retired id $anyRetired in $victimDoc (this class has none on disk), then "
            }
            $retiredDocPath = 'docs/' + [IO.Path]::GetFileName($victimDoc)
            $new = [pscustomobject]@{ Id = $anyRetired; DocPath = $retiredDocPath
                File = $victim.File; Line = 9997; Text = "// $glyphFor$anyRetired $retiredDocPath" }
            if ($poisonClass -eq 'G') { $tags += $new } else { $dtags += $new }
            $poisonDescription += "added a tag naming $anyRetired, which $victimDoc lists as RETIRED"
        }
    }
    Write-Info ''
    Write-Info "  POISON [$Poison] (class $poisonClass): $poisonDescription"
    Write-Info '  (applied in memory, after reading and before checking; disk untouched)'
}

# ================================================================ THE CHECKS
function Invoke-TagChecks {
    param($TagSet, $DocSet, [string]$ClassName)
    $c1 = @(); $c2 = @(); $c3 = @(); $c4 = @()
    foreach ($t in $TagSet) {
        $doc = Resolve-DocIn $t.DocPath $DocSet
        if (-not $doc) { $c1 += "$($t.File):$($t.Line)  $($t.Id) names '$($t.DocPath)' - NO SUCH $ClassName DOC"; continue }
        if ($DocSet[$doc].Retired.Contains($t.Id)) {
            $c4 += "$($t.File):$($t.Line)  $($t.Id) is RETIRED in $doc (line $($DocSet[$doc].Retired[$t.Id])) and must never be tagged again"
            continue
        }
        if (-not $DocSet[$doc].Live.Contains($t.Id)) {
            $c1 += "$($t.File):$($t.Line)  $($t.Id) has NO entry in $doc"
        }
    }
    foreach ($doc in $DocSet.Keys) {
        foreach ($id in $DocSet[$doc].Live.Keys) {
            $refs = @($TagSet | Where-Object { $_.Id -eq $id -and (Resolve-DocIn $_.DocPath $DocSet) -eq $doc })
            if ($refs.Count -eq 0) {
                $c2 += "${doc}:$($DocSet[$doc].Live[$id])  $id is an ORPHAN - no source tag references it; the entry lost its site"
            }
            elseif ($refs.Count -gt 1) {
                $c3 += "$id is carried by $($refs.Count) sites: " + (($refs | ForEach-Object { "$($_.File):$($_.Line)" }) -join ', ')
            }
        }
        foreach ($s in $DocSet[$doc].DupLive) { $c3 += "${doc}: duplicate LIVE entry $s" }
        foreach ($s in $DocSet[$doc].DupRetired) { $c3 += "${doc}: duplicate RETIRED entry $s" }
        $both = @($DocSet[$doc].Live.Keys | Where-Object { $DocSet[$doc].Retired.Contains($_) })
        foreach ($b in $both) { $c3 += "${doc}: $b is BOTH live and retired" }
    }
    return @{ C1 = $c1; C2 = $c2; C3 = $c3; C4 = $c4 }
}

$G = Invoke-TagChecks -TagSet $tags  -DocSet $docs  -ClassName 'GUARDS'
$D = Invoke-TagChecks -TagSet $dtags -DocSet $ddocs -ClassName 'RATIONALE'

# ==================================================================== REPORT
function Write-ClassReport {
    param($R, $TagSet, $DocSet, [string]$Label)
    $liveTotal = ($DocSet.Keys | ForEach-Object { $DocSet[$_].Live.Count } | Measure-Object -Sum).Sum
    $retiredTotal = ($DocSet.Keys | ForEach-Object { $DocSet[$_].Retired.Count } | Measure-Object -Sum).Sum
    if ($null -eq $liveTotal) { $liveTotal = 0 }
    if ($null -eq $retiredTotal) { $retiredTotal = 0 }
    Write-Info ''
    Write-Info ("  === {0} ===" -f $Label)
    Write-Info ('    live entries  : {0}' -f $liveTotal)
    Write-Info ('    retired ids   : {0}' -f $retiredTotal)
    Write-Info ('    tags found    : {0}' -f $TagSet.Count)
    Write-Info ('    CHECK 1  tag -> doc  : {0} violation(s)' -f $R.C1.Count)
    foreach ($x in $R.C1) { Write-Info "        $x" }
    Write-Info ('    CHECK 2  doc -> tag  : {0} violation(s)' -f $R.C2.Count)
    foreach ($x in $R.C2) { Write-Info "        $x" }
    Write-Info ('    CHECK 3  uniqueness  : {0} violation(s)' -f $R.C3.Count)
    foreach ($x in $R.C3) { Write-Info "        $x" }
    Write-Info ('    CHECK 4  retirement  : {0} violation(s)' -f $R.C4.Count)
    foreach ($x in $R.C4) { Write-Info "        $x" }
    return ($R.C1.Count + $R.C2.Count + $R.C3.Count + $R.C4.Count)
}

Write-Info ''
Write-Info ('  source files scanned : {0}' -f $scanned)
$gTotal = Write-ClassReport -R $G -TagSet $tags  -DocSet $docs  -Label "GUARD      $GLYPH`G-nn -> *-guards.md"
$dTotal = Write-ClassReport -R $D -TagSet $dtags -DocSet $ddocs -Label "DERIVATION $D_GLYPH`D-nn -> *-rationale.md"

$total = $gTotal + $dTotal
Write-Info ''

if ($Poison -ne 'None') {
    $R = if ($poisonClass -eq 'G') { $G } else { $D }
    $expected = switch -Wildcard ($Poison) {
        '*UnresolvedTag' { $R.C1.Count }
        '*Orphan'        { $R.C2.Count }
        '*Duplicate'     { $R.C3.Count }
        '*RetiredReuse'  { $R.C4.Count }
    }
    if ($expected -gt 0) {
        Write-Host "  POISON ARM FIRED: the seeded $poisonClass defect was caught by its OWN check. RESULT: FAIL (as required)"
        exit 1
    }
    # (c) NOT Write-Error. $ErrorActionPreference is 'Stop' at the top of this file, so a
    # Write-Error here is a TERMINATING error: the script aborts and pwsh exits 1, and the
    # `exit 2` below is UNREACHABLE. That was equally true of the original `exit 1`, which is
    # why nobody noticed - the two outcomes exited 1 either way and the code looked deliberate.
    [Console]::Error.WriteLine("POISON ARM DID NOT FIRE - the check is vacuous. This is an instrument failure, not a dirty tree.")
    exit 2
}

if ($total -eq 0) {
    Write-Info '  RESULT: CLEAN - both classes: every tag resolves, no orphan, no duplicate, no retired id reused.'
    exit 0
}
Write-Info "  RESULT: $total VIOLATION(S) - HARD GATE, conversion rejected."
exit 1
