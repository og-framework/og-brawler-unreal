#Requires -Version 7.0
# SPDX-License-Identifier: MPL-2.0
<#
.SYNOPSIS
    E4 engine-lane fence - every engine replication marker in source is joined to
    one row of a reviewed manifest, in BOTH directions, and every row is classified.

.DESCRIPTION
    The end-state's E4 says sim state never rides the engine's replication lane.
    This lint is that rule's source-side gate (design_p7_session_state_split.md
    4.2). It scans `*.h` and `*.cpp` under -ScanRoots for engine replication
    markers, turns each into a key `Kind|Class|Member`, and joins the keys against
    the table of -ManifestPath.

    ------------------------------------------------------------------------
    THE SCAN
    ------------------------------------------------------------------------
    Skipped: any directory segment `Binaries`, `Intermediate`, `ThirdParty`, `glm`
    or `.git`, and any directory whose name starts with `build` (any case). The
    segments are taken RELATIVE TO -RepoRoot, so a repo that itself sits under a
    `build` folder is still scanned.

    Comments (`//`, `/* */` across lines) and string and char literals are removed
    first, keeping every newline, so line numbers stay true. It is modelled on
    configurability_lint.ps1's strip but is ONE left-to-right pass over the whole
    file: that lint's per-line strip treats a `/*` inside a `//` comment or inside
    a string as a block opener and drops the code after it until the next `*/`
    (measured on it: `// see Net/*.h` then `DOREPLIFETIME(AFix, Color);` strips
    the second line to nothing). The real tree has such a line
    (CorrectionStateBufferCodec.h:207). A `'` right after a letter or digit is a
    digit separator, not a char literal.

    ------------------------------------------------------------------------
    MARKERS -> KEYS  Kind|Class|Member
    ------------------------------------------------------------------------
      Property        `DOREPLIFETIME\w*\s*\(\s*(\w+)\s*,\s*(\w+)` (the cpp half)
                      and a `UPROPERTY(` whose TOP-LEVEL specifier list holds a bare
                      `Replicated` or a `ReplicatedUsing = ...` (the header half).
                      The header half's member is the identifier before the first
                      `=`, `;` or `{` after the `)`, its type the tokens before it.
      Rpc             a `UFUNCTION(` whose top-level specifier list holds a bare
                      `Server`, `Client` or `NetMulticast`. The member is the first
                      `(\w+)\s*\(` after the `)`, the type the parameter types,
                      comma-separated, names and default values dropped (`-` if none).
      ReplicatedClass `bReplicates\s*=\s*true`, `SetReplicates\s*\(\s*true`,
                      `SetIsReplicated(ByDefault)?\s*\(\s*true`. Member `*`, type `-`.
      SubObject       `AddReplicatedSubObject\s*\(`, `ReplicateSubobject\s*\(`,
                      `bReplicateUsingRegisteredSubObjectList\s*=\s*true`,
                      `RegisterReplicationFragments\s*\(`. Member `*`, type `-`.
      IrisSerializer  `UE_NET_(IMPLEMENT|DECLARE)\w*\s*\(\s*(\w+)`. Class `-`, member
                      the captured name, type `-`.

    Specifiers are matched as whole top-level TOKENS, so `Category = Client`,
    `NotReplicated` and anything inside `meta = (...)` are not markers.

    THE CLASS. A UPROPERTY or UFUNCTION takes the type declared on the first
    `class` / `struct` line after the nearest preceding `UCLASS(`; a forward
    declaration (`class X;`) never counts. Every other marker takes that rule in a
    header, and in a `.cpp` the qualifier of the nearest preceding out-of-line
    definition: a line with NO leading whitespace matching
    `(\w+)::~?\w+\s*\([^;]*$`. `Super::Foo(` calls are indented and never match.
    A class that cannot be found is `?`.

    ------------------------------------------------------------------------
    THE MANIFEST
    ------------------------------------------------------------------------
    Exactly one line `sim-rows: open` or `sim-rows: closed`, then ONE table whose
    header is exactly
        | Kind | Class | Member | Type | Lane class | Retired by | Readers |
    then a separator line, then one row per key. A cell wrapped in backticks is read
    without them. A `Retired by` of `-` counts as empty.

    A row of Kind `EngineClass` names a class that replicates ONLY through an
    engine base (no source marker of its own and no other row): member `*`, type
    `-`, lane class Session. It has no source marker, so J2 skips it; R1 is what
    keeps it true.

    ------------------------------------------------------------------------
    THE ARMS - each prints `<relative file>:<line>: [E4-<arm>] <message>`
    ------------------------------------------------------------------------
      J1  a marker key with no manifest row                      (unlisted)
      J2  a manifest row with no marker (EngineClass rows skipped) (stale)
      J3  a header Property half with no DOREPLIFETIME of the same class and
          member, or the reverse                                 (half-registered)
      C1  a lane class not in Sim, Presentation, Session, Binding; or an
          EngineClass row whose lane class is not Session, member not `*` or
          type not `-`
      C2  a Sim row whose key is not in the frozen $Ph0SimKeys
      C3  `sim-rows: closed` while any Sim row exists
      C4  a Binding row whose key is not
          Property|AOGBrawlerUECharacter|SimCharacterIdValue, or more than one
          Binding row
      C5  two rows with the same key, a Sim row with an empty Retired by, or an
          EngineClass row for a class another row already names
      T1  a non-Sim Property or Rpc row whose type, or an inner type of TArray,
          TSet, TMap or TOptional, is a SIM WIRE TYPE: a USTRUCT, UCLASS or UENUM
          declared in a scanned file that #includes a path starting with one of
          -SimIncludePrefixes. Object references are exempt: `T*`, TObjectPtr,
          TWeakObjectPtr, TSoftObjectPtr, TSubclassOf, TScriptInterface. The
          source's type is checked; the Type cell only when the source has none.
      T2  a row's Type cell differs from the source's. Both sides are compared
          with whitespace collapsed and no space around `< > , & * ::`. A
          Property known only by its DOREPLIFETIME has no source type (J3 fires).

    A header half keeps its key, so deleting only a DOREPLIFETIME fires J3 and
    not J2: the row still has a marker (design 4.2 lists the header half as a
    marker). J2 fires once both halves are gone.

    $Ph0SimKeys is design 2.1 rows 3-4 and 6-17 verbatim; this list may only shrink, never grow.

    ------------------------------------------------------------------------
    THE REFLECTION ARM - only with -ReflectionLog (design 4.5)
    ------------------------------------------------------------------------
    The Editor automation test OGBrawler.Replication.EngineLaneMembers prints one
    `[EngineLane.Reflected] <Kind> <Class> <Member> <CPPType>` line per CPF_Net
    property, FUNC_Net function and replicating default object of the game's
    script packages and /Game Blueprints. It sees what source grep cannot:
    Blueprint replication and replication inherited from an engine base. The
    lines may carry any log prefix; repeats are one key.
      R1  a reflected Property or Rpc key with no manifest row; a manifest
          Property or Rpc row that is not reflected; a reflected ReplicatedClass
          that no manifest row names (add an EngineClass row if it replicates
          only through an engine base); an EngineClass row whose class is not
          reflected as a ReplicatedClass (stale). IrisSerializer and SubObject
          rows are outside R1; a ReplicatedClass row only names its class.

    WHAT IT CANNOT SEE: a primitive carrying sim meaning under a non-Sim class (the
    manifest edit and its Readers cell are the reviewed evidence), `#if 0`
    blocks, raw string literals, and markers spelled through a user macro.
    Without -ReflectionLog it cannot see Blueprint replication or replication
    inherited from an engine base.

    ------------------------------------------------------------------------
    OUTPUT
    ------------------------------------------------------------------------
    Every violation line, then always
        [E4] rows=N Sim=a Presentation=b Session=c Binding=d frozen=14 sim-rows=<open|closed>
    then `[E4] files=F markers=M keys=K violations=V elapsed=S s`. With
    -ReflectionLog, between them,
        [E4] reflection=<log> lines=L keys=K classes=C

.PARAMETER RepoRoot
    Root the scan roots, the default manifest and every printed path are relative
    to. Default: two levels above this script.

.PARAMETER ScanRoots
    Directories under -RepoRoot to scan. Default: Source, Plugins. A root that does
    not exist is an exit 2, never a silent skip (a `pwsh -File` call passes
    `'Source,Plugins'` as ONE root, which would otherwise scan nothing and pass).

.PARAMETER ManifestPath
    The manifest. Default: <RepoRoot>/tools/lint/engine_lane_manifest.md. A
    relative path is taken under -RepoRoot, like the scan roots.

.PARAMETER SimIncludePrefixes
    Include-path prefixes that mark a file as sim-side for T1. Default:
    OGSimulation/, OGBrawler/, OGSimulationJolt/, OGNet/ (case-insensitive).

.PARAMETER ReflectionLog
    Optional. A log holding the `[EngineLane.Reflected]` lines of the Editor
    automation test OGBrawler.Replication.EngineLaneMembers; enables R1. A
    relative path is taken under -RepoRoot. Without it R1 does not run.

.OUTPUTS
    Exit 0 = clean.
    Exit 1 = one or more violations.
    Exit 2 = usage or IO, one message line on stderr and nothing else: a missing
             -RepoRoot, scan root or manifest; a missing, duplicated or invalid
             `sim-rows:` line; no table, a table header that is not exactly the
             one above, a missing separator, a row without 7 cells, or a second
             table; a -ReflectionLog that does not exist, holds no
             `[EngineLane.Reflected]` line, or holds one that does not parse.
#>
[CmdletBinding()]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..' '..')).Path,
    [string[]]$ScanRoots = @('Source', 'Plugins'),
    [string]$ManifestPath = (Join-Path $RepoRoot 'tools/lint/engine_lane_manifest.md'),
    [string[]]$SimIncludePrefixes = @('OGSimulation/', 'OGBrawler/', 'OGSimulationJolt/', 'OGNet/'),
    [string]$ReflectionLog = ''
)

$ErrorActionPreference = 'Stop'
$clock = [Diagnostics.Stopwatch]::StartNew()

$Ph0SimKeys = @(
    'Property|AOGBrawlerUECharacter|SimCharacterIdValue'
    'Property|AOGBrawlerUECharacter|PhysicsSlotValue'
    'ReplicatedClass|USimulatableNetSyncComponent|*'
    'Property|USimulatableNetSyncComponent|m_simulationStateCorrectionSyncedBuffer'
    'Rpc|USimulatableNetSyncComponent|ServerReceiveRemoteMove'
    'ReplicatedClass|ASimulationInputRelay|*'
    'Property|ASimulationInputRelay|m_relayedInputRing'
    'ReplicatedClass|ASimulationTimingRelay|*'
    'Property|ASimulationTimingRelay|m_buffer'
    'Property|ASimulationTimingRelay|m_relayDelayFloorTicks'
    'ReplicatedClass|ASimulationConnectionRelay|*'
    'Property|ASimulationConnectionRelay|m_connectionTier'
    'IrisSerializer|-|SimulationStateSyncBuffer'
    'IrisSerializer|-|RelayedInputRing'
)
$LaneClasses = @('Sim', 'Presentation', 'Session', 'Binding')
$BindingKey = 'Property|AOGBrawlerUECharacter|SimCharacterIdValue'
$EngineClassKind = 'EngineClass'
$ReflectedTagRx = [regex]::new('\[EngineLane\.Reflected\]')
$ReflectedLineRx = [regex]::new('\[EngineLane\.Reflected\] (Property|Rpc|ReplicatedClass) (\S+) (\S+)(?: (.*?))?\s*$')
$TableHeader = '| Kind | Class | Member | Type | Lane class | Retired by | Readers |'
$ExcludedSegments = @('Binaries', 'Intermediate', 'ThirdParty', 'glm', '.git')
$ObjectRefTemplates = @('TObjectPtr', 'TWeakObjectPtr', 'TSoftObjectPtr', 'TSubclassOf', 'TScriptInterface')
$ContainerTemplates = @('TArray', 'TSet', 'TMap', 'TOptional')
$CvQualifiers = @('const', 'volatile', 'unsigned', 'signed', 'struct', 'class', 'enum')

$RxOpts = [Text.RegularExpressions.RegexOptions]::Multiline
$StripRx = [regex]::new('//[^\n]*|/\*[\s\S]*?(?:\*/|\z)|"(?:\\.|[^"\\\n])*"?|(?<![A-Za-z0-9_])''(?:\\.|[^''\\\n])*''?')
$TriggerRx = [regex]::new('UPROPERTY|UFUNCTION|DOREPLIFETIME|bReplicat|SetReplicates|SetIsReplicated|ReplicatedSubObject|ReplicateSubobject|RegisterReplicationFragments|UE_NET_|USTRUCT|UCLASS|UENUM')
$DoRepRx = [regex]::new('\bDOREPLIFETIME\w*\s*\(\s*(\w+)\s*,\s*(\w+)')
$UPropertyRx = [regex]::new('\bUPROPERTY\s*\(')
$UFunctionRx = [regex]::new('\bUFUNCTION\s*\(')
$ReplicatedClassRx = [regex]::new('\bbReplicates\s*=\s*true\b|\bSetReplicates\s*\(\s*true\b|\bSetIsReplicated(?:ByDefault)?\s*\(\s*true\b')
$SubObjectRx = [regex]::new('\bAddReplicatedSubObject\s*\(|\bReplicateSubobject\s*\(|\bbReplicateUsingRegisteredSubObjectList\s*=\s*true\b|\bRegisterReplicationFragments\s*\(')
$IrisRx = [regex]::new('\bUE_NET_(IMPLEMENT|DECLARE)\w*\s*\(\s*(\w+)')
$UClassRx = [regex]::new('\bUCLASS\s*\(')
$UTypeRx = [regex]::new('\b(?:USTRUCT|UCLASS|UENUM)\s*\(')
$ClassLineRx = [regex]::new('^[ \t]*(?:class|struct)[ \t]+(?:\w+_API[ \t]+)?(\w+)(.*)$', $RxOpts)
$TypeLineRx = [regex]::new('^[ \t]*(?:class|struct|enum(?:[ \t]+class|[ \t]+struct)?)[ \t]+(?:\w+_API[ \t]+)?(\w+)(.*)$', $RxOpts)
$CppDefRx = [regex]::new('^(?=\S).*?(\w+)::~?\w+\s*\([^;\n]*$', $RxOpts)
$IncludeRx = [regex]::new('^[ \t]*#[ \t]*include[ \t]*[<"]([^>"\r\n]+)[>"]', $RxOpts)
$IncludeCodeRx = [regex]::new('^[ \t]*#[ \t]*include\b', $RxOpts)
$FuncNameRx = [regex]::new('(\w+)\s*\(')

function Exit-Usage {
    param([string]$Message)
    [Console]::Error.WriteLine("[E4] exit 2: $Message")
    exit 2
}

function Get-RelPath {
    param([string]$Path)
    return [IO.Path]::GetRelativePath($RepoRoot, $Path).Replace('\', '/')
}

function Get-CellText {
    param([string]$Cell)
    $c = $Cell.Trim()
    if ($c.Length -ge 2 -and $c.StartsWith('`') -and $c.EndsWith('`')) { $c = $c.Substring(1, $c.Length - 2).Trim() }
    return $c
}

function Get-NormalType {
    param([string]$Type)
    if ($null -eq $Type) { return $null }
    $t = [regex]::Replace($Type.Trim(), '\s+', ' ')
    $t = [regex]::Replace($t, '\s*(<|>|,|&|\*|::)\s*', '$1')
    return $t.Replace(',', ', ')
}

function Read-Manifest {
    if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) { Exit-Usage "manifest not found: $ManifestPath" }
    $lines = [IO.File]::ReadAllLines($ManifestPath)
    $rel = Get-RelPath $ManifestPath
    $flagAt = @()
    for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -match '^\s*sim-rows:') { $flagAt += $i } }
    if ($flagAt.Count -eq 0) { Exit-Usage "${rel}: no 'sim-rows:' line" }
    if ($flagAt.Count -gt 1) { Exit-Usage ("${rel}: 'sim-rows:' appears {0} times (lines {1})" -f $flagAt.Count, (($flagAt | ForEach-Object { $_ + 1 }) -join ', ')) }
    $fm = [regex]::Match($lines[$flagAt[0]], '^\s*sim-rows:\s*(open|closed)\s*$')
    if (-not $fm.Success) { Exit-Usage ("${rel}:{0}: 'sim-rows:' must be 'open' or 'closed'" -f ($flagAt[0] + 1)) }

    $h = -1
    for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -match '^\s*\|') { $h = $i; break } }
    if ($h -lt 0) { Exit-Usage "${rel}: no table" }
    if ($lines[$h].Trim() -cne $TableHeader) { Exit-Usage ("${rel}:{0}: the table header is not exactly '{1}'" -f ($h + 1), $TableHeader) }
    if ($h + 1 -ge $lines.Count -or $lines[$h + 1] -notmatch '^\s*\|(\s*:?-+:?\s*\|){7}\s*$') {
        Exit-Usage ("${rel}:{0}: the table header has no 7-column separator line under it" -f ($h + 2))
    }
    $rows = [Collections.Generic.List[object]]::new()
    $i = $h + 2
    while ($i -lt $lines.Count -and $lines[$i] -match '^\s*\|') {
        $t = $lines[$i].Trim()
        if (-not $t.EndsWith('|')) { Exit-Usage ("${rel}:{0}: a row must end with '|'" -f ($i + 1)) }
        $cells = $t.Substring(1, $t.Length - 2).Split('|')
        if ($cells.Count -ne 7) { Exit-Usage ("${rel}:{0}: a row has {1} cells, not 7" -f ($i + 1), $cells.Count) }
        $c = @($cells | ForEach-Object { Get-CellText $_ })
        $rows.Add([pscustomobject]@{
            Kind = $c[0]; Class = $c[1]; Member = $c[2]; Type = $c[3]; Lane = $c[4]; RetiredBy = $c[5]; Readers = $c[6]
            Key = ('{0}|{1}|{2}' -f $c[0], $c[1], $c[2]); Line = $i + 1
        })
        $i++
    }
    for (; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '^\s*\|') { Exit-Usage ("${rel}:{0}: a second table; the manifest holds one" -f ($i + 1)) }
    }
    return [pscustomobject]@{ Rel = $rel; Flag = $fm.Groups[1].Value; FlagLine = $flagAt[0] + 1; Rows = $rows }
}

function Read-ReflectionLog {
    $path = [IO.Path]::GetFullPath([IO.Path]::Combine($RepoRoot, $ReflectionLog))
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { Exit-Usage "reflection log not found: $path" }
    $rel = Get-RelPath $path
    $lines = [IO.File]::ReadAllLines($path)
    $members = [ordered]@{}
    $classes = [ordered]@{}
    $seen = 0
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if (-not $ReflectedTagRx.IsMatch($lines[$i])) { continue }
        $m = $ReflectedLineRx.Match($lines[$i])
        if (-not $m.Success) { Exit-Usage ("${rel}:{0}: an [EngineLane.Reflected] line that does not parse as '<Kind> <Class> <Member> <CPPType>'" -f ($i + 1)) }
        $seen++
        $kind = $m.Groups[1].Value
        if ($kind -ceq 'ReplicatedClass') {
            if (-not $classes.Contains($m.Groups[2].Value)) { $classes[$m.Groups[2].Value] = $i + 1 }
            continue
        }
        $key = '{0}|{1}|{2}' -f $kind, $m.Groups[2].Value, $m.Groups[3].Value
        if (-not $members.Contains($key)) { $members[$key] = $i + 1 }
    }
    if ($seen -eq 0) { Exit-Usage "${rel}: no [EngineLane.Reflected] line" }
    return [pscustomobject]@{ Rel = $rel; Lines = $seen; Members = $members; Classes = $classes }
}

function Get-ScanFiles {
    $out = [Collections.Generic.List[string]]::new()
    foreach ($r in $ScanRoots) {
        $full = [IO.Path]::GetFullPath([IO.Path]::Combine($RepoRoot, $r))
        if (-not [IO.Directory]::Exists($full)) { Exit-Usage "scan root not found: $r (under $RepoRoot)" }
        $stack = [Collections.Generic.Stack[string]]::new()
        $stack.Push($full)
        while ($stack.Count -gt 0) {
            $d = $stack.Pop()
            foreach ($f in [IO.Directory]::EnumerateFiles($d)) {
                $ext = [IO.Path]::GetExtension($f)
                if ($ext -eq '.h' -or $ext -eq '.cpp') { $out.Add($f) }
            }
            foreach ($s in [IO.Directory]::EnumerateDirectories($d)) {
                $relSegs = (Get-RelPath $s).Split('/')
                $skip = $false
                foreach ($seg in $relSegs) {
                    if ($ExcludedSegments -contains $seg -or $seg.StartsWith('build', [StringComparison]::OrdinalIgnoreCase)) { $skip = $true; break }
                }
                if (-not $skip) { $stack.Push($s) }
            }
        }
    }
    $out.Sort([StringComparer]::Ordinal)
    return , $out
}

function Remove-CommentsAndStrings {
    param([string]$Text)
    return $StripRx.Replace($Text, {
        param($m)
        $v = $m.Value
        if ($v.StartsWith('/')) { return [regex]::Replace($v, '[^\n]', '') + ' ' }
        $q = $v[0]
        return "$q$q"
    })
}

function Get-CloseParen {
    param([string]$Text, [int]$Open)
    $depth = 0
    for ($i = $Open; $i -lt $Text.Length; $i++) {
        $ch = $Text[$i]
        if ($ch -eq '(') { $depth++ }
        elseif ($ch -eq ')') { $depth--; if ($depth -eq 0) { return $i } }
    }
    return -1
}

function Split-TopLevel {
    param([string]$Text)
    $parts = [Collections.Generic.List[string]]::new()
    $depth = 0; $start = 0
    for ($i = 0; $i -lt $Text.Length; $i++) {
        $ch = $Text[$i]
        if ($ch -eq '(' -or $ch -eq '<' -or $ch -eq '[' -or $ch -eq '{') { $depth++ }
        elseif ($ch -eq ')' -or $ch -eq '>' -or $ch -eq ']' -or $ch -eq '}') { $depth-- }
        elseif ($ch -eq ',' -and $depth -eq 0) { $parts.Add($Text.Substring($start, $i - $start)); $start = $i + 1 }
    }
    $parts.Add($Text.Substring($start))
    return $parts.ToArray()
}

function Get-Specifiers {
    param([string]$Code, [int]$Open)
    $close = Get-CloseParen $Code $Open
    if ($close -lt 0) { return $null }
    $tokens = @(Split-TopLevel $Code.Substring($Open + 1, $close - $Open - 1) | ForEach-Object { [regex]::Replace($_.Trim(), '\s+', ' ') })
    return [pscustomobject]@{ Tokens = $tokens; Close = $close }
}

function Get-ParamTypes {
    param([string]$ParamText)
    $types = @()
    foreach ($p in (Split-TopLevel $ParamText)) {
        $q = [regex]::Replace($p.Trim(), '\s+', ' ')
        if ($q -eq '' -or $q -eq 'void') { continue }
        $eq = $q.IndexOf('=')
        if ($eq -ge 0) { $q = $q.Substring(0, $eq).Trim() }
        $q = [regex]::Replace($q, '\s*\[[^\]]*\]$', '')
        $m = [regex]::Match($q, '^(.*?[\s&*>])(\w+)$')
        if ($m.Success) {
            $head = $m.Groups[1].Value.Trim()
            $isQualifierOnly = (@($head.Split(' ') | Where-Object { $CvQualifiers -notcontains $_ }).Count -eq 0)
            if (-not $isQualifierOnly -and -not $head.EndsWith('::')) { $q = $head }
        }
        $types += $q
    }
    if ($types.Count -eq 0) { return '-' }
    return ($types -join ', ')
}

function Get-LineOf {
    param([int[]]$Starts, [int]$Index)
    $k = [Array]::BinarySearch($Starts, $Index)
    if ($k -lt 0) { $k = (-bnot $k) - 1 }
    return $k + 1
}

function Find-DeclaredType {
    param([string]$Code, [int]$From, [regex]$LineRx)
    $m = $LineRx.Match($Code, $From)
    while ($m.Success) {
        if ($m.Groups[2].Value -notmatch '^\s*;') { return $m.Groups[1].Value }
        $m = $m.NextMatch()
    }
    return '?'
}

function Get-HeaderClass {
    param([string]$Code, [int]$Index)
    $u = -1
    foreach ($m in $UClassRx.Matches($Code)) { if ($m.Index -le $Index) { $u = $m.Index } else { break } }
    if ($u -lt 0) { return '?' }
    return (Find-DeclaredType $Code $u $ClassLineRx)
}

function Get-CppClass {
    param([string]$Code, [int]$Index)
    $c = '?'
    foreach ($m in $CppDefRx.Matches($Code)) { if ($m.Index -le $Index) { $c = $m.Groups[1].Value } else { break } }
    return $c
}

function Get-StatementClass {
    param([string]$Code, [int]$Index, [bool]$IsHeader)
    if ($IsHeader) { return (Get-HeaderClass $Code $Index) }
    return (Get-CppClass $Code $Index)
}

function Get-Declarator {
    param([string]$Code, [int]$From)
    $end = $Code.IndexOfAny([char[]]@(';', '=', '{'), $From)
    if ($end -lt 0) { return $null }
    $d = [regex]::Replace($Code.Substring($From, $end - $From).Trim(), '\s+', ' ')
    $d = [regex]::Replace($d, '\s*:\s*\d+$', '')
    $d = [regex]::Replace($d, '\s*\[[^\]]*\]$', '')
    $m = [regex]::Match($d, '^(.*?)\s*\b(\w+)$')
    if (-not $m.Success) { return $null }
    $type = $m.Groups[1].Value.Trim()
    if ($type -eq '') { $type = '?' }
    return [pscustomobject]@{ Member = $m.Groups[2].Value; Type = $type }
}

function Read-SourceFile {
    param([string]$Path)
    $raw = [IO.File]::ReadAllText($Path)
    if (-not $TriggerRx.IsMatch($raw)) { return }
    $raw = $raw.Replace("`r`n", "`n").Replace("`r", "`n")
    $code = Remove-CommentsAndStrings $raw
    $rel = Get-RelPath $Path
    $isHeader = $Path.EndsWith('.h', [StringComparison]::OrdinalIgnoreCase)
    $starts = [Collections.Generic.List[int]]::new()
    $starts.Add(0)
    for ($i = 0; $i -lt $code.Length; $i++) { if ($code[$i] -eq "`n") { $starts.Add($i + 1) } }
    $st = $starts.ToArray()

    $rawLines = $raw.Split("`n")
    $isSimSide = $false
    foreach ($m in $IncludeCodeRx.Matches($code)) {
        $ln = Get-LineOf $st $m.Index
        $im = $IncludeRx.Match($rawLines[$ln - 1])
        if (-not $im.Success) { continue }
        $path = $im.Groups[1].Value.Replace('\', '/')
        foreach ($p in $SimIncludePrefixes) { if ($path.StartsWith($p, [StringComparison]::OrdinalIgnoreCase)) { $isSimSide = $true } }
    }
    if ($isSimSide) {
        foreach ($m in $UTypeRx.Matches($code)) {
            $name = Find-DeclaredType $code $m.Index $TypeLineRx
            if ($name -ne '?' -and -not $script:SimWireTypes.ContainsKey($name)) {
                $script:SimWireTypes[$name] = '{0}:{1}' -f $rel, (Get-LineOf $st $m.Index)
            }
        }
    }

    function Add-Marker {
        param([string]$Kind, [string]$Class, [string]$Member, $Type, [string]$Half, [int]$Index)
        $script:Markers.Add([pscustomobject]@{
            Kind = $Kind; Class = $Class; Member = $Member; Type = $Type; Half = $Half
            Key = ('{0}|{1}|{2}' -f $Kind, $Class, $Member); File = $rel; Line = (Get-LineOf $st $Index)
        })
    }

    foreach ($m in $DoRepRx.Matches($code)) { Add-Marker 'Property' $m.Groups[1].Value $m.Groups[2].Value $null 'Cpp' $m.Index }
    foreach ($m in $UPropertyRx.Matches($code)) {
        $s = Get-Specifiers $code ($m.Index + $m.Length - 1)
        if ($null -eq $s) { continue }
        $hit = @($s.Tokens | Where-Object { $_ -ceq 'Replicated' -or $_ -cmatch '^ReplicatedUsing ?=' }).Count -gt 0
        if (-not $hit) { continue }
        $d = Get-Declarator $code ($s.Close + 1)
        if ($null -eq $d) { Add-Marker 'Property' (Get-HeaderClass $code $m.Index) '?' '?' 'Header' $m.Index; continue }
        Add-Marker 'Property' (Get-HeaderClass $code $m.Index) $d.Member $d.Type 'Header' $m.Index
    }
    foreach ($m in $UFunctionRx.Matches($code)) {
        $s = Get-Specifiers $code ($m.Index + $m.Length - 1)
        if ($null -eq $s) { continue }
        $hit = @($s.Tokens | Where-Object { $_ -ceq 'Server' -or $_ -ceq 'Client' -or $_ -ceq 'NetMulticast' }).Count -gt 0
        if (-not $hit) { continue }
        $fm = $FuncNameRx.Match($code, $s.Close + 1)
        if (-not $fm.Success) { Add-Marker 'Rpc' (Get-HeaderClass $code $m.Index) '?' '?' $null $m.Index; continue }
        $open = $fm.Index + $fm.Length - 1
        $close = Get-CloseParen $code $open
        $ptypes = if ($close -lt 0) { '?' } else { Get-ParamTypes $code.Substring($open + 1, $close - $open - 1) }
        Add-Marker 'Rpc' (Get-HeaderClass $code $m.Index) $fm.Groups[1].Value $ptypes $null $m.Index
    }
    foreach ($m in $ReplicatedClassRx.Matches($code)) { Add-Marker 'ReplicatedClass' (Get-StatementClass $code $m.Index $isHeader) '*' '-' $null $m.Index }
    foreach ($m in $SubObjectRx.Matches($code)) { Add-Marker 'SubObject' (Get-StatementClass $code $m.Index $isHeader) '*' '-' $null $m.Index }
    foreach ($m in $IrisRx.Matches($code)) { Add-Marker 'IrisSerializer' '-' $m.Groups[2].Value '-' $null $m.Index }
}

function Test-SimWireType {
    param([string]$Type)
    $t = [regex]::Replace($Type, '\b(const|volatile|struct|class|enum)\b', '').Trim()
    $t = $t.TrimEnd('&', ' ')
    if ($t.EndsWith('*')) { return $false }
    $m = [regex]::Match($t, '^([\w:]+)\s*(?:<(.*)>)?$')
    if (-not $m.Success) { return $false }
    $name = $m.Groups[1].Value.Split(':')[-1]
    if ($ObjectRefTemplates -contains $name) { return $false }
    if ($ContainerTemplates -contains $name -and $m.Groups[2].Success) {
        foreach ($inner in (Split-TopLevel $m.Groups[2].Value)) { if (Test-SimWireType $inner) { return $true } }
        return $false
    }
    return $script:SimWireTypes.ContainsKey($name)
}

function New-Violation {
    param([string]$File, [int]$Line, [string]$Arm, [string]$Message)
    return [pscustomobject]@{ File = $File; Line = $Line; Arm = $Arm; Message = $Message }
}

function Invoke-ArmJ1 {
    foreach ($k in $script:FirstMarker.Keys) {
        if (-not $script:RowKeys.Contains($k)) {
            $mk = $script:FirstMarker[$k]
            New-Violation $mk.File $mk.Line 'J1' "unlisted: marker $k has no manifest row"
        }
    }
}

function Invoke-ArmJ2 {
    foreach ($r in $script:Manifest.Rows) {
        if ($r.Kind -ceq $EngineClassKind) { continue }
        if (-not $script:FirstMarker.Contains($r.Key)) {
            New-Violation $script:Manifest.Rel $r.Line 'J2' "stale: row $($r.Key) has no marker in source"
        }
    }
}

function Invoke-ArmJ3 {
    foreach ($k in $script:HeaderHalves.Keys) {
        if (-not $script:CppHalves.Contains($k)) {
            $mk = $script:HeaderHalves[$k]
            New-Violation $mk.File $mk.Line 'J3' "half-registered: $k has a replicated UPROPERTY but no DOREPLIFETIME"
        }
    }
    foreach ($k in $script:CppHalves.Keys) {
        if (-not $script:HeaderHalves.Contains($k)) {
            $mk = $script:CppHalves[$k]
            New-Violation $mk.File $mk.Line 'J3' "half-registered: $k has a DOREPLIFETIME but no replicated UPROPERTY"
        }
    }
}

function Invoke-ArmC1 {
    foreach ($r in $script:Manifest.Rows) {
        if ($LaneClasses -cnotcontains $r.Lane) {
            New-Violation $script:Manifest.Rel $r.Line 'C1' ("row $($r.Key): lane class '$($r.Lane)' is not one of " + ($LaneClasses -join ', '))
        }
        elseif ($r.Kind -ceq $EngineClassKind -and ($r.Lane -cne 'Session' -or $r.Member -cne '*' -or $r.Type -cne '-')) {
            New-Violation $script:Manifest.Rel $r.Line 'C1' "row $($r.Key): an EngineClass row is lane class Session, member '*' and type '-'"
        }
    }
}

function Invoke-ArmC2 {
    foreach ($r in $script:Manifest.Rows) {
        if ($r.Lane -ceq 'Sim' -and $Ph0SimKeys -cnotcontains $r.Key) {
            New-Violation $script:Manifest.Rel $r.Line 'C2' "row $($r.Key): a Sim row outside the frozen Ph0 list; no new Sim row may appear"
        }
    }
}

function Invoke-ArmC3 {
    if ($script:Manifest.Flag -ne 'closed') { return }
    foreach ($r in $script:Manifest.Rows) {
        if ($r.Lane -ceq 'Sim') {
            New-Violation $script:Manifest.Rel $r.Line 'C3' "row $($r.Key): a Sim row while 'sim-rows: closed' (line $($script:Manifest.FlagLine))"
        }
    }
}

function Invoke-ArmC4 {
    $binding = @($script:Manifest.Rows | Where-Object { $_.Lane -ceq 'Binding' })
    foreach ($r in $binding) {
        if ($r.Key -cne $BindingKey) {
            New-Violation $script:Manifest.Rel $r.Line 'C4' "row $($r.Key): the only key allowed as Binding is $BindingKey"
        }
    }
    if ($binding.Count -gt 1) {
        New-Violation $script:Manifest.Rel $binding[1].Line 'C4' "$($binding.Count) Binding rows; at most one is allowed"
    }
}

function Invoke-ArmC5 {
    $seen = @{}
    foreach ($r in $script:Manifest.Rows) {
        if ($seen.ContainsKey($r.Key)) {
            New-Violation $script:Manifest.Rel $r.Line 'C5' "row $($r.Key): a duplicate of line $($seen[$r.Key])"
        }
        else { $seen[$r.Key] = $r.Line }
        if ($r.Lane -ceq 'Sim' -and ($r.RetiredBy -eq '' -or $r.RetiredBy -eq '-')) {
            New-Violation $script:Manifest.Rel $r.Line 'C5' "row $($r.Key): a Sim row with an empty 'Retired by'"
        }
        if ($r.Kind -ceq $EngineClassKind) {
            $other = @($script:Manifest.Rows | Where-Object { $_.Kind -cne $EngineClassKind -and $_.Class -ceq $r.Class }) | Select-Object -First 1
            if ($null -ne $other) {
                New-Violation $script:Manifest.Rel $r.Line 'C5' "row $($r.Key): class $($r.Class) is already named by row $($other.Key) (line $($other.Line)); an EngineClass row is only for a class no other row names"
            }
        }
    }
}

function Invoke-ArmR1 {
    if ($null -eq $script:Reflection) { return }
    $rel = $script:Reflection.Rel
    foreach ($k in $script:Reflection.Members.Keys) {
        if (-not $script:RowKeys.Contains($k)) {
            New-Violation $rel $script:Reflection.Members[$k] 'R1' "reflected $k has no manifest row"
        }
    }
    $named = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($r in $script:Manifest.Rows) {
        [void]$named.Add($r.Class)
        if (($r.Kind -ceq 'Property' -or $r.Kind -ceq 'Rpc') -and -not $script:Reflection.Members.Contains($r.Key)) {
            New-Violation $script:Manifest.Rel $r.Line 'R1' "row $($r.Key) is not in the reflection dump"
        }
        if ($r.Kind -ceq $EngineClassKind -and -not $script:Reflection.Classes.Contains($r.Class)) {
            New-Violation $script:Manifest.Rel $r.Line 'R1' "stale: EngineClass row $($r.Key), but the reflection dump has no ReplicatedClass $($r.Class)"
        }
    }
    foreach ($c in $script:Reflection.Classes.Keys) {
        if (-not $named.Contains($c)) {
            New-Violation $rel $script:Reflection.Classes[$c] 'R1' "reflected ReplicatedClass $c is named by no manifest row; a class that replicates only through an engine base needs an EngineClass row (Session)"
        }
    }
}

function Invoke-ArmT1 {
    foreach ($r in $script:Manifest.Rows) {
        if ($r.Lane -ceq 'Sim' -or ($r.Kind -cne 'Property' -and $r.Kind -cne 'Rpc')) { continue }
        $type = $r.Type
        if ($script:SourceType.Contains($r.Key) -and $null -ne $script:SourceType[$r.Key]) { $type = $script:SourceType[$r.Key] }
        $parts = if ($r.Kind -ceq 'Rpc') { @(Split-TopLevel $type) } else { @($type) }
        foreach ($p in $parts) {
            if (Test-SimWireType $p) {
                $name = [regex]::Matches($p, '\w+') | ForEach-Object { $_.Value } | Where-Object { $script:SimWireTypes.ContainsKey($_) } | Select-Object -First 1
                New-Violation $script:Manifest.Rel $r.Line 'T1' ("row $($r.Key) ($($r.Lane)): type '$($p.Trim())' carries sim wire type $name, declared at $($script:SimWireTypes[$name])")
            }
        }
    }
}

function Invoke-ArmT2 {
    foreach ($r in $script:Manifest.Rows) {
        if (-not $script:SourceType.Contains($r.Key)) { continue }
        $src = $script:SourceType[$r.Key]
        if ($null -eq $src) { continue }
        if ((Get-NormalType $r.Type) -cne (Get-NormalType $src)) {
            $mk = $script:FirstMarker[$r.Key]
            New-Violation $script:Manifest.Rel $r.Line 'T2' "row $($r.Key): Type '$($r.Type)' but the source declares '$src' ($($mk.File):$($mk.Line))"
        }
    }
}

if (-not (Test-Path -LiteralPath $RepoRoot -PathType Container)) { Exit-Usage "RepoRoot not found: $RepoRoot" }
$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
$ManifestPath = [IO.Path]::GetFullPath([IO.Path]::Combine($RepoRoot, $ManifestPath))
if ($ScanRoots.Count -eq 0) { Exit-Usage 'no scan roots' }
$script:Manifest = Read-Manifest
$script:Reflection = if ($ReflectionLog -ne '') { Read-ReflectionLog } else { $null }
$files = Get-ScanFiles

$script:Markers = [Collections.Generic.List[object]]::new()
$script:SimWireTypes = @{}
foreach ($f in $files) { Read-SourceFile $f }

$script:FirstMarker = [ordered]@{}
$script:HeaderHalves = [ordered]@{}
$script:CppHalves = [ordered]@{}
$script:SourceType = @{}
foreach ($mk in $script:Markers) {
    if (-not $script:FirstMarker.Contains($mk.Key)) { $script:FirstMarker[$mk.Key] = $mk }
    if ($mk.Half -eq 'Header' -and -not $script:HeaderHalves.Contains($mk.Key)) { $script:HeaderHalves[$mk.Key] = $mk }
    if ($mk.Half -eq 'Cpp' -and -not $script:CppHalves.Contains($mk.Key)) { $script:CppHalves[$mk.Key] = $mk }
    if ($null -ne $mk.Type -and -not ($script:SourceType.Contains($mk.Key) -and $null -ne $script:SourceType[$mk.Key])) { $script:SourceType[$mk.Key] = $mk.Type }
    elseif (-not $script:SourceType.Contains($mk.Key)) { $script:SourceType[$mk.Key] = $null }
}
$script:RowKeys = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($r in $script:Manifest.Rows) { [void]$script:RowKeys.Add($r.Key) }

$violations = @()
$violations += @(Invoke-ArmJ1)
$violations += @(Invoke-ArmJ2)
$violations += @(Invoke-ArmJ3)
$violations += @(Invoke-ArmC1)
$violations += @(Invoke-ArmC2)
$violations += @(Invoke-ArmC3)
$violations += @(Invoke-ArmC4)
$violations += @(Invoke-ArmC5)
$violations += @(Invoke-ArmT1)
$violations += @(Invoke-ArmT2)
$violations += @(Invoke-ArmR1)
$violations = @($violations | Where-Object { $null -ne $_ })

foreach ($v in $violations) { Write-Host ('{0}:{1}: [E4-{2}] {3}' -f $v.File, $v.Line, $v.Arm, $v.Message) }

$rowsList = $script:Manifest.Rows
$count = @{}
foreach ($c in $LaneClasses) { $count[$c] = @($rowsList | Where-Object { $_.Lane -ceq $c }).Count }
Write-Host ('[E4] rows={0} Sim={1} Presentation={2} Session={3} Binding={4} frozen={5} sim-rows={6}' -f `
    $rowsList.Count, $count['Sim'], $count['Presentation'], $count['Session'], $count['Binding'], $Ph0SimKeys.Count, $script:Manifest.Flag)
if ($null -ne $script:Reflection) {
    Write-Host ('[E4] reflection={0} lines={1} keys={2} classes={3}' -f `
        $script:Reflection.Rel, $script:Reflection.Lines, $script:Reflection.Members.Count, $script:Reflection.Classes.Count)
}
Write-Host ('[E4] files={0} markers={1} keys={2} violations={3} elapsed={4} s' -f `
    $files.Count, $script:Markers.Count, $script:FirstMarker.Count, $violations.Count,
    $clock.Elapsed.TotalSeconds.ToString('0.00', [Globalization.CultureInfo]::InvariantCulture))

if ($violations.Count -gt 0) { exit 1 }
exit 0
