# SPDX-License-Identifier: MPL-2.0
#Requires -Modules Pester
<#
    Pester spec for tools/lint/configurability_lint.ps1 (og-session-runtime task 35: the knob table arm; the
    blacklist arm's commonLeadTicks = 3.65 entry, which og-time-dilation task 14 changed).

    Each poison is one edit of the clean fixture and must fire its own check alone: the knob table problems are
    counted by their message, and a poison that fires a second kind is a failure. Every negative control is the
    poisoned text where the lint must not look (a comment, a member access). The fixture is a small tree in
    TestDrive: with the minimal TimeConfig.h the self-lint needs, passed through -RepoRoot and -ScanRoots;
    nothing here reads or writes the repository tree.

    Run:  pwsh -NoProfile -Command "Invoke-Pester tools/lint/tests/configurability_lint.Tests.ps1 -Output Detailed"
#>

BeforeAll {
    $script:Lint = Join-Path (Split-Path -Parent $PSScriptRoot) 'configurability_lint.ps1'
    if (-not (Test-Path $script:Lint)) { throw "lint not found at $script:Lint" }
    $script:Pwsh = (Get-Process -Id $PID).Path

    $script:TimeConfig = "#pragma once`nstruct TimeConfig`n{`n`tint32_t predOffsetFloorTicks = 4;`n};`n"
    $script:TimeConfigPath = 'Plugins/OGSimulation/Source/OGSimulation/og-simulation/OGSimulation/PCTimeManagement/TimeConfig.h'

    $script:Rows = @'
#pragma once
#include "OGSimulation/Config/KnobTable.h"
struct Owner { int32_t ticks = 4; double gain = 0.5; };
constexpr int32_t clampTicks(int32_t v) { return v < 0 ? 0 : v; }
constexpr double clampGain(double v) { return v < 0.0 ? 0.0 : v; }
inline constexpr knobTable::KnobRow<Owner, int32_t> kTicksRow{
	.intake = { .section = "Fixture", .key = "FixtureTicks" },
	.tag    = "Fixture.Ticks",
	.name   = "ticks",
	.clamp  = &clampTicks,
	.apply  = [](Owner& o, int32_t v) { o.ticks = v; },
	.stored = [](const Owner& o) { return o.ticks; },
};
inline constexpr knobTable::KnobIntake<double> kGainIntake{ .section = "Fixture", .key = "FixtureGain" };
inline constexpr knobTable::KnobRow<Owner, double> kGainRow{
	.intake = kGainIntake,
	.tag    = "Fixture.Gain",
	.name   = "gain",
	.clamp  = &clampGain,
	.apply  = [](Owner& o, double v) { o.gain = v; },
	.stored = [](const Owner& o) { return o.gain; },
};
// knobTable::KnobRow<Owner, int32_t> kCommented{ .intake = { .section = "F", .key = "FixtureTicks" }, .tag = "T" };
inline int32_t otherKey(const Config& config) { return config.readInt("FixtureOther").value_or(0); }
'@

    $script:Reader = @'
#pragma once
// config.readInt("FixtureTicks") in a comment is not a read
inline void configure(TimeConfig& config) { config.commonLeadTicks = 3.65; }
'@

    function script:Invoke-Lint {
        param([hashtable]$Files)
        $root = Join-Path $TestDrive ([guid]::NewGuid().ToString('N'))
        $all = @{ $script:TimeConfigPath = $script:TimeConfig }
        foreach ($k in $Files.Keys) { $all[$k] = $Files[$k] }
        foreach ($k in $all.Keys) {
            $p = Join-Path $root $k
            New-Item -ItemType Directory -Force (Split-Path -Parent $p) | Out-Null
            [IO.File]::WriteAllText($p, $all[$k])
        }
        $out = & $script:Pwsh -NoProfile -Command "& '$script:Lint' -RepoRoot '$root' -ScanRoots 'Source','Plugins'; exit `$LASTEXITCODE" 2>&1
        $text = ($out -join "`n")
        return [pscustomobject]@{
            Exit        = $LASTEXITCODE
            Text        = $text
            NoClamp     = ([regex]::Matches($text, 'has no clamp')).Count
            NotFound    = ([regex]::Matches($text, 'its key was not found')).Count
            SecondRead  = ([regex]::Matches($text, 'and is read here too')).Count
            Unbalanced  = ([regex]::Matches($text, 'braces do not close')).Count
            Blacklisted = ([regex]::Matches($text, 'Synthesis Addendum Correction 5 binding rule')).Count
        }
    }

    function script:Get-Edited {
        param([string]$Text, [string]$Old, [string]$New)
        if (([regex]::Matches($Text, [regex]::Escape($Old))).Count -ne 1) { throw "anchor not found exactly once: $Old" }
        return $Text.Replace($Old, $New)
    }

    function script:Assert-Only {
        param($Result, [string]$Kind, [int]$Count = 1)
        foreach ($k in @('NoClamp', 'NotFound', 'SecondRead', 'Unbalanced', 'Blacklisted')) {
            $expected = if ($k -eq $Kind) { $Count } else { 0 }
            $Result.$k | Should -Be $expected -Because "$k after the $Kind poison"
        }
        $Result.Exit | Should -Be 1
    }
}

Describe 'configurability_lint: the knob table' {
    It 'is clean on the clean fixture and names both keys (an inline intake and a named one)' {
        $r = Invoke-Lint @{ 'Source/Fixture/Rows.h' = $script:Rows; 'Source/Fixture/Reader.h' = $script:Reader }
        $r.Text | Should -Match 'knob table: 2 row\(s\), 2 key\(s\): FixtureGain, FixtureTicks'
        $r.Text | Should -Match 'Configurability lint: clean \(3 files scanned\)'
        $r.Exit | Should -Be 0
    }

    It 'RED: fails a row without a clamp, and names its key' {
        $rows = Get-Edited $script:Rows "`t.clamp  = &clampTicks,`n" ''
        $r = Invoke-Lint @{ 'Source/Fixture/Rows.h' = $rows; 'Source/Fixture/Reader.h' = $script:Reader }
        $r.Text | Should -Match "Rows\.h:6: knob row for key 'FixtureTicks' has no clamp"
        Assert-Only $r 'NoClamp'
    }

    It 'fails a row whose clamp is nullptr' {
        $rows = Get-Edited $script:Rows '.clamp  = &clampGain,' '.clamp  = nullptr,'
        $r = Invoke-Lint @{ 'Source/Fixture/Rows.h' = $rows; 'Source/Fixture/Reader.h' = $script:Reader }
        $r.Text | Should -Match "knob row for key 'FixtureGain' has no clamp"
        Assert-Only $r 'NoClamp'
    }

    It 'fails a key that has a row and is also read directly, in another file' {
        $reader = $script:Reader + "`ninline int32_t ticks(const Config& config) { return *config.readInt(`"FixtureTicks`"); }`n"
        $r = Invoke-Lint @{ 'Source/Fixture/Rows.h' = $script:Rows; 'Source/Fixture/Reader.h' = $reader }
        $r.Text | Should -Match "Reader\.h:4: key 'FixtureTicks' has a knob row \(Source.Fixture.Rows\.h:6\) and is read here too"
        Assert-Only $r 'SecondRead'
    }

    It 'fails a row whose named intake is not in its file' {
        $rows = Get-Edited $script:Rows '.intake = kGainIntake,' '.intake = kMissingIntake,'
        $r = Invoke-Lint @{ 'Source/Fixture/Rows.h' = $rows; 'Source/Fixture/Reader.h' = $script:Reader }
        $r.Text | Should -Match 'knob table: 2 row\(s\), 1 key\(s\): FixtureTicks'
        Assert-Only $r 'NotFound'
    }

    It 'fails a row whose braces do not close' {
        $rows = Get-Edited $script:Rows "`t.stored = [](const Owner& o) { return o.gain; },`n};" "`t.stored = [](const Owner& o) { return o.gain; },`n"
        $r = Invoke-Lint @{ 'Source/Fixture/Rows.h' = $rows }
        Assert-Only $r 'Unbalanced'
    }
}

Describe 'configurability_lint: the blacklist arm still fires (commonLeadTicks = 3.65, TD-14)' {
    It 'fails a non-member commonLeadTicks = 3.65 outside ClockDilationController.h, and only that' {
        $reader = $script:Reader + "`ninline double lead() { double commonLeadTicks = 3.65; return commonLeadTicks; }`n"
        $r = Invoke-Lint @{ 'Source/Fixture/Rows.h' = $script:Rows; 'Source/Fixture/Reader.h' = $reader }
        $r.Text | Should -Match 'Reader\.h:4: commonLeadTicks default \(3\.65 ticks'
        Assert-Only $r 'Blacklisted'
    }

    It 'leaves the same value alone in ClockDilationController.h, its home' {
        $home_ = "#pragma once`nstruct ClockDilationConfig { double commonLeadTicks = 3.65; };`n"
        $r = Invoke-Lint @{ 'Source/Fixture/Rows.h' = $script:Rows; 'Source/Fixture/ClockDilationController.h' = $home_ }
        $r.Exit | Should -Be 0
    }
}
