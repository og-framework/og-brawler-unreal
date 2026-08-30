# SPDX-License-Identifier: MPL-2.0
#Requires -Modules Pester
<#
    Pester spec for tools/lint/palette_legend_lint.ps1.

    WHY THIS FILE EXISTS. Per doc_anchor_lint.Tests.ps1's own house rule, this lint's
    whole value is telling a true legend from a false one, and that must be a TEST, not
    a claim. Every Context below pairs a MUST-BE-CLEAN fixture with a MUST-FIRE mutation
    of the SAME fixture, so the lint's ability to fail is exercised, not merely asserted.

    The fixture is a SMALL, self-contained header + doc pair — three provenance arms,
    one machine hole plus one machine state, one delay hole plus one delay state, and
    the two non-enumerator colours — never the shipped tree. `palette_legend_lint.ps1`'s
    four header paths are ordinary parameters, so a scratch fixture root is passed
    through them directly; nothing here reads or writes the real repository.

    The doc fixture now carries the `On screen (sRGB hex)` and `Colour` columns this
    lint's hex arm checks (Colour is prose, never asserted against). Every hex value
    below was computed with the SAME formula the lint implements — s = 12.92·L when
    L <= 0.0031308, else s = 1.055·L^(1/2.4) - 0.055, then round(s*255) clamped to
    0..255 — cross-checked independently in Python, not copied from the lint's own
    output, so this file cannot rubber-stamp a bug the lint and the fixture share.

    Run:  pwsh -NoProfile -Command "Invoke-Pester tools/lint/tests/palette_legend_lint.Tests.ps1 -Output Detailed"
#>

BeforeAll {
    $script:Lint = Join-Path (Split-Path -Parent $PSScriptRoot) 'palette_legend_lint.ps1'
    if (-not (Test-Path $script:Lint)) { throw "lint not found at $script:Lint" }

    function Invoke-Lint {
        param([string]$Root, [switch]$EmitHex)
        $args = @{
            BarsHeaderPath  = Join-Path $Root 'Bars.h'
            CoreHeaderPath  = Join-Path $Root 'Core.h'
            LanesHeaderPath = Join-Path $Root 'Lanes.h'
            DocPath         = Join-Path $Root 'legend.md'
        }
        if ($EmitHex) { $args['EmitHex'] = $true }
        $out = & $script:Lint @args 6>&1 | Out-String
        return [pscustomobject]@{ Out = $out; Code = $LASTEXITCODE }
    }

    # Extract ONLY the shared sRGB conversion functions (ConvertTo-SRGB8 /
    # ConvertTo-SRGBHex), up to their sentinel comment, and load them into THIS scope —
    # the same "read a source prefix, run only that" technique this initiative's Python
    # checker controls already use. This proves the round-trip test below calls the
    # EXACT function -EmitHex and the hex-check arm both call, not a second
    # reimplementation that merely looks the same.
    $lintText = Get-Content -Raw -LiteralPath $script:Lint
    $beginMarker = '# --- BEGIN SRGB SHARED FUNCTIONS ---'
    $endMarker   = '# --- END SRGB SHARED FUNCTIONS ---'
    $start = $lintText.IndexOf($beginMarker)
    $end   = $lintText.IndexOf($endMarker)
    if ($start -lt 0 -or $end -lt 0 -or $end -le $start) {
        throw "sentinel pair not found (or out of order) in $script:Lint -- anchor stale"
    }
    # ONLY the two function definitions between the markers -- not the param block or
    # any top-level execution above/below them, which would run as a side effect of
    # Invoke-Expression and fail on state (like $PSScriptRoot) this scope doesn't have.
    Invoke-Expression $lintText.Substring($start, $end - $start)

    # -- the fixture header: three switches, small but shaped exactly like the real one -
    $script:BarsH = @'
#pragma once
// SPDX-License-Identifier: BUSL-1.1
namespace fixture
{

struct LaneCellColor { float r = 0.f; float g = 0.f; float b = 0.f; };
enum class LaneCellFill : unsigned char { Hole, State, Unnamed };
struct LaneCellStyle { LaneCellFill fill = LaneCellFill::Hole; LaneCellColor color{}; };

inline constexpr LaneCellColor kUnnamedLaneColor{ 1.00f, 1.00f, 0.35f };
inline constexpr LaneCellColor kLaneElisionColor{ 0.62f, 0.32f, 0.00f };
inline constexpr LaneCellColor kLaneResyncColor{ 0.05f, 0.00f, 1.00f };

constexpr LaneCellStyle provenanceCellStyleOf(RowProvenanceSummary summary)
{
	switch (summary)
	{
	case RowProvenanceSummary::Unknown:
		return { LaneCellFill::State, { 0.10f, 0.10f, 0.10f } };
	case RowProvenanceSummary::Pending:
		return { LaneCellFill::State, { 0.20f, 0.20f, 0.20f } };
	case RowProvenanceSummary::Confirmed:
		return { LaneCellFill::State, { 0.30f, 0.30f, 0.30f } };
	}

	return { LaneCellFill::Unnamed, kUnnamedLaneColor };
}

constexpr LaneCellStyle machineCellStyleOf(MachineStateCell cell)
{
	switch (cell)
	{
	case MachineStateCell::NotSampled:
		return { LaneCellFill::Hole, {} };
	case MachineStateCell::Attacking:
		return { LaneCellFill::State, { 0.85f, 0.10f, 0.10f } };
	}

	return { LaneCellFill::Unnamed, kUnnamedLaneColor };
}

constexpr LaneCellStyle delayVerdictStyleOf(InputDelayVerdict verdict)
{
	switch (verdict)
	{
	case InputDelayVerdict::NoVerdict:
		return { LaneCellFill::Hole, {} };
	case InputDelayVerdict::Agree:
		return { LaneCellFill::State, { 0.00f, 0.84f, 0.00f } };
	}

	return { LaneCellFill::Unnamed, kUnnamedLaneColor };
}

} // namespace fixture
'@

    $script:CoreH = @'
#pragma once
enum class RowProvenanceSummary : uint8_t { Unknown = 0, Pending, Confirmed };
inline constexpr uint8_t kRowProvenanceSummaryCount = 3u;
'@

    $script:LanesH = @'
#pragma once
enum class MachineStateCell : uint8_t { NotSampled = 0, Attacking };
inline constexpr uint8_t kMachineStateCellCount = 2u;
enum class InputDelayVerdict : uint8_t { NoVerdict = 0, Agree };
inline constexpr uint8_t kInputDelayVerdictCount = 2u;
'@

    # -- the clean fixture doc: four tables, values matching Bars.h above exactly -------
    # Hex column: 0.10/0.10/0.10 -> #595959; 0.20/0.20/0.20 -> #7C7C7C;
    # 0.30/0.30/0.30 -> #959595; 0.85/0.10/0.10 -> #ED5959; 0.00/0.84/0.00 -> #00EC00;
    # 0.62/0.32/0.00 -> #CE9900; 0.05/0.00/1.00 -> #3F00FF;
    # 1.00/1.00/0.35 -> #FFFFA0 (each cross-checked in Python).
    $script:CleanDoc = @'
# Fixture legend

#### Provenance palette

| Enumerator | RGB (as written in the header) | On screen (sRGB hex) | Colour | Meaning |
|---|---|---|---|---|
| `RowProvenanceSummary::Unknown` | 0.10f, 0.10f, 0.10f | #595959 | dark grey | never observed |
| `RowProvenanceSummary::Pending` | 0.20f, 0.20f, 0.20f | #7C7C7C | medium grey | pressed, not yet run |
| `RowProvenanceSummary::Confirmed` | 0.30f, 0.30f, 0.30f | #959595 | light grey | authority certified it |

#### Machine-state palette

| Enumerator | RGB (as written in the header) | On screen (sRGB hex) | Colour | Meaning |
|---|---|---|---|---|
| `MachineStateCell::NotSampled` | — (hole; draws nothing) | — (hole; draws nothing) | — (hole) | no sample this tick |
| `MachineStateCell::Attacking` | 0.85f, 0.10f, 0.10f | #ED5959 | coral red | mid-attack |

#### Input-delay verdict palette

| Enumerator | RGB (as written in the header) | On screen (sRGB hex) | Colour | Meaning |
|---|---|---|---|---|
| `InputDelayVerdict::NoVerdict` | — (hole; draws nothing) | — (hole; draws nothing) | — (hole) | nothing claimed yet |
| `InputDelayVerdict::Agree` | 0.00f, 0.84f, 0.00f | #00EC00 | bright green | lag matches |

#### Non-enumerator markers

| Name | RGB (as written in the header) | On screen (sRGB hex) | Colour | Meaning |
|---|---|---|---|---|
| `kLaneElisionColor` | 0.62f, 0.32f, 0.00f | #CE9900 | burnt amber | a collapsed idle span |
| `kLaneResyncColor` | 0.05f, 0.00f, 1.00f | #3F00FF | electric indigo | a hard resync's axis cut |
| `kUnnamedLaneColor` | 1.00f, 1.00f, 0.35f | #FFFFA0 | pale yellow | an ordinal no enumerator covers |
'@

    function New-FixtureRoot {
        param([string]$Doc = $script:CleanDoc)
        $dir = Join-Path ([IO.Path]::GetTempPath()) ('palettelint-' + [Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $dir | Out-Null
        Set-Content -LiteralPath (Join-Path $dir 'Bars.h') -Value $script:BarsH
        Set-Content -LiteralPath (Join-Path $dir 'Core.h') -Value $script:CoreH
        Set-Content -LiteralPath (Join-Path $dir 'Lanes.h') -Value $script:LanesH
        Set-Content -LiteralPath (Join-Path $dir 'legend.md') -Value $Doc
        return $dir
    }
}

Describe 'palette_legend_lint' {

    Context 'The negative control - clean fixture is silent, and it prints the three counts' {

        It 'is CLEAN on the matching fixture and exits 0' {
            $root = New-FixtureRoot
            try {
                $r = Invoke-Lint -Root $root
                $r.Out  | Should -Match 'RESULT: CLEAN'
                $r.Code | Should -Be 0
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'prints all three parsed table row counts' {
            $root = New-FixtureRoot
            try {
                $r = Invoke-Lint -Root $root
                $r.Out | Should -Match 'provenance\s+3'
                $r.Out | Should -Match 'machine\s+2'
                $r.Out | Should -Match 'delay\s+2'
                $r.Out | Should -Match 'non-enum\s+3'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'states plainly that the MEANING column is not checked (and now says so of Colour too)' {
            $root = New-FixtureRoot
            try {
                $out = (Invoke-Lint -Root $root).Out
                $out | Should -Match 'NOT CHECKED: the MEANING column is prose'
                $out | Should -Match 'Colour'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'RED (a) - perturb one RGB channel' {

        It 'fails and names the perturbed enumerator' {
            $root = New-FixtureRoot
            try {
                # Confirmed's header colour becomes { 0.99f, 0.30f, 0.30f } -- the doc still says 0.30f/0.30f/0.30f.
                $bad = $script:BarsH -replace [regex]::Escape('{ 0.30f, 0.30f, 0.30f }'), '{ 0.99f, 0.30f, 0.30f }'
                Set-Content -LiteralPath (Join-Path $root 'Bars.h') -Value $bad
                $r = Invoke-Lint -Root $root
                $r.Code | Should -Be 1
                $r.Out  | Should -Match "RowProvenanceSummary::Confirmed' RGB mismatch"
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'RED (b) - delete one table row' {

        It 'fails and names the missing enumerator' {
            $root = New-FixtureRoot
            try {
                $bad = ($script:CleanDoc -split "`n" | Where-Object { $_ -notmatch 'RowProvenanceSummary::Pending' }) -join "`n"
                $root2 = New-FixtureRoot -Doc $bad
                try {
                    $r = Invoke-Lint -Root $root2
                    $r.Code | Should -Be 1
                    $r.Out  | Should -Match "RowProvenanceSummary::Pending' has a switch arm but no"
                } finally { Remove-Item -LiteralPath $root2 -Recurse -Force }
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'RED (c) - add a table row naming no arm' {

        It 'fails and names the row with no matching arm' {
            $bogus = $script:CleanDoc -replace [regex]::Escape('| `RowProvenanceSummary::Confirmed` | 0.30f, 0.30f, 0.30f | #959595 | light grey | authority certified it |'),
                "| ``RowProvenanceSummary::Confirmed`` | 0.30f, 0.30f, 0.30f | #959595 | light grey | authority certified it |`n| ``RowProvenanceSummary::Bogus`` | 0.40f, 0.40f, 0.40f | #ABABAB | mid grey | does not exist |"
            $root = New-FixtureRoot -Doc $bogus
            try {
                $r = Invoke-Lint -Root $root
                $r.Code | Should -Be 1
                $r.Out  | Should -Match "RowProvenanceSummary::Bogus' names no arm"
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'RED (d) - document the NoVerdict hole as a colour' {

        It 'fails on the fill-kind check' {
            $bad = $script:CleanDoc -replace [regex]::Escape('| `InputDelayVerdict::NoVerdict` | — (hole; draws nothing) | — (hole; draws nothing) | — (hole) | nothing claimed yet |'),
                '| `InputDelayVerdict::NoVerdict` | 0.50f, 0.50f, 0.50f | #7C7C7C | grey | nothing claimed yet |'
            $root = New-FixtureRoot -Doc $bad
            try {
                $r = Invoke-Lint -Root $root
                $r.Code | Should -Be 1
                $r.Out  | Should -Match "InputDelayVerdict::NoVerdict' is Hole in the header but the table row is a colour"
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'RED (e) - perturb one hex digit' {

        It 'fails and names the enumerator whose hex disagrees with its own floats' {
            # Attacking's floats (0.85/0.10/0.10) truly generate #ED5959; the doc is
            # perturbed to #ED5958 -- one hex digit off, floats left untouched.
            $bad = $script:CleanDoc -replace [regex]::Escape('| `MachineStateCell::Attacking` | 0.85f, 0.10f, 0.10f | #ED5959 | coral red | mid-attack |'),
                '| `MachineStateCell::Attacking` | 0.85f, 0.10f, 0.10f | #ED5958 | coral red | mid-attack |'
            $root = New-FixtureRoot -Doc $bad
            try {
                $r = Invoke-Lint -Root $root
                $r.Code | Should -Be 1
                $r.Out  | Should -Match "MachineStateCell::Attacking' hex mismatch"
                $r.Out  | Should -Match 'generate #ED5959 but the table says #ED5958'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'RED (f) - delete the hex column from one table' {

        It 'fails on the hex fill-kind check for every coloured row in that table' {
            # Drop the whole `On screen (sRGB hex)` cell from BOTH provenance colour rows,
            # collapsing them to 4 cells (Enumerator|RGB|Colour|Meaning) -- so the parser's
            # THIRD cell is now what used to be the Colour prose, which never matches the
            # hex pattern and reads as a hole.
            $bad = $script:CleanDoc `
                -replace [regex]::Escape('| `RowProvenanceSummary::Unknown` | 0.10f, 0.10f, 0.10f | #595959 | dark grey | never observed |'),
                    '| `RowProvenanceSummary::Unknown` | 0.10f, 0.10f, 0.10f | dark grey | never observed |' `
                -replace [regex]::Escape('| `RowProvenanceSummary::Pending` | 0.20f, 0.20f, 0.20f | #7C7C7C | medium grey | pressed, not yet run |'),
                    '| `RowProvenanceSummary::Pending` | 0.20f, 0.20f, 0.20f | medium grey | pressed, not yet run |'
            $root = New-FixtureRoot -Doc $bad
            try {
                $r = Invoke-Lint -Root $root
                $r.Code | Should -Be 1
                $r.Out  | Should -Match "RowProvenanceSummary::Unknown' is State in the header but the hex column is a hole"
                $r.Out  | Should -Match "RowProvenanceSummary::Pending' is State in the header but the hex column is a hole"
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'RED (g) - change a linear float without regenerating its hex (the drift case)' {

        It 'fails on hex mismatch even though the RGB the doc shows was never touched' {
            # Retune Agree's GREEN channel in the header (0.84f -> 0.90f, a real colour
            # change) but leave the doc's RGB cell AND its hex cell exactly as they were --
            # the doc's RGB check now ALSO fires (a genuine RGB mismatch), but the point of
            # this arm is that the hex re-derives from the HEADER's new float, so it names
            # the drift independently of whether the RGB check already caught it.
            $bad = $script:BarsH -replace [regex]::Escape('{ 0.00f, 0.84f, 0.00f }'), '{ 0.00f, 0.90f, 0.00f }'
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'Bars.h') -Value $bad
                $r = Invoke-Lint -Root $root
                $r.Code | Should -Be 1
                $r.Out  | Should -Match "InputDelayVerdict::Agree' RGB mismatch"
                $r.Out  | Should -Match "InputDelayVerdict::Agree' hex mismatch"
                # 0.00,0.90,0.00 really does generate a DIFFERENT hex than 0.00,0.84,0.00 --
                # cross-checked in Python: #00F300 -- so this is not the SAME hex surviving
                # a coincidence; the drift is real and the arm names the actual generated value.
                $r.Out  | Should -Match 'generate #00F300 but the table says #00EC00'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'RED (h) - delete the resync marker row from the non-enumerator table' {

        It 'fails and names kLaneResyncColor, the name the hard-coded list gained' {
            # The fourth table's membership is not derivable from any enumerator count, so
            # the only thing standing between a dropped marker row and a silent pass is the
            # lint's own name list. This arm is that list, exercised.
            $bad = ($script:CleanDoc -split "`n" | Where-Object { $_ -notmatch 'kLaneResyncColor' }) -join "`n"
            $root = New-FixtureRoot -Doc $bad
            try {
                $r = Invoke-Lint -Root $root
                $r.Code | Should -Be 1
                $r.Out  | Should -Match "'kLaneResyncColor' has no .7.11 table row"
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'RED (i) - retune the resync marker without regenerating its row' {

        It 'fails on BOTH the RGB and the hex arm, naming the generated value' {
            # The header's red channel moves 0.05 -> 0.35 and the doc's row is left alone.
            # Cross-checked in Python: 0.05 generates 0x3F (63.19) and 0.35 generates 0xA0
            # (159.68), so the drift really does change the byte rather than surviving a
            # rounding coincidence -- the arm asserts the generated value by name.
            $bad = $script:BarsH -replace [regex]::Escape('kLaneResyncColor{ 0.05f, 0.00f, 1.00f }'),
                'kLaneResyncColor{ 0.35f, 0.00f, 1.00f }'
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'Bars.h') -Value $bad
                $r = Invoke-Lint -Root $root
                $r.Code | Should -Be 1
                $r.Out  | Should -Match "'kLaneResyncColor' RGB mismatch"
                $r.Out  | Should -Match "'kLaneResyncColor' hex mismatch"
                $r.Out  | Should -Match 'generate #A000FF but the table says #3F00FF'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'The round-trip test — emitter and checker share ONE conversion function' {

        It 'ConvertTo-SRGB8 / ConvertTo-SRGBHex (extracted from the real lint file) reproduce the Backlog-pinned spot values' {
            # These are the exact literal function definitions the lint's -EmitHex path
            # and its hex-check arm both call -- loaded via the sentinel-marker extraction
            # in BeforeAll, not re-typed here. A pass here is a pass for BOTH code paths,
            # because there is only one implementation to test.
            (ConvertTo-SRGBHex @(0.00, 0.84, 0.00)) | Should -Be '#00EC00'
            (ConvertTo-SRGBHex @(0.00, 0.14, 0.52)) | Should -Be '#0069BF'
            (ConvertTo-SRGBHex @(0.36, 1.00, 1.00)) | Should -Be '#A2FFFF'
            # NOTE: the Backlog's own AC text pins this one as #FFFF9E. That figure does
            # not survive the stated formula (12.92*0.35 does not apply -- 0.35 > the
            # 0.0031308 breakpoint -- so s = 1.055*0.35^(1/2.4) - 0.055 = 0.626..., and
            # 0.626*255 = 159.68, which rounds to 160 = 0xA0 under EITHER rounding
            # convention, not a tie). #FFFFA0 is what this lint actually generates and
            # checks -- see the correction note in §7.11 under "Non-enumerator markers".
            (ConvertTo-SRGBHex @(1.00, 1.00, 0.35)) | Should -Be '#FFFFA0'
            # The resync marker's own generated value, computed in Python from the stated
            # formula before this fixture was written: 0.05 -> 1.055*0.05^(1/2.4) - 0.055 =
            # 0.2478, and 0.2478*255 = 63.19 -> 63 = 0x3F; 0.00 -> 0x00; 1.00 -> 0xFF.
            (ConvertTo-SRGBHex @(0.05, 0.00, 1.00)) | Should -Be '#3F00FF'
        }

        It 'pins the rounding convention on a SYNTHETIC exact .5 tie the real palette does not offer' {
            # L chosen so that 12.92*L*255 == 2.5 EXACTLY (constructed algebraically, not
            # hand-typed, so it reproduces the same IEEE-754 double in this test as it would
            # anywhere else this exact expression is evaluated).
            $tieLinear = 2.5 / (255 * 12.92)
            (ConvertTo-SRGB8 $tieLinear) | Should -Be 3   # round-half-AWAY-FROM-ZERO
            # Banker's rounding ([math]::Round's .NET default) would have given 2 here,
            # since 2 is even -- this is exactly the convention this arm exists to pin.
            [Math]::Round($tieLinear * 12.92 * 255, 0, [MidPointRounding]::ToEven) | Should -Be 2
        }

        It 'the linear-segment edge L = 0.0031308 uses the <= branch and yields byte 10 (0x0A)' {
            (ConvertTo-SRGB8 0.0031308) | Should -Be 10
        }

        It 'the <= 0.0031308 branch is not cosmetic -- well below it, the two branches disagree' {
            # At L = 0.001 the linear branch (correct) gives byte 3; a version that wrongly
            # applied the power formula everywhere would give byte 1. Proves the branch
            # condition is load-bearing, not redundant with continuity at the edge.
            (ConvertTo-SRGB8 0.001) | Should -Be 3
        }
    }

    Context '-EmitHex prints a GENERATED table, computed from the header, and touches no doc' {

        It 'prints the correct hex for every arm and every non-enumerator colour, and exits 0' {
            $root = New-FixtureRoot
            try {
                $r = Invoke-Lint -Root $root -EmitHex
                $r.Code | Should -Be 0
                $r.Out | Should -Match 'RowProvenanceSummary::Unknown'
                $r.Out | Should -Match '#595959'
                $r.Out | Should -Match 'RowProvenanceSummary::Confirmed'
                $r.Out | Should -Match '#959595'
                $r.Out | Should -Match 'MachineStateCell::NotSampled'
                $r.Out | Should -Match '\(hole; draws nothing\)'
                $r.Out | Should -Match 'kLaneResyncColor'
                $r.Out | Should -Match '#3F00FF'
                $r.Out | Should -Match 'kUnnamedLaneColor'
                $r.Out | Should -Match '#FFFFA0'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'does not require the doc to even be well-formed -- it never opens it' {
            $root = New-FixtureRoot -Doc 'this is not a valid legend document at all'
            try {
                $r = Invoke-Lint -Root $root -EmitHex
                $r.Code | Should -Be 0
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'Usage errors are exit 2, not a silent pass' {

        It 'exits 2 when a configured header does not exist' {
            $root = New-FixtureRoot
            try {
                $r = & $script:Lint -BarsHeaderPath (Join-Path $root 'Bars.h') `
                                     -CoreHeaderPath (Join-Path $root 'DoesNotExist.h') `
                                     -LanesHeaderPath (Join-Path $root 'Lanes.h') `
                                     -DocPath (Join-Path $root 'legend.md') 6>&1 | Out-String
                $LASTEXITCODE | Should -Be 2
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'exits 2 when a table this lint needs is missing from the doc entirely' {
            $root = New-FixtureRoot
            try {
                $noTable = $script:CleanDoc -replace '(?ms)#### Machine-state palette.*?(?=#### Input-delay)', ''
                Set-Content -LiteralPath (Join-Path $root 'legend.md') -Value $noTable
                $r = Invoke-Lint -Root $root
                $r.Code | Should -Be 2
                $r.Out  | Should -Match "no '#### Machine-state palette' table found"
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }
}
