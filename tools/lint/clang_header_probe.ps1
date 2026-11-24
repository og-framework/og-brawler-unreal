#Requires -Version 7.0
# SPDX-License-Identifier: MPL-2.0
<#
.SYNOPSIS
    Clang header probe - every og-simulation, og-sim-net and og-sim-session header compiles on
    its own under
    clang-cl, and under the Android NDK's clang with libc++, so a clang-only or
    libc++-only error in a header is caught on Win64.

.DESCRIPTION
    MSVC accepts constructs that clang refuses, and the MSVC STL answers some
    trait checks that libc++ turns into hard errors. The Win64 builds (UBT, the
    standalone CMake on MSVC) and the other lints cannot see either; only a clang
    compile does. Two cases made this probe:

    - og-input-latency task 19b, the CLANG-CL ARM: `simFrame`'s private helper
      templates (`signedPick`, `signedPlace`, `place`) defined BELOW the member
      functions that call them. The header's namespace-scope `static_assert`s
      constant-evaluate those callers; clang then fails with "undefined function
      'signedPick<1, false>' cannot be used in a constant expression", and MSVC
      accepts both orders. SimulationFrame-guards.md G-02 is the prohibition.
    - og-input-latency task 19d, the LIBC++ ARM: `TickDriverRenderSink`'s
      return-type requirements spelled `std::convertible_to<std::optional<glm::vec3>>`.
      Its `ApplyReturnsVoid` self-check makes the compiler check
      `std::convertible_to<void, std::optional<glm::vec3>>`; the NDK r27c libc++
      `<optional>` fails that with "cannot form a reference to 'void'" instead of
      answering false, and the MSVC STL answers false. The header's
      `OptionalVec3Result` concept rejects `void` first
      (TickDriverConcepts-rationale.md section 5).

    og-input-latency task P5-10 added og-sim-net's headers (<SimNetRoot>/OGSimNet).
    Their TUs also get og-sim-net's include root (-SimNetRoot) and og-net's
    (-NetRoot, for their `OGNet/` includes). og-simulation's TUs keep only
    og-simulation's roots, so an og-simulation header that includes an og-net or
    og-sim-net header fails here too.

    og-session-runtime task 22 added og-sim-session's headers
    (<SessionRoot>/OGSimSession). Their TUs get og-sim-session's include root
    (-SessionRoot) as well as og-sim-net's and og-net's, so an og-sim-net header
    that includes an og-sim-session header fails here too.

    ------------------------------------------------------------------------
    WHAT IT RUNS
    ------------------------------------------------------------------------
    For each header, a one-line TU `#include "<header>"` written to a scratch
    directory, compiled by each arm in -Arms:

    clang-cl:
        clang-cl /nologo /std:c++20 /DOG_STANDALONE_BUILD /Zs
                 /clang:-fno-delayed-template-parsing
                 [-I<IncludeOverlay>] -I<SimRoot> -I<SimRoot>/glm
                 [-I<SimNetRoot> -I<NetRoot>] [-I<SessionRoot>]  <tu>.cpp

    libc++ (the NDK's clang++):
        clang++ --target=<AndroidTarget> -std=c++20 -stdlib=libc++ -fsyntax-only
                -DOG_STANDALONE_BUILD
                [-I<IncludeOverlay>] -I<SimRoot> -I<SimRoot>/glm
                [-I<SimNetRoot> -I<NetRoot>] [-I<SessionRoot>]  <tu>.cpp

    The bracketed -I<SimNetRoot> -I<NetRoot> are passed for og-sim-net and
    og-sim-session headers, -I<SessionRoot> for og-sim-session headers only.

    `/Zs` and `-fsyntax-only` are syntax-only modes: templates are instantiated
    and `static_assert`s are evaluated, nothing is code-generated.
    `-fno-delayed-template-parsing` is what Linux and Android clang always do;
    clang-cl's MSVC-style default delays template bodies to the end of the TU.
    In the clang-cl arm MS compatibility stays on, so the MSVC STL and Windows SDK
    headers parse. The libc++ arm uses the NDK's own sysroot and libc++, the
    standard library of the Android builds. Every (arm, header) compile runs in
    one parallel pool (one compiler per logical processor).

    A header PASSES an arm when that compiler exits 0. Warnings are not counted.

    ------------------------------------------------------------------------
    NO CLANG, NO NDK
    ------------------------------------------------------------------------
    Each arm skips on its own, and a skipped arm is not a failure:
    - clang-cl: when -ClangCl does not exist, and (for the default path only) no
      `clang-cl` is on PATH: `[clang-probe] skipped: no clang (<path>)`. A machine
      without the Visual Studio "C++ Clang tools" component.
    - libc++: when no NDK root is found, or the root has no
      toolchains/llvm/prebuilt/<host>/bin/clang++:
      `[clang-probe] libc++ skipped: no NDK (<what was looked at>)`. The NDK root
      is -NdkRoot, else the first set of the environment variables NDKROOT and
      NDK_ROOT (both written by the engine's Engine/Extras/Android/SetupAndroid.bat)
      and ANDROID_NDK_ROOT.
    When every arm skips, the probe exits 0.

    ------------------------------------------------------------------------
    KNOWN FAILURES (libc++ arm)
    ------------------------------------------------------------------------
    -LibcxxKnownFailures maps a header to a regular expression its libc++
    output must match. Such a header is reported, not counted, while it fails
    with a matching line:
        [clang-probe] libc++ known <header>: <first matching line>
    It FAILS the probe when it compiles (the entry is stale: remove it) or when
    it fails without a matching line (a new error). An entry naming a header that
    does not exist is an exit 2. The default is empty: every header must compile.

    ------------------------------------------------------------------------
    OUTPUT
    ------------------------------------------------------------------------
    Per failing header, `[clang-probe] FAIL <header> rc=<n>` (clang-cl) or
    `[clang-probe] libc++ FAIL <header> rc=<n>` (libc++), followed by its first
    -MaxErrorLines `error:` / `note:` lines, indented. Then, per arm that ran,
        [clang-probe] headers=N simnet=M session=S failed=F clang=<version> elapsed=S s
        [clang-probe] libc++ headers=N simnet=M session=S failed=F known=K clang=<version> ndk=<rev> target=<t> elapsed=S s
    (N counts every header, M the og-sim-net ones among them, S the og-sim-session
    ones; elapsed is the whole probe's). A seen-failing run on an old copy of a header:
    put the copy at <dir>/OGSimulation/<Header>.h (an og-sim-net header at
    <dir>/OGSimNet/<Header>.h, an og-sim-session header at
    <dir>/OGSimSession/<Header>.h) and pass `-IncludeOverlay <dir>`;
    the overlay is searched first by both arms, so every TU that includes that
    header sees the copy.

.PARAMETER RepoRoot
    Root of og-brawler-unreal. Default: two levels above this script.

.PARAMETER SimRoot
    og-simulation's include root (the directory that holds `OGSimulation/` and
    `glm/`), relative to -RepoRoot or absolute. Default:
    Plugins/OGSimulation/Source/OGSimulation/og-simulation.

.PARAMETER SimNetRoot
    og-sim-net's include root (the directory that holds `OGSimNet/`), relative to
    -RepoRoot or absolute. Default:
    Plugins/OGSimulation/Source/OGSimulationNet/og-sim-net. '' leaves og-sim-net
    out: no og-sim-net header is probed by default, and one named in -Headers is
    an exit 2.

.PARAMETER NetRoot
    og-net's include root (the directory that holds `OGNet/`), for og-sim-net's
    TUs, relative to -RepoRoot or absolute. Default:
    Plugins/OGSimulation/Source/OGNet/og-net. Not read when -SimNetRoot is ''.

.PARAMETER SessionRoot
    og-sim-session's include root (the directory that holds `OGSimSession/`),
    relative to -RepoRoot or absolute. Default:
    Plugins/OGSimulation/Source/OGSimulationSession/og-sim-session. '' leaves
    og-sim-session out, and so does -SimNetRoot '': og-sim-session depends on
    og-sim-net, so an explicit -SessionRoot with -SimNetRoot '' is an exit 2.

.PARAMETER Headers
    Headers to probe, as include paths under -SimRoot
    (e.g. `OGSimulation/SimulationFrame.h`), or under -SimNetRoot for a path that
    starts with `OGSimNet/` (e.g. `OGSimNet/SimNetChannel.h`), or under
    -SessionRoot for a path that starts with `OGSimSession/`. Default: every `*.h`
    under <SimRoot>/OGSimulation, then every `*.h` under <SimNetRoot>/OGSimNet,
    then every `*.h` under <SessionRoot>/OGSimSession.
    A header that does not exist is an exit 2.

.PARAMETER Arms
    The arms to run: `clang-cl`, `libc++` or both (the default). Anything else
    is an exit 2.

.PARAMETER ClangCl
    The clang-cl executable. Default: the Visual Studio 2022 Community LLVM
    component, C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/
    Llvm/x64/bin/clang-cl.exe, then `clang-cl` on PATH.

.PARAMETER NdkRoot
    The Android NDK root (the directory that holds `toolchains/`). Default: the
    first set of $env:NDKROOT, $env:NDK_ROOT, $env:ANDROID_NDK_ROOT. A root that
    does not exist skips the libc++ arm.

.PARAMETER AndroidTarget
    The libc++ arm's `--target`. Default: aarch64-linux-android26.

.PARAMETER LibcxxKnownFailures
    Header -> regular expression, see KNOWN FAILURES. Default @{} (none).

.PARAMETER IncludeOverlay
    An include directory searched before -SimRoot. Must exist.

.PARAMETER MaxErrorLines
    Diagnostic lines quoted per failing header. Default 4.

.OUTPUTS
    Exit 0 = every arm that ran passed, or every arm skipped.
    Exit 1 = one or more headers fail an arm (a stale known failure included).
    Exit 2 = usage or IO, one message line on stderr: a missing -RepoRoot,
             -SimRoot, -SimNetRoot, -NetRoot, -SessionRoot (or one without
             -SimNetRoot), header, known-failure header or
             -IncludeOverlay, an
             unknown arm, or an empty header list.
#>
[CmdletBinding()]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..' '..')).Path,
    [string]$SimRoot = 'Plugins/OGSimulation/Source/OGSimulation/og-simulation',
    [string]$SimNetRoot = 'Plugins/OGSimulation/Source/OGSimulationNet/og-sim-net',
    [string]$NetRoot = 'Plugins/OGSimulation/Source/OGNet/og-net',
    [string]$SessionRoot = 'Plugins/OGSimulation/Source/OGSimulationSession/og-sim-session',
    [string[]]$Headers = @(),
    [string[]]$Arms = @('clang-cl', 'libc++'),
    [string]$ClangCl = '',
    [string]$NdkRoot = '',
    [string]$AndroidTarget = 'aarch64-linux-android26',
    [hashtable]$LibcxxKnownFailures = @{},
    [string]$IncludeOverlay = '',
    [int]$MaxErrorLines = 4
)

$ErrorActionPreference = 'Stop'
$clock = [Diagnostics.Stopwatch]::StartNew()
$DefaultClangCl = 'C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-cl.exe'
$KnownArms = @('clang-cl', 'libc++')

function Exit-Usage {
    param([string]$Message)
    [Console]::Error.WriteLine("[clang-probe] exit 2: $Message")
    exit 2
}

if ($Arms.Count -eq 0) { Exit-Usage 'no arms to run' }
foreach ($a in $Arms) {
    if ($KnownArms -cnotcontains $a) { Exit-Usage "unknown arm: $a (known: $($KnownArms -join ', '))" }
}

if (-not (Test-Path -LiteralPath $RepoRoot -PathType Container)) { Exit-Usage "RepoRoot not found: $RepoRoot" }
$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
$simDir = if ([IO.Path]::IsPathRooted($SimRoot)) { $SimRoot } else { Join-Path $RepoRoot $SimRoot }
if (-not (Test-Path -LiteralPath (Join-Path $simDir 'OGSimulation') -PathType Container)) { Exit-Usage "SimRoot has no OGSimulation/ directory: $simDir" }
$simDir = (Resolve-Path -LiteralPath $simDir).Path

$simNetDir = ''
$netDir = ''
if ($SimNetRoot -ne '') {
    $simNetDir = if ([IO.Path]::IsPathRooted($SimNetRoot)) { $SimNetRoot } else { Join-Path $RepoRoot $SimNetRoot }
    if (-not (Test-Path -LiteralPath (Join-Path $simNetDir 'OGSimNet') -PathType Container)) { Exit-Usage "SimNetRoot has no OGSimNet/ directory: $simNetDir" }
    $simNetDir = (Resolve-Path -LiteralPath $simNetDir).Path
    $netDir = if ([IO.Path]::IsPathRooted($NetRoot)) { $NetRoot } else { Join-Path $RepoRoot $NetRoot }
    if (-not (Test-Path -LiteralPath (Join-Path $netDir 'OGNet') -PathType Container)) { Exit-Usage "NetRoot has no OGNet/ directory: $netDir" }
    $netDir = (Resolve-Path -LiteralPath $netDir).Path
}

$sessionDir = ''
if ($SessionRoot -ne '' -and $simNetDir -eq '' -and $PSBoundParameters.ContainsKey('SessionRoot')) {
    Exit-Usage "SessionRoot needs SimNetRoot: og-sim-session depends on og-sim-net"
}
if ($SessionRoot -ne '' -and $simNetDir -ne '') {
    $sessionDir = if ([IO.Path]::IsPathRooted($SessionRoot)) { $SessionRoot } else { Join-Path $RepoRoot $SessionRoot }
    if (-not (Test-Path -LiteralPath (Join-Path $sessionDir 'OGSimSession') -PathType Container)) { Exit-Usage "SessionRoot has no OGSimSession/ directory: $sessionDir" }
    $sessionDir = (Resolve-Path -LiteralPath $sessionDir).Path
}

function Test-SimNetHeader {
    param([string]$Header)
    return $Header.StartsWith('OGSimNet/')
}

function Test-SessionHeader {
    param([string]$Header)
    return $Header.StartsWith('OGSimSession/')
}

function Resolve-HeaderPath {
    param([string]$Header)
    if (Test-SimNetHeader $Header) {
        if ($simNetDir -eq '') { return '' }
        return (Join-Path $simNetDir $Header)
    }
    if (Test-SessionHeader $Header) {
        if ($sessionDir -eq '') { return '' }
        return (Join-Path $sessionDir $Header)
    }
    return (Join-Path $simDir $Header)
}

$overlayDir = ''
if ($IncludeOverlay -ne '') {
    if (-not (Test-Path -LiteralPath $IncludeOverlay -PathType Container)) { Exit-Usage "IncludeOverlay not found: $IncludeOverlay" }
    $overlayDir = (Resolve-Path -LiteralPath $IncludeOverlay).Path
}

if ($Headers.Count -eq 0) {
    $Headers = @(Get-ChildItem -LiteralPath (Join-Path $simDir 'OGSimulation') -Recurse -File -Filter '*.h' |
        ForEach-Object { [IO.Path]::GetRelativePath($simDir, $_.FullName).Replace('\', '/') } | Sort-Object)
    if ($simNetDir -ne '') {
        $Headers += @(Get-ChildItem -LiteralPath (Join-Path $simNetDir 'OGSimNet') -Recurse -File -Filter '*.h' |
            ForEach-Object { [IO.Path]::GetRelativePath($simNetDir, $_.FullName).Replace('\', '/') } | Sort-Object)
    }
    if ($sessionDir -ne '') {
        $Headers += @(Get-ChildItem -LiteralPath (Join-Path $sessionDir 'OGSimSession') -Recurse -File -Filter '*.h' |
            ForEach-Object { [IO.Path]::GetRelativePath($sessionDir, $_.FullName).Replace('\', '/') } | Sort-Object)
    }
}
if ($Headers.Count -eq 0) { Exit-Usage "no headers to probe under $simDir" }
foreach ($h in $Headers) {
    $path = Resolve-HeaderPath $h
    if ($path -eq '' -or -not (Test-Path -LiteralPath $path -PathType Leaf)) { Exit-Usage "header not found under SimRoot, SimNetRoot or SessionRoot: $h" }
}
foreach ($k in $LibcxxKnownFailures.Keys) {
    $path = Resolve-HeaderPath $k
    if ($path -eq '' -or -not (Test-Path -LiteralPath $path -PathType Leaf)) { Exit-Usage "LibcxxKnownFailures header not found under SimRoot, SimNetRoot or SessionRoot: $k" }
}

$compilers = [ordered]@{}

if ($Arms -ccontains 'clang-cl') {
    $clang = $null
    if ($ClangCl -ne '') {
        if (Test-Path -LiteralPath $ClangCl -PathType Leaf) { $clang = $ClangCl }
    } else {
        if (Test-Path -LiteralPath $DefaultClangCl -PathType Leaf) { $clang = $DefaultClangCl }
        else {
            $onPath = Get-Command 'clang-cl' -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($null -ne $onPath) { $clang = $onPath.Source }
        }
    }
    if ($null -eq $clang) {
        $looked = if ($ClangCl -ne '') { $ClangCl } else { "$DefaultClangCl, PATH" }
        Write-Host "[clang-probe] skipped: no clang ($looked)"
    } else {
        $flags = @('/nologo', '/std:c++20', '/DOG_STANDALONE_BUILD', '/Zs', '/clang:-fno-delayed-template-parsing')
        $compilers['clang-cl'] = @{ Exe = $clang; Flags = $flags }
    }
}

if ($Arms -ccontains 'libc++') {
    $ndk = $NdkRoot
    $looked = if ($NdkRoot -ne '') { "-NdkRoot $NdkRoot" } else { 'NDKROOT, NDK_ROOT, ANDROID_NDK_ROOT unset' }
    if ($ndk -eq '') {
        foreach ($name in 'NDKROOT', 'NDK_ROOT', 'ANDROID_NDK_ROOT') {
            $value = [Environment]::GetEnvironmentVariable($name)
            if (-not [string]::IsNullOrWhiteSpace($value)) { $ndk = $value; $looked = "$name=$value"; break }
        }
    }
    $ndkClang = $null
    if ($ndk -ne '') {
        foreach ($rel in 'toolchains/llvm/prebuilt/windows-x86_64/bin/clang++.exe',
                         'toolchains/llvm/prebuilt/linux-x86_64/bin/clang++',
                         'toolchains/llvm/prebuilt/darwin-x86_64/bin/clang++') {
            $candidate = Join-Path $ndk $rel
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { $ndkClang = $candidate; break }
        }
    }
    if ($null -eq $ndkClang) {
        Write-Host "[clang-probe] libc++ skipped: no NDK ($looked)"
    } else {
        $flags = @("--target=$AndroidTarget", '-std=c++20', '-stdlib=libc++', '-fsyntax-only', '-DOG_STANDALONE_BUILD')
        $compilers['libc++'] = @{ Exe = $ndkClang; Flags = $flags }
    }
}

if ($compilers.Count -eq 0) { exit 0 }

$includes = @()
if ($overlayDir -ne '') { $includes += "-I$overlayDir" }
$includes += @("-I$simDir", ("-I" + (Join-Path $simDir 'glm')))
$simNetIncludes = if ($simNetDir -ne '') { @("-I$simNetDir", "-I$netDir") } else { @() }
$simNetCount = @($Headers | Where-Object { Test-SimNetHeader $_ }).Count
$sessionIncludes = if ($sessionDir -ne '') { @($simNetIncludes + "-I$sessionDir") } else { @() }
$sessionCount = @($Headers | Where-Object { Test-SessionHeader $_ }).Count
$versions = @{}
foreach ($arm in $compilers.Keys) {
    $versions[$arm] = (& $compilers[$arm].Exe --version 2>&1 | Select-Object -First 1).ToString().Trim()
}
if ($compilers.Contains('libc++')) {
    $short = if ($versions['libc++'] -match 'clang version \S+') { $Matches[0] } else { $versions['libc++'] }
    $properties = Join-Path $ndk 'source.properties'
    $revision = if ((Test-Path -LiteralPath $properties -PathType Leaf) -and
        ((Get-Content -LiteralPath $properties -Raw) -match 'Pkg\.Revision\s*=\s*(\S+)')) { $Matches[1] } else { 'unknown' }
    $versions['libc++'] = "$short ndk=$revision"
}

$scratch = Join-Path ([IO.Path]::GetTempPath()) ("clang_header_probe_" + [Guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $scratch)

try {
    $tus = for ($i = 0; $i -lt $Headers.Count; $i++) {
        $tu = Join-Path $scratch ("tu{0:d4}.cpp" -f $i)
        [IO.File]::WriteAllText($tu, "#include `"$($Headers[$i])`"`n")
        $extra = if (Test-SimNetHeader $Headers[$i]) { $simNetIncludes } elseif (Test-SessionHeader $Headers[$i]) { $sessionIncludes } else { @() }
        [pscustomobject]@{ Header = $Headers[$i]; Tu = $tu; Includes = @($includes + $extra) }
    }
    $jobs = foreach ($arm in $compilers.Keys) {
        foreach ($t in $tus) {
            [pscustomobject]@{ Arm = $arm; Header = $t.Header; Tu = $t.Tu
                Exe = $compilers[$arm].Exe; Args = @($compilers[$arm].Flags + $t.Includes + $t.Tu) }
        }
    }

    $results = @($jobs | ForEach-Object -ThrottleLimit ([Environment]::ProcessorCount) -Parallel {
        $a = $_.Args
        $output = & $_.Exe @a 2>&1 | ForEach-Object { $_.ToString() }
        [pscustomobject]@{ Arm = $_.Arm; Header = $_.Header; Rc = $LASTEXITCODE; Output = @($output) }
    })

    $elapsed = $clock.Elapsed.TotalSeconds.ToString('0.00', [Globalization.CultureInfo]::InvariantCulture)
    $anyFailed = $false
    foreach ($arm in $compilers.Keys) {
        $prefix = if ($arm -eq 'clang-cl') { '[clang-probe]' } else { "[clang-probe] $arm" }
        $armResults = @($results | Where-Object { $_.Arm -eq $arm } | Sort-Object Header)
        $failed = 0; $known = 0
        foreach ($r in $armResults) {
            $quote = @($r.Output | Where-Object { $_ -match '\b(error|note):' } | Select-Object -First $MaxErrorLines)
            if ($arm -eq 'libc++' -and $LibcxxKnownFailures.ContainsKey($r.Header)) {
                $pattern = $LibcxxKnownFailures[$r.Header]
                if ($r.Rc -eq 0) {
                    Write-Host "$prefix FAIL $($r.Header) rc=0: listed in -LibcxxKnownFailures but it compiles; remove the entry"
                    $failed++
                    continue
                }
                $match = @($r.Output | Where-Object { $_ -match $pattern } | Select-Object -First 1)
                if ($match.Count -eq 1) {
                    Write-Host "$prefix known $($r.Header): $($match[0].Trim())"
                    $known++
                    continue
                }
                Write-Host "$prefix FAIL $($r.Header) rc=$($r.Rc): a known failure with no line matching '$pattern'"
                $quote | ForEach-Object { Write-Host "    $_" }
                $failed++
                continue
            }
            if ($r.Rc -ne 0) {
                Write-Host "$prefix FAIL $($r.Header) rc=$($r.Rc)"
                $quote | ForEach-Object { Write-Host "    $_" }
                $failed++
            }
        }
        if ($arm -eq 'clang-cl') {
            Write-Host ('{0} headers={1} simnet={2} session={3} failed={4} clang={5} elapsed={6} s' -f $prefix, $armResults.Count, $simNetCount, $sessionCount, $failed, $versions[$arm], $elapsed)
        } else {
            Write-Host ('{0} headers={1} simnet={2} session={3} failed={4} known={5} clang={6} target={7} elapsed={8} s' -f `
                $prefix, $armResults.Count, $simNetCount, $sessionCount, $failed, $known, $versions[$arm], $AndroidTarget, $elapsed)
        }
        if ($armResults.Count -ne $Headers.Count) {
            [Console]::Error.WriteLine("[clang-probe] exit 2: $arm has $($armResults.Count) results for $($Headers.Count) headers")
            exit 2
        }
        if ($failed -gt 0) { $anyFailed = $true }
    }
    if ($anyFailed) { exit 1 }
    exit 0
} finally {
    Remove-Item -LiteralPath $scratch -Recurse -Force -ErrorAction SilentlyContinue
}
