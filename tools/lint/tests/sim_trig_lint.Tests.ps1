# SPDX-License-Identifier: MPL-2.0
#Requires -Modules Pester
<#
    Pester spec for tools/lint/sim_trig_lint.ps1 (og-input-latency task 22).

    Every arm has a mutation of ONE clean fixture that fires that arm's tag and no other tag, and every negative
    control (a comment, a string, a member call, a simTrig:: call, an allowed file) is the same call text in a
    place the lint must not flag. The fixture is a small tree in TestDrive:, passed through -RepoRoot,
    -ScanRoots and -Allow; nothing here reads or writes the repository tree.

    Run:  pwsh -NoProfile -Command "Invoke-Pester tools/lint/tests/sim_trig_lint.Tests.ps1 -Output Detailed"
#>

BeforeAll {
    $script:Lint = Join-Path (Split-Path -Parent $PSScriptRoot) 'sim_trig_lint.ps1'
    if (-not (Test-Path $script:Lint)) { throw "lint not found at $script:Lint" }
    $script:Pwsh = (Get-Process -Id $PID).Path

    $script:Clean = @'
#pragma once
#include "OGSimulation/SimTrig.h"
// glm::rotate(glm::mat4(1.f), a, axis) and std::acos(x) in a comment
/* std::atan2(y, x) in a block comment */
inline float clean(float a, const glm::vec2& v)
{
    const char* text = "std::sin(a) glm::acos(a)";
    const simTrig::SinCos sc = simTrig::sinCos(a);
    const float cosMaxSlope(sc.cos);
    return simTrig::acos(a) + simTrig::atan2(v.y, v.x) + sc.sin + cosMaxSlope + text[0];
}
'@

    function script:Invoke-Lint {
        param([hashtable]$Files, [string[]]$Allow = @('Sim/Allowed.h'))
        $root = Join-Path $TestDrive ([guid]::NewGuid().ToString('N'))
        foreach ($k in $Files.Keys) {
            $p = Join-Path $root $k
            New-Item -ItemType Directory -Force (Split-Path -Parent $p) | Out-Null
            [IO.File]::WriteAllText($p, $Files[$k])
        }
        $allowArg = if ($Allow.Count -gt 0) { ($Allow | ForEach-Object { "'$_'" }) -join ',' } else { '@()' }
        $out = & $script:Pwsh -NoProfile -Command "& '$script:Lint' -RepoRoot '$root' -ScanRoots 'Sim' -Allow $allowArg; exit `$LASTEXITCODE" 2>&1
        return [pscustomobject]@{ Exit = $LASTEXITCODE; Text = ($out -join "`n") }
    }

    function script:Get-Mutated {
        param([string]$Line)
        $anchor = "    return simTrig::acos(a)"
        if (([regex]::Matches($script:Clean, [regex]::Escape($anchor))).Count -ne 1) { throw 'anchor not found exactly once' }
        return $script:Clean.Replace($anchor, "    $Line`n$anchor")
    }

    $script:AllowedText = "#pragma once`ninline float viz(float a) { return std::sin(a) + glm::acos(a); }`n"
}

Describe 'sim_trig_lint' {
    It 'is clean on the clean fixture (comments, strings, simTrig:: calls, a cos-prefixed name and an allowed file)' {
        $r = Invoke-Lint @{ 'Sim/Clean.h' = $script:Clean; 'Sim/Allowed.h' = $script:AllowedText }
        $r.Text | Should -Match '2 files found, 1 scanned, 1 allowed; 0 violation'
        $r.Exit | Should -Be 0
    }

    It 'flags <Arm> for <Line>' -ForEach @(
        @{ Arm = 'Unqualified'; Line = 'const float u = cos(a);' }
        @{ Arm = 'Unqualified'; Line = 'const float u = atan2f(a, a);' }
        @{ Arm = 'Unqualified'; Line = 'const float u = sinh(a);' }
        @{ Arm = 'Qualified'; Line = 'const float u = std::acos(a);' }
        @{ Arm = 'Qualified'; Line = 'const float u = glm::sin(a);' }
        @{ Arm = 'Qualified'; Line = 'const float u = ::tanf(a);' }
        @{ Arm = 'Qualified'; Line = 'const float u = std :: atan2 (a, a);' }
        @{ Arm = 'GlmHelper'; Line = 'const glm::mat4 u = glm::rotate(glm::mat4(1.f), a, glm::vec3(0.f, 0.f, 1.f));' }
        @{ Arm = 'GlmHelper'; Line = 'const float u = glm::orientedAngle(v, v);' }
        @{ Arm = 'GlmHelper'; Line = 'const glm::quat u = glm::angleAxis(a, glm::vec3(0.f, 0.f, 1.f));' }
        @{ Arm = 'FMath'; Line = 'const float u = FMath::Atan2(a, a);' }
    ) {
        $r = Invoke-Lint @{ 'Sim/Clean.h' = (Get-Mutated $Line); 'Sim/Allowed.h' = $script:AllowedText }
        $r.Exit | Should -Be 1
        $r.Text | Should -Match "Sim/Clean\.h:10: \[SimTrig\.$Arm\]"
        ([regex]::Matches($r.Text, '\[SimTrig\.')).Count | Should -Be 1
    }

    It 'does not flag a member call or a qualified call of another namespace' -ForEach @(
        @{ Line = 'const float u = sc.cos + other::sin(a) + p->tan(a);' }
    ) {
        $r = Invoke-Lint @{ 'Sim/Clean.h' = (Get-Mutated $Line); 'Sim/Allowed.h' = $script:AllowedText }
        $r.Exit | Should -Be 0
    }

    It 'checks the allowed file once it leaves -Allow' {
        $r = Invoke-Lint @{ 'Sim/Clean.h' = $script:Clean; 'Sim/Allowed.h' = $script:AllowedText } -Allow @('Sim/Other.h')
        $r.Exit | Should -Be 1
        $r.Text | Should -Match 'Sim/Allowed\.h:2: \[SimTrig\.Qualified\]'
        $r.Text | Should -Match 'Sim/Other\.h: \[SimTrig\.Allow\]'
    }

    It 'matches * within one path segment' {
        $r = Invoke-Lint @{ 'Sim/Clean.h' = $script:Clean; 'Sim/FooVisualization.h' = $script:AllowedText } -Allow @('Sim/*Visualization*.h')
        $r.Exit | Should -Be 0
        $r = Invoke-Lint @{ 'Sim/Clean.h' = $script:Clean; 'Sim/Deep/FooVisualization.h' = $script:AllowedText } -Allow @('Sim/*Visualization*.h', 'Sim/Clean.h')
        $r.Exit | Should -Be 1
        $r.Text | Should -Match 'Sim/Deep/FooVisualization\.h:2:'
    }

    It 'checks a new file by default, and skips docs and ThirdParty directories' {
        $files = @{
            'Sim/Clean.h' = $script:Clean; 'Sim/Allowed.h' = $script:AllowedText
            'Sim/New/NewSystem.cpp' = "float f(float a) { return std::cos(a); }`n"
            'Sim/docs/Note.h' = "float f(float a) { return std::cos(a); }`n"
            'Sim/ThirdParty/Lib.h' = "float f(float a) { return std::cos(a); }`n"
        }
        $r = Invoke-Lint $files
        $r.Exit | Should -Be 1
        $r.Text | Should -Match 'Sim/New/NewSystem\.cpp:1: \[SimTrig\.Qualified\]'
        $r.Text | Should -Not -Match 'docs/Note|ThirdParty/Lib'
    }

    It 'refuses a run that scans no file' {
        $r = Invoke-Lint @{ 'Sim/readme.txt' = 'no sources' } -Allow @()
        $r.Exit | Should -Be 2
    }
}
