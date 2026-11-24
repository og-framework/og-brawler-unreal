#Requires -Version 7.0
# SPDX-License-Identifier: MPL-2.0
<#
.SYNOPSIS
    Sim trigonometry lint - no libm trigonometry in the engine-free simulation trees, so every platform
    computes the same angles and rotations (og-input-latency task 22).

.DESCRIPTION
    Each platform's libm (UCRT, glibc, bionic) rounds sin, cos, acos and atan2 its own way, so a sim that
    calls them gives other bits on Win64, Linux and Android. og-sim's `simTrig` (OGSimulation/SimTrig.h)
    is the one implementation the sim uses: a scalar port of Jolt's Trigonometry.h built from IEEE add,
    multiply, divide and sqrt only.

    The lint scans every .h, .cpp and .inl under -ScanRoots (directory segments Binaries, Intermediate,
    ThirdParty, glm, docs, .git and build* skipped). Comments and string and char literals are removed
    first, keeping newlines. It flags:
      Unqualified    sin( cos( tan( asin( acos( atan( atan2( sincos( and their f, l and h forms, unless
                     preceded by `.`, `->` or `::`
      Qualified      std:: and glm:: (and a leading ::) forms of the same names
      GlmHelper      glm::rotate, rotateX/Y/Z, rotateNormalizedAxis, angleAxis, angle, axis, slerp,
                     eulerAngles, eulerAngle*, orientedAngle, pitch, yaw, roll: glm computes them with libm
      FMath          FMath::Sin, Cos, Tan, Asin, Acos, Atan, Atan2, SinCos, FastAsin
    Each hit prints `<relative file>:<line>: [SimTrig] ...` and the run exits 1.

    -Allow lists the files that are presentation only (they read sim state and write none), plus
    SimTrig.h itself. An entry is a path relative to -RepoRoot; `*` matches within one path segment.
    An entry that matches no scanned file fails the run, so the list cannot go stale. A new file in a
    scanned tree is checked by default.

    Exit 0 = clean. Exit 1 = a violation or a stale -Allow entry. Exit 2 = usage error, a missing root, or
    no file scanned.

.EXAMPLE
    pwsh -NoProfile -File tools/lint/sim_trig_lint.ps1
#>
[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..' '..')).Path,
    [string[]]$ScanRoots = @(
        'Plugins/OGSimulation/Source/OGSimulation/og-simulation/OGSimulation',
        'Plugins/OGSimulation/Source/OGSimulationJolt/og-simulation-jolt/OGSimulationJolt',
        'Plugins/OGSimulation/Source/OGSimulationNet/og-sim-net',
        'Plugins/OGSimulation/Source/OGSimulationSession/og-sim-session',
        'Plugins/OGBrawler/Source/OGBrawler/og-brawler/OGBrawler'
    ),
    [string[]]$Allow = @(
        'Plugins/OGSimulation/Source/OGSimulation/og-simulation/OGSimulation/SimTrig.h',
        'Plugins/OGSimulation/Source/OGSimulation/og-simulation/OGSimulation/RenderCorrectionSmoothing.h',
        'Plugins/OGSimulation/Source/OGSimulation/og-simulation/OGSimulation/RenderInterpolation.h',
        'Plugins/OGSimulation/Source/OGSimulation/og-simulation/OGSimulation/TickDriverPresenter.h',
        'Plugins/OGBrawler/Source/OGBrawler/og-brawler/OGBrawler/*Visualization*.h',
        'Plugins/OGBrawler/Source/OGBrawler/og-brawler/OGBrawler/DAttackCamera.h',
        'Plugins/OGBrawler/Source/OGBrawler/og-brawler/OGBrawler/DAttackCamera.cpp'
    )
)

$ErrorActionPreference = 'Stop'

function Exit-Usage {
    param([string]$Message)
    [Console]::Error.WriteLine("sim_trig_lint: $Message")
    exit 2
}

$RepoRoot = [IO.Path]::GetFullPath($RepoRoot)
if (-not [IO.Directory]::Exists($RepoRoot)) { Exit-Usage "repo root not found: $RepoRoot" }

$ExcludedSegments = @('Binaries', 'Intermediate', 'ThirdParty', 'glm', 'docs', '.git')
$Extensions = @('.h', '.cpp', '.inl')

function Get-RelPath {
    param([string]$Path)
    return [IO.Path]::GetRelativePath($RepoRoot, $Path).Replace('\', '/')
}

$files = [Collections.Generic.List[string]]::new()
$rootCounts = [Collections.Generic.List[string]]::new()
foreach ($r in $ScanRoots) {
    $before = $files.Count
    $full = [IO.Path]::GetFullPath([IO.Path]::Combine($RepoRoot, $r))
    if (-not [IO.Directory]::Exists($full)) { Exit-Usage "scan root not found: $r (under $RepoRoot)" }
    $stack = [Collections.Generic.Stack[string]]::new()
    $stack.Push($full)
    while ($stack.Count -gt 0) {
        $d = $stack.Pop()
        foreach ($f in [IO.Directory]::EnumerateFiles($d)) {
            if ($Extensions -contains [IO.Path]::GetExtension($f)) { $files.Add($f) }
        }
        foreach ($s in [IO.Directory]::EnumerateDirectories($d)) {
            $leaf = [IO.Path]::GetFileName($s)
            if ($ExcludedSegments -contains $leaf -or $leaf.StartsWith('build', [StringComparison]::OrdinalIgnoreCase)) { continue }
            $stack.Push($s)
        }
    }
    $rootCounts.Add("sim_trig_lint: root $r`: $($files.Count - $before) files")
}
$files.Sort([StringComparer]::Ordinal)
if ($files.Count -eq 0) { Exit-Usage 'no .h, .cpp or .inl file under the scan roots' }

function ConvertTo-AllowRegex {
    param([string]$Entry)
    $parts = $Entry.Replace('\', '/').Split('*')
    return [regex]::new('^' + (($parts | ForEach-Object { [regex]::Escape($_) }) -join '[^/]*') + '$')
}
$allowRx = @($Allow | ForEach-Object { [pscustomobject]@{ Entry = $_; Rx = (ConvertTo-AllowRegex $_); Hits = 0 } })

$StripRx = [regex]::new('//[^\n]*|/\*[\s\S]*?(?:\*/|\z)|"(?:\\.|[^"\\\n])*"?|(?<![A-Za-z0-9_])''(?:\\.|[^''\\\n])*''?')
$Names = '(?:a?sin|a?cos|a?tan|atan2|sincos)(?:h|f|l|hf|hl)?'
$Rules = @(
    [pscustomobject]@{ Arm = 'Unqualified'; Rx = [regex]::new("(?<![\w.>:])(?<!(?:::|\.|->)\s+)$Names\s*\(") }
    [pscustomobject]@{ Arm = 'Qualified'; Rx = [regex]::new("(?:\b(?:std|glm)\s*|(?<![\w>]))::\s*$Names\s*\(") }
    [pscustomobject]@{ Arm = 'GlmHelper'; Rx = [regex]::new('\bglm\s*::\s*(?:rotate[XYZ]?|rotateNormalizedAxis|angleAxis|angle|axis|slerp|eulerAngles|eulerAngle\w+|orientedAngle|pitch|yaw|roll)\s*\(') }
    [pscustomobject]@{ Arm = 'FMath'; Rx = [regex]::new('\bFMath\s*::\s*(?:Sin|Cos|Tan|Asin|Acos|Atan|Atan2|SinCos|FastAsin)\s*\(') }
)

$violations = 0
$scanned = 0
foreach ($f in $files) {
    $rel = Get-RelPath $f
    $allowed = $false
    foreach ($a in $allowRx) {
        if ($a.Rx.IsMatch($rel)) { $a.Hits++; $allowed = $true }
    }
    if ($allowed) { continue }
    $scanned++
    $text = [IO.File]::ReadAllText($f).Replace("`r`n", "`n")
    $code = $StripRx.Replace($text, {
        param($m)
        $v = $m.Value
        if ($v.StartsWith('/')) { return [regex]::Replace($v, '[^\n]', '') + ' ' }
        return "$($v[0])$($v[0])"
    })
    $starts = [Collections.Generic.List[int]]::new()
    $starts.Add(0)
    for ($i = 0; $i -lt $code.Length; $i++) { if ($code[$i] -eq "`n") { $starts.Add($i + 1) } }
    foreach ($rule in $Rules) {
        foreach ($m in $rule.Rx.Matches($code)) {
            $k = $starts.BinarySearch($m.Index)
            if ($k -lt 0) { $k = (-bnot $k) - 1 }
            $call = ($m.Value -replace '\s+', '')
            Write-Output "${rel}:$($k + 1): [SimTrig.$($rule.Arm)] libm trigonometry '$call' in a sim tree; use simTrig:: (OGSimulation/SimTrig.h), or add the file to -Allow if it is presentation only"
            $violations++
        }
    }
}

foreach ($a in $allowRx) {
    if ($a.Hits -eq 0) {
        Write-Output "$($a.Entry): [SimTrig.Allow] the -Allow entry matches no scanned file"
        $violations++
    }
}

$rootCounts | ForEach-Object { Write-Output $_ }
Write-Output "sim_trig_lint: $($files.Count) files found, $scanned scanned, $($files.Count - $scanned) allowed; $violations violation(s)"
if ($violations -gt 0) { exit 1 }
exit 0
