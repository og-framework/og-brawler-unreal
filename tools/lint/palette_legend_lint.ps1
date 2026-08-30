#Requires -Version 7.0
# SPDX-License-Identifier: MPL-2.0
<#
.SYNOPSIS
    Palette-legend lint — the frame-meter's three colour switches, checked against
    docs/InputHistoryDisplay-rationale.md §7.11's legend tables.

.DESCRIPTION
    Nothing in the tree maps a lane colour to its meaning except the `switch` arms in
    BrawlerInputHistoryVisualizationBars.h. §7.11 writes that binding down as three
    tables (provenance, machine-state, input-delay verdict) plus a fourth table for the
    three colours that belong to no enumerator at all. This lint parses BOTH sides — the
    header's `provenanceCellStyleOf` / `machineCellStyleOf` / `delayVerdictStyleOf`
    switches, and the doc's markdown tables — and diffs them.

    ⭐ WHY A HAND-TYPED TABLE CANNOT BE TRUSTED. Palette values are each implementer's
    own choice; the test suite asserts RELATIONS on them (pairwise gaps, cross-palette
    floors, a strict-max isolation), never literals. A prose table copied from today's
    code goes stale silently the first time anyone retunes a hue. So the table is
    CHECKED here, not trusted — the same argument VISUALIZATION_DISCIPLINE.md already
    makes about its own scanned file list.

    WHAT THIS LINT VERIFIES, per palette:
      * every enumerator the header's switch gives an explicit `case` arm to has a row
        in the matching §7.11 table, and every table row names an arm the switch has;
      * the row's RGB triple (or its "hole" marker) matches the arm's `LaneCellFill`
        and, for a State arm, its exact three channel literals;
      * the parsed row COUNT for each table equals the enumerator's own count constant
        (`kRowProvenanceSummaryCount`, `kMachineStateCellCount`, `kInputDelayVerdictCount`
        — read from their OWN declaring headers, never a literal), so an enumerator
        added without a matching switch arm AND table row is still caught: the count
        constant moves and the table does not follow it.
      * `kLaneElisionColor`, `kLaneResyncColor` and `kUnnamedLaneColor` — module-level
        colours that belong to no enumerator — each have a row in the fourth table,
        checked by value but deliberately NOT folded into any enumerator's count.
      * §7.11's **`On screen (sRGB hex)`** column — the DISPLAYED colour, since
        `meterCellColor` builds an `FLinearColor` straight from these LINEAR floats and
        lets Unreal gamma-encode it for the screen. Every row's hex is RE-DERIVED here
        from that row's own header floats (the same conversion `-EmitHex` prints) and
        compared against what the doc has written down, so a float retuned without
        regenerating its hex fails exactly like an RGB mismatch does — this is the drift
        case the arm exists for.

    ⭐ THE EMIT MODE. `-EmitHex` prints, per enumerator (and per non-enumerator colour),
    the same sRGB hex the checker above independently re-derives — GENERATED from the
    header's own RGB floats, so a doc column built from this output cannot itself be a
    transcription error the checker would then rubber-stamp. Emitter and checker call the
    SAME conversion function (`ConvertTo-SRGB8` / `ConvertTo-SRGBHex`, defined once, right
    below the header side's parsing helpers, ending at a sentinel comment the Pester spec's
    round-trip test extracts by) — two implementations of one formula would drift
    silently, so there is exactly one.

    ⭐ WHAT THIS LINT DOES **NOT** CHECK. It can verify enumerator names, RGB values, the
    derived sRGB hex and `LaneCellFill` kinds. It CANNOT verify that a row's MEANING
    sentence, or its plain-English `Colour` word, are TRUE — both are prose, and a green
    run here is not a claim that either is accurate, only that the names/colours/hex/kinds
    it can read agree with the header. Say this again at the top of §7.11 itself; a green
    lint that read as "the meanings (or the colour names) are verified" would imply more
    than this lint establishes.

.PARAMETER EmitHex
    Prints the GENERATED `On screen (sRGB hex)` value for every switch arm and every
    non-enumerator colour, computed from -BarsHeaderPath's own RGB floats, then exits 0.
    Does not read or check the doc at all — this is the source a human pastes the hex
    column FROM, not a second check of it. Run the script again without -EmitHex to check
    what actually ended up in the doc.

.PARAMETER RepoRoot
    Repository root. Defaults to two levels above this script (tools/lint/..).

.PARAMETER BarsHeaderPath
    The header carrying the three colour switches and the two non-enumerator colours.
    Relative paths resolve against -RepoRoot; an absolute path is used as-is (this is
    what lets a RED probe point the lint at a SCRATCH COPY without touching the tree).

.PARAMETER CoreHeaderPath
    The header declaring `kRowProvenanceSummaryCount`.

.PARAMETER LanesHeaderPath
    The header declaring `kMachineStateCellCount` and `kInputDelayVerdictCount`.

.PARAMETER DocPath
    The rationale doc carrying §7.11's four tables.

.EXAMPLE
    pwsh tools/lint/palette_legend_lint.ps1

.OUTPUTS
    Exit 0 = every parsed arm and every table row agree (RGB, sRGB hex and fill kind),
             and the three parsed counts match their governing constants. With -EmitHex,
             exit 0 means the generated hex table printed; nothing was checked.
    Exit 1 = at least one mismatch (a missing/extra table row, an RGB, hex or fill-kind
             mismatch, or a row count that does not match its constant).
    Exit 2 = usage / IO error (a configured path does not exist, or a table this lint
             needs could not be found at all — never a silent zero).
#>
[CmdletBinding()]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..' '..')).Path,
    [string]$BarsHeaderPath = 'Plugins/OGBrawler/Source/OGBrawler/og-brawler/OGBrawler/BrawlerInputHistoryVisualizationBars.h',
    [string]$CoreHeaderPath = 'Plugins/OGBrawler/Source/OGBrawler/og-brawler/OGBrawler/BrawlerInputHistoryVisualization.h',
    [string]$LanesHeaderPath = 'Plugins/OGBrawler/Source/OGBrawler/og-brawler/OGBrawler/BrawlerInputHistoryVisualizationLanes.h',
    [string]$DocPath = 'Source/OGBrawlerUnreal/docs/InputHistoryDisplay-rationale.md',
    [switch]$EmitHex
)

$ErrorActionPreference = 'Stop'

function Resolve-InputPath {
    param([string]$Path)
    if ([IO.Path]::IsPathRooted($Path)) { return $Path }
    return (Join-Path $RepoRoot $Path)
}

$barsPath  = Resolve-InputPath $BarsHeaderPath
$corePath  = Resolve-InputPath $CoreHeaderPath
$lanesPath = Resolve-InputPath $LanesHeaderPath
$docPath   = Resolve-InputPath $DocPath

foreach ($p in @($barsPath, $corePath, $lanesPath, $docPath)) {
    if (-not (Test-Path -LiteralPath $p)) {
        Write-Host "ERROR: file not found: $p"
        exit 2
    }
}

$barsText  = [IO.File]::ReadAllText($barsPath)
$coreText  = [IO.File]::ReadAllText($corePath)
$lanesText = [IO.File]::ReadAllText($lanesPath)
$docLines  = [IO.File]::ReadAllLines($docPath)

# ---------------------------------------------------------------------------
# 1. Parse the header side.
# ---------------------------------------------------------------------------

# Returns the text strictly BETWEEN a function's opening and matching closing brace,
# found by depth-counting rather than a non-greedy regex — the switch itself contains
# nested `{ }` (the LaneCellStyle braced-init), which a lazy match would clip early.
function Get-FunctionBody {
    param([string]$Text, [string]$Signature)
    $m = [regex]::Match($Text, [regex]::Escape($Signature) + '\s*\{')
    if (-not $m.Success) { return $null }
    $start = $m.Index + $m.Length
    $depth = 1
    $i = $start
    while ($i -lt $Text.Length -and $depth -gt 0) {
        if ($Text[$i] -eq '{') { $depth++ }
        elseif ($Text[$i] -eq '}') { $depth-- }
        $i++
    }
    if ($depth -ne 0) { return $null }
    return $Text.Substring($start, $i - 1 - $start)
}

# One `case Enum::Name: return { LaneCellFill::Kind, { r, g, b } };` (or `{}` for a hole).
$caseRx = [regex]'(?ms)case\s+(?<enum>\w+)::(?<name>\w+):\s*\r?\n\s*return\s*\{\s*LaneCellFill::(?<fill>\w+)\s*,\s*(?<rgb>\{[^{}]*\})\s*\};'
$floatRx = [regex]'(?<v>[0-9]*\.?[0-9]+)f'

function Get-Arms {
    param([string]$Text, [string]$Signature)
    $body = Get-FunctionBody -Text $Text -Signature $Signature
    if ($null -eq $body) { return $null }
    $arms = [System.Collections.Generic.List[object]]::new()
    foreach ($m in $caseRx.Matches($body)) {
        $rgbRaw = $m.Groups['rgb'].Value
        $floats = @($floatRx.Matches($rgbRaw) | ForEach-Object { [double]$_.Groups['v'].Value })
        $arms.Add([pscustomobject]@{
            EnumType = $m.Groups['enum'].Value
            Name     = $m.Groups['name'].Value
            Fill     = $m.Groups['fill'].Value
            RGB      = $floats
        })
    }
    return $arms
}

# --- BEGIN SRGB SHARED FUNCTIONS ---
# ---------------------------------------------------------------------------
# sRGB conversion — the ONE function the emit mode and the hex-check arm both call.
# Two implementations of this formula would drift silently the first time anyone
# retuned a hue; there is exactly one, and the Pester round-trip test extracts THIS
# exact block (up to the sentinel below) to prove it is calling the real thing.
#
# Per channel: s = 12.92·L when L <= 0.0031308, else s = 1.055·L^(1/2.4) - 0.055;
# then round(s * 255), clamped to 0..255.
#
# ⭐ ROUNDING CONVENTION, PINNED: round-half-AWAY-FROM-ZERO, via `floor(x + 0.5)` —
# NOT `[math]::Round`, whose PowerShell/.NET default is banker's rounding (half-to-
# even) and would silently disagree with this on a future exact .5 tie. None of the
# 22 colours in the shipped palette land on an exact tie today (the closest is
# 0.50f's channel, at fractional 0.5160 — not a tie), so this choice changes no
# value in this doc; the Pester spec pins a SYNTHETIC tie (L = 2.5 / (255 * 12.92),
# which lands on exactly s*255 == 2.5) to prove which convention actually shipped,
# since the real palette does not offer one.
function ConvertTo-SRGB8 {
    param([double]$Linear)
    $s = if ($Linear -le 0.0031308) { 12.92 * $Linear } else { 1.055 * [Math]::Pow($Linear, 1.0 / 2.4) - 0.055 }
    $rounded = [Math]::Floor(($s * 255) + 0.5)
    if ($rounded -lt 0)   { return 0 }
    if ($rounded -gt 255) { return 255 }
    return [int]$rounded
}

function ConvertTo-SRGBHex {
    param([double[]]$RGB)
    $bytes = @($RGB | ForEach-Object { ConvertTo-SRGB8 $_ })
    return ('#{0:X2}{1:X2}{2:X2}' -f $bytes[0], $bytes[1], $bytes[2])
}
# --- END SRGB SHARED FUNCTIONS ---

$palettes = @(
    @{ Key = 'provenance'; Signature = 'constexpr LaneCellStyle provenanceCellStyleOf(RowProvenanceSummary summary)'
       CountName = 'kRowProvenanceSummaryCount'; CountText = $coreText
       TableTitle = 'Provenance palette' }
    @{ Key = 'machine'; Signature = 'constexpr LaneCellStyle machineCellStyleOf(MachineStateCell cell)'
       CountName = 'kMachineStateCellCount'; CountText = $lanesText
       TableTitle = 'Machine-state palette' }
    @{ Key = 'delay'; Signature = 'constexpr LaneCellStyle delayVerdictStyleOf(InputDelayVerdict verdict)'
       CountName = 'kInputDelayVerdictCount'; CountText = $lanesText
       TableTitle = 'Input-delay verdict palette' }
)

$errors = [System.Collections.Generic.List[string]]::new()

foreach ($p in $palettes) {
    $arms = Get-Arms -Text $barsText -Signature $p.Signature
    if ($null -eq $arms) {
        Write-Host "ERROR: could not locate '$($p.Signature)' in $barsPath"
        exit 2
    }
    $p.Arms = $arms

    $countRx = [regex]('inline constexpr \S+ ' + [regex]::Escape($p.CountName) + '\s*=\s*(?<n>\d+)u;')
    $cm = $countRx.Match($p.CountText)
    if (-not $cm.Success) {
        Write-Host "ERROR: could not find $($p.CountName) — the constant this table's row count is checked against"
        exit 2
    }
    $p.Count = [int]$cm.Groups['n'].Value
}

# The colours that belong to no enumerator: module-level `inline constexpr
# LaneCellColor NAME{ r, g, b };`, no `=`, no switch arm, no palette of their own.
$moduleColorRx = [regex]'inline constexpr LaneCellColor (?<name>\w+)\{\s*(?<rgb>[^}]*)\}\s*;'
$moduleColors = @{}
foreach ($m in $moduleColorRx.Matches($barsText)) {
    $floats = @($floatRx.Matches($m.Groups['rgb'].Value) | ForEach-Object { [double]$_.Groups['v'].Value })
    $moduleColors[$m.Groups['name'].Value] = $floats
}
foreach ($name in @('kUnnamedLaneColor', 'kLaneElisionColor', 'kLaneResyncColor')) {
    if (-not $moduleColors.ContainsKey($name)) {
        Write-Host "ERROR: could not find $name in $barsPath"
        exit 2
    }
}

# ---------------------------------------------------------------------------
# EMIT MODE — prints the generated hex column and stops. Reads only the header
# (already parsed above); never opens $docPath. GENERATED, not hand-typed: this is
# the output a human pastes into §7.11's `On screen (sRGB hex)` column.
# ---------------------------------------------------------------------------
if ($EmitHex) {
    Write-Host "GENERATED 'On screen (sRGB hex)' -- from $barsPath's own RGB floats, via ConvertTo-SRGBHex."
    Write-Host 'Hand-typing this column would reintroduce the drift this arm exists to prevent.'
    Write-Host ''
    foreach ($p in $palettes) {
        Write-Host ("{0}:" -f $p.Key)
        foreach ($a in $p.Arms) {
            $label = "  {0}::{1}" -f $a.EnumType, $a.Name
            if ($a.Fill -eq 'Hole') {
                Write-Host ("{0,-58}— (hole; draws nothing)" -f $label)
            }
            else {
                $rgbText = ($a.RGB | ForEach-Object { $_.ToString('0.00', [System.Globalization.CultureInfo]::InvariantCulture) + 'f' }) -join ', '
                $hex = ConvertTo-SRGBHex $a.RGB
                Write-Host ("{0,-58}{1}  ->  {2}" -f $label, $rgbText, $hex)
            }
        }
    }
    Write-Host 'non-enum:'
    foreach ($name in @('kLaneElisionColor', 'kLaneResyncColor', 'kUnnamedLaneColor')) {
        $label = "  {0}" -f $name
        $rgbText = ($moduleColors[$name] | ForEach-Object { $_.ToString('0.00', [System.Globalization.CultureInfo]::InvariantCulture) + 'f' }) -join ', '
        $hex = ConvertTo-SRGBHex $moduleColors[$name]
        Write-Host ("{0,-58}{1}  ->  {2}" -f $label, $rgbText, $hex)
    }
    exit 0
}

# ---------------------------------------------------------------------------
# 2. Parse the doc side — four markdown tables inside §7.11.
#
# A table is identified by its own preceding `#### <title>` heading (matched against
# TableTitle above), not by position, so the four tables may be reordered or have prose
# between them without breaking the parse. Rows are ordinary GFM table rows; the
# Enumerator cell holds `` `Type::Name` `` (qualified, so doc_anchor_lint's QSYM rule
# also resolves it against the header) or, for the fourth table, `` `kName` ``. The RGB
# cell holds either three float literals written exactly as the header spells them
# (`0.42f, 0.42f, 0.42f`) or a hole marker (any cell not parsing as three floats). The
# THIRD cell is the `On screen (sRGB hex)` column — a `#RRGGBB` token, or the same hole
# marker convention when the row is a hole (a hole draws nothing, so it has no hex
# either). The fourth cell, `Colour`, is plain prose and is never parsed — nothing
# mechanical reads it, per §7.11's own what-is-not-checked paragraph.
# ---------------------------------------------------------------------------
function Get-TableRows {
    param([string[]]$Lines, [string]$Heading)
    $headingRx = [regex]('^####\s+' + [regex]::Escape($Heading) + '\s*$')
    $start = -1
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        if ($headingRx.IsMatch($Lines[$i])) { $start = $i; break }
    }
    if ($start -lt 0) { return $null }

    $rows = [System.Collections.Generic.List[object]]::new()
    $inTable = $false
    for ($i = $start + 1; $i -lt $Lines.Count; $i++) {
        $line = $Lines[$i]
        if (-not $line.TrimStart().StartsWith('|')) {
            if ($inTable) { break }   # the table has ended
            if ($line -match '^####\s') { break }   # no table under this heading at all
            continue
        }
        $inTable = $true
        $cells = @($line.Trim().Trim('|') -split '\|' | ForEach-Object { $_.Trim() })
        # Skip the header row and the `|---|---|---|` separator row.
        if ($cells[0] -match '^:?-+:?$') { continue }
        if ($cells.Count -ge 1 -and $cells[0] -notmatch '^`') { continue }
        $rows.Add($cells)
    }
    return $rows
}

$tokenRx = [regex]'^`(?<t>[^`]+)`$'
$hexCellRx = [regex]'#(?<hex>[0-9A-Fa-f]{6})'

foreach ($p in $palettes) {
    $rows = Get-TableRows -Lines $docLines -Heading $p.TableTitle
    if ($null -eq $rows) {
        Write-Host "ERROR: no '#### $($p.TableTitle)' table found in $docPath"
        exit 2
    }
    $parsed = [System.Collections.Generic.List[object]]::new()
    foreach ($cells in $rows) {
        $tm = $tokenRx.Match($cells[0])
        if (-not $tm.Success) { continue }
        $qualified = $tm.Groups['t'].Value
        $seg = $qualified -split '::'
        $rgbCell = $cells[1]
        $floats = @($floatRx.Matches($rgbCell) | ForEach-Object { [double]$_.Groups['v'].Value })
        $isHole = ($floats.Count -ne 3)
        $hexCell = if ($cells.Count -gt 2) { $cells[2] } else { '' }
        $hexMatch = $hexCellRx.Match($hexCell)
        $hexIsHole = -not $hexMatch.Success
        $hexValue = if ($hexMatch.Success) { '#' + $hexMatch.Groups['hex'].Value.ToUpperInvariant() } else { $null }
        $parsed.Add([pscustomobject]@{
            EnumType = $seg[0]; Name = $seg[-1]; RGB = $floats; IsHole = $isHole
            Qualified = $qualified; Hex = $hexValue; HexIsHole = $hexIsHole
        })
    }
    $p.DocRows = $parsed
}

$nonEnumRows = Get-TableRows -Lines $docLines -Heading 'Non-enumerator markers'
if ($null -eq $nonEnumRows) {
    Write-Host "ERROR: no '#### Non-enumerator markers' table found in $docPath"
    exit 2
}
$nonEnumParsed = @{}
$nonEnumHex = @{}
foreach ($cells in $nonEnumRows) {
    $tm = $tokenRx.Match($cells[0])
    if (-not $tm.Success) { continue }
    $floats = @($floatRx.Matches($cells[1]) | ForEach-Object { [double]$_.Groups['v'].Value })
    $nonEnumParsed[$tm.Groups['t'].Value] = $floats
    $hexCell = if ($cells.Count -gt 2) { $cells[2] } else { '' }
    $hexMatch = $hexCellRx.Match($hexCell)
    $nonEnumHex[$tm.Groups['t'].Value] = if ($hexMatch.Success) { '#' + $hexMatch.Groups['hex'].Value.ToUpperInvariant() } else { $null }
}

# ---------------------------------------------------------------------------
# 3. Diff.
# ---------------------------------------------------------------------------
function Compare-RGB {
    param([double[]]$A, [double[]]$B)
    if ($A.Count -ne 3 -or $B.Count -ne 3) { return $false }
    for ($i = 0; $i -lt 3; $i++) {
        if ([Math]::Abs($A[$i] - $B[$i]) -gt 0.0001) { return $false }
    }
    return $true
}

$parsedCounts = [ordered]@{}

foreach ($p in $palettes) {
    $armByName = @{}
    foreach ($a in $p.Arms) { $armByName[$a.Name] = $a }
    $rowByName = @{}
    foreach ($r in $p.DocRows) { $rowByName[$r.Name] = $r }

    foreach ($a in $p.Arms) {
        if (-not $rowByName.ContainsKey($a.Name)) {
            $errors.Add("$($p.Key): enumerator '$($a.EnumType)::$($a.Name)' has a switch arm but no §7.11 table row")
            continue
        }
        $r = $rowByName[$a.Name]
        if ($r.EnumType -ne $a.EnumType) {
            $errors.Add("$($p.Key): row '$($r.Qualified)' names the wrong enum type (expected $($a.EnumType))")
        }
        $expectHole = ($a.Fill -eq 'Hole')
        if ($expectHole -ne $r.IsHole) {
            $errors.Add("$($p.Key): '$($a.EnumType)::$($a.Name)' is $($a.Fill) in the header but the table row is $(if ($r.IsHole) {'a hole'} else {'a colour'})")
        }
        elseif (-not $expectHole -and -not (Compare-RGB $a.RGB $r.RGB)) {
            $errors.Add("$($p.Key): '$($a.EnumType)::$($a.Name)' RGB mismatch — header ($($a.RGB -join ', ')) vs table ($($r.RGB -join ', '))")
        }
        # The hex arm — GENERATED from the header's own RGB via ConvertTo-SRGBHex, the
        # SAME function -EmitHex calls, then compared against what the doc actually has.
        # A float retuned without regenerating its hex fails here even when the RGB
        # check above stays quiet about anything else, because this re-derives from
        # $a.RGB (the header's own floats), never from $r.RGB (the doc's own copy).
        if ($expectHole -ne $r.HexIsHole) {
            $errors.Add("$($p.Key): '$($a.EnumType)::$($a.Name)' is $($a.Fill) in the header but the hex column is $(if ($r.HexIsHole) {'a hole'} else {'a colour'})")
        }
        elseif (-not $expectHole) {
            $expectedHex = ConvertTo-SRGBHex $a.RGB
            if ($r.Hex -ne $expectedHex) {
                $errors.Add("$($p.Key): '$($a.EnumType)::$($a.Name)' hex mismatch — header floats ($($a.RGB -join ', ')) generate $expectedHex but the table says $($r.Hex)")
            }
        }
    }
    foreach ($r in $p.DocRows) {
        if (-not $armByName.ContainsKey($r.Name)) {
            $errors.Add("$($p.Key): table row '$($r.Qualified)' names no arm in $($p.Signature)")
        }
    }

    $parsedCounts[$p.Key] = $p.DocRows.Count
    if ($p.DocRows.Count -ne $p.Count) {
        $errors.Add("$($p.Key): §7.11 table has $($p.DocRows.Count) row(s) but $($p.CountName) is $($p.Count)")
    }
    if ($p.Arms.Count -ne $p.Count) {
        $errors.Add("$($p.Key): $($p.Signature) has $($p.Arms.Count) case arm(s) but $($p.CountName) is $($p.Count)")
    }
}

foreach ($name in @('kUnnamedLaneColor', 'kLaneElisionColor', 'kLaneResyncColor')) {
    if (-not $nonEnumParsed.ContainsKey($name)) {
        $errors.Add("non-enumerator markers: '$name' has no §7.11 table row")
        continue
    }
    if (-not (Compare-RGB $moduleColors[$name] $nonEnumParsed[$name])) {
        $errors.Add("non-enumerator markers: '$name' RGB mismatch — header ($($moduleColors[$name] -join ', ')) vs table ($($nonEnumParsed[$name] -join ', '))")
    }
    $expectedHex = ConvertTo-SRGBHex $moduleColors[$name]
    if ($nonEnumHex[$name] -ne $expectedHex) {
        $errors.Add("non-enumerator markers: '$name' hex mismatch — header floats generate $expectedHex but the table says $($nonEnumHex[$name])")
    }
}
foreach ($name in $nonEnumParsed.Keys) {
    if (-not $moduleColors.ContainsKey($name)) {
        $errors.Add("non-enumerator markers: table row '$name' names no module-level colour in $barsPath")
    }
}

# ---------------------------------------------------------------------------
# 4. Report. The three parsed counts are ALWAYS printed, clean or not — a lint that
#    only speaks on failure is exactly the silent-shrink risk VISUALIZATION_DISCIPLINE.md
#    already flags for its own file list.
# ---------------------------------------------------------------------------
Write-Host 'parsed table row counts:'
foreach ($k in $parsedCounts.Keys) { Write-Host ("    {0,-12} {1}" -f $k, $parsedCounts[$k]) }
Write-Host ("    {0,-12} {1}" -f 'non-enum', $nonEnumParsed.Count)
Write-Host ''
Write-Host 'NOT CHECKED: the MEANING column is prose. A clean run proves the enumerator names, RGB'
Write-Host 'values, the derived On-screen sRGB hex, and LaneCellFill kinds agree between the header'
Write-Host 'and §7.11 — it does not prove any Meaning sentence, or the plain-English Colour word, is'
Write-Host 'true. Those two columns are prose; nothing mechanical reads either of them.'

if ($errors.Count -eq 0) {
    Write-Host ''
    Write-Host 'RESULT: CLEAN — every switch arm and every table row agree (RGB, hex, fill kind), and every parsed count matches its governing constant.'
    exit 0
}
Write-Host ''
Write-Host "RESULT: $($errors.Count) MISMATCH(ES)"
foreach ($e in $errors) { Write-Host "  $e" }
exit 1
