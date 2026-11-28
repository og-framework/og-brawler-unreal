#Requires -Version 7.0
# SPDX-License-Identifier: MPL-2.0
# tools/lint/README.md "Module tier lint"
[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..' '..')).Path,
    [string]$TablePath = (Join-Path $PSScriptRoot 'module_tiers.psd1'),
    [ValidateSet('None', 'A1', 'A2', 'A3', 'A4', 'R1', 'R2')]
    [string]$Poison = 'None',
    [string[]]$FixturesPath = @(Join-Path $PSScriptRoot 'tests' 'fixtures' 'module_tier' 'clean.tree.txt'),
    [string]$ExpandTo = '',
    [switch]$ListFiles
)

$ErrorActionPreference = 'Stop'
$Arms = @('A1', 'A2', 'A3', 'A4', 'R1', 'R2')
$TopKeys = @('SourceParents', 'ExcludeSegments', 'Extensions', 'EngineHeaders', 'Nodes', 'Unlinted', 'Passengers', 'Watch', 'ReachClasses')
$NodeKeys = @('Tier', 'EngineFree', 'Group', 'Roots', 'Names', 'ExceptNames', 'ModuleBoilerplate', 'MayNotInclude')

function Exit-Usage {
    param([string]$Message)
    [Console]::Error.WriteLine("module_tier_lint: $Message")
    exit 2
}

function ConvertTo-GlobRegex {
    param([string]$Glob)
    $parts = $Glob.Replace('\', '/').Trim('/').Split('/')
    $sb = [Text.StringBuilder]::new('^')
    for ($i = 0; $i -lt $parts.Count; $i++) {
        $p = $parts[$i]
        if ($p -eq '**') { [void]$sb.Append('(?:[^/]+/)*'); continue }
        $seg = [regex]::Escape($p).Replace('\*', '[^/]*').Replace('\?', '[^/]')
        [void]$sb.Append($seg)
        if ($i -lt $parts.Count - 1) { [void]$sb.Append('/') }
    }
    [void]$sb.Append('$')
    return [regex]::new($sb.ToString(), [Text.RegularExpressions.RegexOptions]::IgnoreCase)
}

function Read-Table {
    param([string]$Path)
    if (-not [IO.File]::Exists($Path)) { Exit-Usage "table not found: $Path" }
    try { $t = Import-PowerShellDataFile -LiteralPath $Path } catch { Exit-Usage "table does not parse: $Path ($($_.Exception.Message))" }
    foreach ($k in $t.Keys) { if ($TopKeys -notcontains $k) { Exit-Usage "table: unknown key '$k'" } }
    foreach ($k in @('SourceParents', 'ExcludeSegments', 'Extensions', 'EngineHeaders', 'Nodes')) {
        if (-not $t.ContainsKey($k) -or @($t[$k]).Count -eq 0) { Exit-Usage "table: '$k' is missing or empty" }
    }
    if ($t.Nodes -isnot [hashtable]) { Exit-Usage "table: 'Nodes' is not a table" }
    $nodes = [ordered]@{}
    foreach ($name in $t.Nodes.Keys) {
        $n = $t.Nodes[$name]
        if ($n -isnot [hashtable]) { Exit-Usage "table: node '$name' is not a table" }
        foreach ($k in $n.Keys) { if ($NodeKeys -notcontains $k) { Exit-Usage "table: node '$name' has unknown key '$k'" } }
        if (-not ($n.Tier -is [int]) -or $n.Tier -lt 0) { Exit-Usage "table: node '$name' needs an integer Tier >= 0" }
        if (-not ($n.EngineFree -is [bool])) { Exit-Usage "table: node '$name' needs EngineFree = `$true or `$false" }
        if (@($n.Roots | Where-Object { $_ }).Count -eq 0) { Exit-Usage "table: node '$name' has no Roots" }
        $nodes[$name] = [pscustomobject]@{
            Name = $name; Tier = $n.Tier; EngineFree = $n.EngineFree
            Group = if ($n.Group) { [string]$n.Group } else { $name }
            Roots = @($n.Roots | ForEach-Object { [pscustomobject]@{ Glob = $_; Rx = (ConvertTo-GlobRegex $_) } })
            Names = @($n.Names | Where-Object { $_ }); ExceptNames = @($n.ExceptNames | Where-Object { $_ })
            Boilerplate = @($n.ModuleBoilerplate | Where-Object { $_ })
            HasNames = (@($n.Names | Where-Object { $_ }).Count -gt 0)
            MayNotInclude = @($n.MayNotInclude | Where-Object { $_ })
        }
    }
    foreach ($n in $nodes.Values) {
        foreach ($x in $n.MayNotInclude) {
            if (-not $nodes.Contains($x)) { Exit-Usage "table: node '$($n.Name)' MayNotInclude names '$x', which is not a node" }
            if ($x -eq $n.Name) { Exit-Usage "table: node '$($n.Name)' MayNotInclude names itself" }
        }
    }
    $unlinted = @()
    if ($t.Unlinted) {
        foreach ($g in $t.Unlinted.Keys) {
            if (-not $t.Unlinted[$g]) { Exit-Usage "table: Unlinted entry '$g' has no reason" }
            $unlinted += [pscustomobject]@{ Glob = $g; Rx = (ConvertTo-GlobRegex $g); Reason = $t.Unlinted[$g] }
        }
    }
    $passengers = $null
    if ($t.Passengers) {
        $of = @($t.Passengers.Of | Where-Object { $_ })
        $riders = @($t.Passengers.Riders | Where-Object { $_ })
        if ($of.Count -eq 0 -or $riders.Count -eq 0) { Exit-Usage 'table: Passengers needs Of and Riders' }
        foreach ($r in $of + $riders) { if (-not $nodes.Contains($r)) { Exit-Usage "table: Passengers names '$r', which is not a node" } }
        $passengers = [pscustomobject]@{ Of = $of; Riders = $riders }
    }
    $watch = @()
    foreach ($w in @($t.Watch)) {
        if ($null -eq $w) { continue }
        if (-not $nodes.Contains($w.From) -or -not $nodes.Contains($w.To)) { Exit-Usage "table: Watch '$($w.From) -> $($w.To)' names an unknown node" }
        $watch += [pscustomobject]@{ From = $w.From; To = $w.To }
    }
    $reach = @()
    foreach ($c in @($t.ReachClasses)) {
        if ($null -eq $c) { continue }
        if (-not $c.Class -or @($c.DeclaredIn | Where-Object { $_ }).Count -eq 0) { Exit-Usage "table: a ReachClasses entry needs Class and DeclaredIn" }
        $names = @($c.Class) + @($c.Aliases | Where-Object { $_ })
        $alt = ($names | ForEach-Object { [regex]::Escape($_) }) -join '|'
        $targ = '<(?:[^<>;{}()]|<(?:[^<>;{}()]|<[^<>;{}()]*>)*>)*>'
        $reach += [pscustomobject]@{
            Class = $c.Class; DeclaredIn = @($c.DeclaredIn)
            DeclRx = [regex]::new("(?<![\w:])(?:$alt)\s*(?:$targ)?\s*(?:const\s*)?[&*]{1,2}\s*(?:const\s+)?([A-Za-z_]\w*)")
        }
    }
    return [pscustomobject]@{
        SourceParents = @($t.SourceParents | ForEach-Object { [pscustomobject]@{ Glob = $_; Rx = (ConvertTo-GlobRegex $_) } })
        ExcludeSegments = @($t.ExcludeSegments); Extensions = @($t.Extensions); EngineHeaders = @($t.EngineHeaders)
        Nodes = $nodes; Unlinted = $unlinted; Passengers = $passengers; Watch = $watch; Reach = $reach
    }
}

function Test-Excluded {
    param([string]$Leaf, $Table)
    foreach ($p in $Table.ExcludeSegments) { if ($Leaf -clike $p) { return $true } }
    return $false
}

function Get-CodeFiles {
    param([string]$Dir, $Table)
    $out = [Collections.Generic.List[string]]::new()
    $stack = [Collections.Generic.Stack[string]]::new()
    $stack.Push($Dir)
    while ($stack.Count -gt 0) {
        $d = $stack.Pop()
        foreach ($f in [IO.Directory]::EnumerateFiles($d)) {
            if ($Table.Extensions -contains [IO.Path]::GetExtension($f).ToLowerInvariant()) { $out.Add($f) }
        }
        foreach ($s in [IO.Directory]::EnumerateDirectories($d)) {
            if (-not (Test-Excluded ([IO.Path]::GetFileName($s)) $Table)) { $stack.Push($s) }
        }
    }
    return $out
}

$StripRx = [regex]::new('//[^\n]*|/\*[\s\S]*?(?:\*/|\z)|"(?:\\.|[^"\\\n])*"?|(?<![A-Za-z0-9_])''(?:\\.|[^''\\\n])*''?')
$IncludeRx = [regex]::new('(?m)^[ \t]*#[ \t]*include[ \t]*(?:"([^"\n]+)"|<([^>\n]+)>)')

function Get-Views {
    param([string]$Text)
    $noComments = $StripRx.Replace($Text, {
            param($m)
            if ($m.Value.StartsWith('/')) { return [regex]::Replace($m.Value, '[^\n]', ' ') }
            return $m.Value
        })
    $bare = $StripRx.Replace($Text, {
            param($m)
            if ($m.Value.StartsWith('/')) { return [regex]::Replace($m.Value, '[^\n]', ' ') }
            return $m.Value.Substring(0, 1) + [regex]::Replace($m.Value.Substring(1), '[^\n]', ' ')
        })
    $starts = [Collections.Generic.List[int]]::new()
    $starts.Add(0)
    for ($i = 0; $i -lt $Text.Length; $i++) { if ($Text[$i] -eq "`n") { $starts.Add($i + 1) } }
    return [pscustomobject]@{ NoComments = $noComments; Bare = $bare; Starts = $starts }
}

function Get-LineOf {
    param($Starts, [int]$Index)
    $k = $Starts.BinarySearch($Index)
    if ($k -lt 0) { $k = (-bnot $k) - 1 }
    return $k + 1
}

function Get-Model {
    param([string]$Root, $Table)
    $Root = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $nodeDirs = @{}; foreach ($n in $Table.Nodes.Keys) { $nodeDirs[$n] = [Collections.Generic.List[string]]::new() }
    $unlintedDirs = [Collections.Generic.List[string]]::new()
    $parentDirs = [Collections.Generic.List[string]]::new()
    $stack = [Collections.Generic.Stack[string]]::new()
    $stack.Push($Root)
    while ($stack.Count -gt 0) {
        $d = $stack.Pop()
        foreach ($s in [IO.Directory]::EnumerateDirectories($d)) {
            if (Test-Excluded ([IO.Path]::GetFileName($s)) $Table) { continue }
            $rel = [IO.Path]::GetRelativePath($Root, $s).Replace('\', '/')
            foreach ($n in $Table.Nodes.Values) {
                foreach ($r in $n.Roots) { if ($r.Rx.IsMatch($rel)) { $nodeDirs[$n.Name].Add($rel); break } }
            }
            foreach ($u in $Table.Unlinted) { if ($u.Rx.IsMatch($rel)) { $unlintedDirs.Add($rel) } }
            foreach ($p in $Table.SourceParents) { if ($p.Rx.IsMatch($rel)) { $parentDirs.Add($rel) } }
            $stack.Push($s)
        }
    }

    $files = [ordered]@{}
    foreach ($n in $Table.Nodes.Values) {
        foreach ($dirRel in $nodeDirs[$n.Name]) {
            $depth = $dirRel.Split('/').Count
            foreach ($f in (Get-CodeFiles ([IO.Path]::Combine($Root, $dirRel)) $Table)) {
                $rel = [IO.Path]::GetRelativePath($Root, $f).Replace('\', '/')
                $base = [IO.Path]::GetFileName($f)
                if ($n.HasNames) {
                    $hit = $false
                    foreach ($p in $n.Names) { if ($base -clike $p) { $hit = $true; break } }
                    foreach ($p in $n.ExceptNames) { if ($base -clike $p) { $hit = $false; break } }
                    if (-not $hit) { continue }
                }
                if (-not $files.Contains($rel)) { $files[$rel] = [Collections.Generic.List[object]]::new() }
                $files[$rel].Add([pscustomobject]@{ Node = $n.Name; Depth = $depth; HasNames = $n.HasNames; RootDir = $dirRel })
            }
        }
    }
    $records = [Collections.Generic.List[object]]::new()
    $byRel = @{}
    foreach ($rel in $files.Keys) {
        $claims = @($files[$rel] | Sort-Object -Property @{ Expression = 'Depth'; Descending = $true }, @{ Expression = 'HasNames'; Descending = $true })
        $best = @($claims | Where-Object { $_.Depth -eq $claims[0].Depth -and $_.HasNames -eq $claims[0].HasNames })
        $bestNodes = @($best | ForEach-Object Node | Sort-Object -Unique)
        if ($bestNodes.Count -gt 1) { Exit-Usage "table: $rel is claimed by $($bestNodes -join ' and ') at the same depth" }
        if ($byRel.ContainsKey($rel)) { continue }
        $node = $Table.Nodes[$bestNodes[0]]
        $rootDir = ($best | Sort-Object { $_.RootDir.Length } -Descending)[0].RootDir
        $leaf = $rootDir.Split('/')[-1]
        $within = $rel.Substring($rootDir.Length + 1)
        $text = [IO.File]::ReadAllText([IO.Path]::Combine($Root, $rel)).Replace("`r`n", "`n")
        $rec = [pscustomobject]@{
            Rel = $rel; Full = [IO.Path]::GetFullPath([IO.Path]::Combine($Root, $rel)); Node = $node.Name; Group = $node.Group
            Tier = $node.Tier; Base = [IO.Path]::GetFileName($rel); IncludePath = "$leaf/$within"
            IsHeader = @('.h', '.hpp', '.inl') -contains [IO.Path]::GetExtension($rel).ToLowerInvariant()
            Views = (Get-Views $text); Includes = [Collections.Generic.List[object]]::new()
        }
        $records.Add($rec); $byRel[$rel] = $rec
    }

    $unlintedSet = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($u in ($unlintedDirs | Sort-Object -Unique)) {
        foreach ($f in (Get-CodeFiles ([IO.Path]::Combine($Root, $u)) $Table)) {
            $rel = [IO.Path]::GetRelativePath($Root, $f).Replace('\', '/')
            if (-not $byRel.ContainsKey($rel)) { [void]$unlintedSet.Add($rel) }
        }
    }
    $unlintedFiles = @($unlintedSet | Sort-Object)

    $uncovered = [ordered]@{}
    foreach ($p in ($parentDirs | Sort-Object -Unique)) {
        $miss = @()
        foreach ($f in (Get-CodeFiles ([IO.Path]::Combine($Root, $p)) $Table)) {
            $rel = [IO.Path]::GetRelativePath($Root, $f).Replace('\', '/')
            if (-not $byRel.ContainsKey($rel) -and -not $unlintedSet.Contains($rel)) { $miss += $rel }
        }
        if ($miss.Count -gt 0) { $uncovered[$p] = @($miss | Sort-Object) }
    }

    return [pscustomobject]@{
        Root = $Root; Records = $records; ByRel = $byRel; NodeDirs = $nodeDirs; ParentDirs = @($parentDirs | Sort-Object -Unique)
        UnlintedFiles = $unlintedFiles; Uncovered = $uncovered
    }
}

function Invoke-Checks {
    param($Model, $Table)
    $byFull = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    $suffix = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($r in $Model.Records) {
        $byFull[$r.Full] = $r
        $segs = $r.Rel.Split('/')
        for ($i = $segs.Count - 1; $i -ge 0; $i--) {
            $key = ($segs[$i..($segs.Count - 1)] -join '/')
            if (-not $suffix.ContainsKey($key)) { $suffix[$key] = [Collections.Generic.List[object]]::new() }
            $suffix[$key].Add($r)
        }
    }
    $engineRx = @($Table.EngineHeaders | ForEach-Object { ConvertTo-GlobRegex $_ })
    $res = [ordered]@{ A1 = @(); A2 = @(); A3 = @(); A4 = @(); R1 = @(); R2 = @() }
    $ambiguous = @()
    $users = @{}; $ownIncluders = @{}
    $watchHits = @{}; foreach ($w in $Table.Watch) { $watchHits["$($w.From)>$($w.To)"] = [Collections.Generic.SortedSet[string]]::new() }

    foreach ($f in $Model.Records) {
        $fNode = $Table.Nodes[$f.Node]
        $fDir = [IO.Path]::GetDirectoryName($f.Full)
        foreach ($m in $IncludeRx.Matches($f.Views.NoComments)) {
            $inc = if ($m.Groups[1].Success) { $m.Groups[1].Value } else { $m.Groups[2].Value }
            $inc = $inc.Trim().Replace('\', '/')
            $line = Get-LineOf $f.Views.Starts $m.Index
            $targets = @()
            $local = [IO.Path]::GetFullPath([IO.Path]::Combine($fDir, $inc))
            if ($byFull.ContainsKey($local)) { $targets = @($byFull[$local]) }
            else {
                $key = $inc
                while ($key.StartsWith('./')) { $key = $key.Substring(2) }
                if ($suffix.ContainsKey($key)) {
                    $cands = @($suffix[$key])
                    $own = @($cands | Where-Object { $_.Node -eq $f.Node })
                    if ($own.Count -gt 0) { $targets = @($own[0]) }
                    else {
                        $targets = @($cands | Group-Object Node | ForEach-Object { $_.Group[0] })
                        if ($targets.Count -gt 1) {
                            $ambiguous += "$($f.Rel):${line}: [ModuleTier.Ambiguous] `"$inc`" matches headers of $((@($targets | ForEach-Object Node)) -join ', ')"
                        }
                    }
                }
            }
            if ($targets.Count -eq 0) {
                if ($fNode.EngineFree -and $fNode.Boilerplate -notcontains $f.Base) {
                    foreach ($rx in $engineRx) {
                        if ($rx.IsMatch($inc)) {
                            $res.A2 += "$($f.Rel):${line}: [ModuleTier.A2] $($f.Node) is engine-free and includes the engine header `"$inc`""
                            break
                        }
                    }
                }
                continue
            }
            foreach ($t in $targets) {
                if ($t.Rel -eq $f.Rel) { continue }
                $f.Includes.Add($t)
                if ($t.Tier -gt $f.Tier) {
                    $res.A1 += "$($f.Rel):${line}: [ModuleTier.A1] $($f.Node) (tier $($f.Tier)) includes `"$inc`" of $($t.Node) (tier $($t.Tier))"
                }
                elseif ($fNode.MayNotInclude -contains $t.Node) {
                    $res.A4 += "$($f.Rel):${line}: [ModuleTier.A4] $($f.Node) may not include $($t.Node): `"$inc`""
                }
                if ($t.Group -eq $f.Group) {
                    $ownCpp = (-not $f.IsHeader) -and ([IO.Path]::GetFileNameWithoutExtension($f.Base) -eq [IO.Path]::GetFileNameWithoutExtension($t.Base))
                    if (-not $ownCpp) {
                        if (-not $ownIncluders.ContainsKey($t.Rel)) { $ownIncluders[$t.Rel] = [Collections.Generic.HashSet[string]]::new() }
                        [void]$ownIncluders[$t.Rel].Add($f.Rel)
                    }
                }
                else {
                    if (-not $users.ContainsKey($t.Rel)) { $users[$t.Rel] = [Collections.Generic.SortedSet[string]]::new() }
                    [void]$users[$t.Rel].Add($f.Node)
                }
                $wk = "$($f.Node)>$($t.Node)"
                if ($watchHits.ContainsKey($wk)) { [void]$watchHits[$wk].Add("$($f.Rel) -> $($t.IncludePath)") }
            }
        }
    }

    foreach ($p in $Model.Uncovered.Keys) {
        $miss = $Model.Uncovered[$p]
        $shown = ($miss | Select-Object -First 3) -join ', '
        $more = if ($miss.Count -gt 3) { ", ... ($($miss.Count) files)" } else { '' }
        $res.A3 += "${p}: [ModuleTier.A3] source root no node and no Unlinted entry covers: $shown$more"
    }

    $orphans = [Collections.Generic.HashSet[string]]::new()
    $changed = $true
    while ($changed) {
        $changed = $false
        foreach ($r in $Model.Records) {
            if (-not $r.IsHeader -or $orphans.Contains($r.Rel) -or -not $users.ContainsKey($r.Rel)) { continue }
            $live = @()
            if ($ownIncluders.ContainsKey($r.Rel)) { $live = @($ownIncluders[$r.Rel] | Where-Object { -not $orphans.Contains($_) }) }
            if ($live.Count -eq 0) { [void]$orphans.Add($r.Rel); $changed = $true }
        }
    }
    $passengerSet = [Collections.Generic.HashSet[string]]::new()
    $passengers = @()
    if ($Table.Passengers) {
        $of = $Table.Passengers.Of; $riders = $Table.Passengers.Riders
        $needed = [Collections.Generic.HashSet[string]]::new(); $reached = [Collections.Generic.HashSet[string]]::new()
        $needQ = [Collections.Generic.Queue[object]]::new(); $rideQ = [Collections.Generic.Queue[object]]::new()
        foreach ($f in $Model.Records) {
            $isRider = $riders -contains $f.Node
            if (-not $isRider -and $of -contains $f.Node -and $f.IsHeader) { continue }
            foreach ($t in $f.Includes) {
                if ($of -notcontains $t.Node) { continue }
                if ($isRider) { $rideQ.Enqueue($t); continue }
                $ownCpp = [IO.Path]::GetFileNameWithoutExtension($f.Base) -eq [IO.Path]::GetFileNameWithoutExtension($t.Base)
                if (-not ($of -contains $f.Node -and $ownCpp)) { $needQ.Enqueue($t) }
            }
        }
        foreach ($pair in @(@($needQ, $needed), @($rideQ, $reached))) {
            $q = $pair[0]; $set = $pair[1]
            while ($q.Count -gt 0) {
                $t = $q.Dequeue()
                if (-not $set.Add($t.Rel)) { continue }
                foreach ($u in $t.Includes) { if ($of -contains $u.Node) { $q.Enqueue($u) } }
            }
        }
        foreach ($r in ($Model.Records | Where-Object { $reached.Contains($_.Rel) -and -not $needed.Contains($_.Rel) } | Sort-Object IncludePath)) {
            [void]$passengerSet.Add($r.Rel)
            $direct = if ($users.ContainsKey($r.Rel)) { @($users[$r.Rel]) -join ', ' } else { 'only through other passengers' }
            $passengers += "$($r.IncludePath) <- $direct"
        }
    }
    foreach ($r in ($Model.Records | Where-Object { $orphans.Contains($_.Rel) } | Sort-Object Node, IncludePath)) {
        $u = @($users[$r.Rel])
        $mark = if ($passengerSet.Contains($r.Rel)) { ' [passenger]' } else { '' }
        $res.R1 += "$($r.Rel): [ModuleTier.R1] $($r.IncludePath) ($($r.Node)) is included by no file of its own group; used by $($u -join ', ')$mark"
    }

    $watchLines = @()
    foreach ($w in $Table.Watch) {
        $present = @($Model.Records | Where-Object { $_.Node -eq $w.From }).Count -gt 0
        $hits = @($watchHits["$($w.From)>$($w.To)"])
        if (-not $present) { $watchLines += "[ModuleTier.R1.Watch] $($w.From) -> $($w.To): $($w.From) absent" }
        elseif ($hits.Count -eq 0) { $watchLines += "[ModuleTier.R1.Watch] $($w.From) -> $($w.To): none" }
        else { foreach ($h in $hits) { $watchLines += "[ModuleTier.R1.Watch] $($w.From) -> $($w.To): $h" } }
    }

    foreach ($c in $Table.Reach) {
        foreach ($f in $Model.Records) {
            if ($c.DeclaredIn -contains $f.Base) { continue }
            $vars = @($c.DeclRx.Matches($f.Views.Bare) | ForEach-Object { $_.Groups[1].Value } | Where-Object { $_ -ne 'const' } | Sort-Object -Unique)
            if ($vars.Count -eq 0) { continue }
            $siteRx = [regex]::new('(?<![\w.])(' + (($vars | ForEach-Object { [regex]::Escape($_) }) -join '|') + ')\s*(?:\.|->)\s*(m_\w+)')
            foreach ($m in $siteRx.Matches($f.Views.Bare)) {
                $line = Get-LineOf $f.Views.Starts $m.Index
                $res.R2 += "$($f.Rel):${line}: [ModuleTier.R2] $($m.Groups[1].Value).$($m.Groups[2].Value) reaches a member of $($c.Class)"
            }
        }
    }

    return [pscustomobject]@{ Arms = $res; Ambiguous = $ambiguous; Watch = $watchLines; Passengers = $passengers; OwnIncluders = $ownIncluders }
}

function Write-Report {
    param($Model, $Table, $Result, [string]$Label)
    Write-Output "module_tier_lint: $Label"
    Write-Output "  repo root: $($Model.Root)"
    foreach ($n in ($Table.Nodes.Values | Sort-Object Tier, Name)) {
        $count = @($Model.Records | Where-Object { $_.Node -eq $n.Name }).Count
        $free = if ($n.EngineFree) { 'engine-free' } else { 'engine' }
        $state = if ($count -gt 0) { "present, $count files" } else { 'absent' }
        Write-Output ('  {0,-24} tier {1}  {2,-11}  {3}' -f $n.Name, $n.Tier, $free, $state)
    }
    Write-Output "  unlinted files: $($Model.UnlintedFiles.Count); source roots checked: $($Model.ParentDirs.Count)"
    $titles = [ordered]@{ A1 = 'A1 upward include (gate)'; A2 = 'A2 engine header in an engine-free node (gate)'
        A3 = 'A3 source root no node covers (gate)'; A4 = 'A4 include of a node MayNotInclude names (gate)'; R1 = 'R1 orphan headers (report)'; R2 = 'R2 private-member reach (report)' }
    foreach ($a in $Arms) {
        $extra = if ($a -eq 'R1') { " ($(@($Result.Passengers).Count) passenger(s))" } else { '' }
        Write-Output ("  {0}: {1}{2}" -f $titles[$a], $Result.Arms[$a].Count, $extra)
        foreach ($x in $Result.Arms[$a]) { Write-Output "    $x" }
        if ($a -eq 'R1') {
            if ($Table.Passengers) {
                Write-Output "    [ModuleTier.R1.Passengers] headers of $($Table.Passengers.Of -join ', ') that only $($Table.Passengers.Riders -join ', ') reach: $(@($Result.Passengers).Count)"
                foreach ($x in $Result.Passengers) { Write-Output "      $x" }
            }
            foreach ($x in $Result.Watch) { Write-Output "    $x" }
        }
    }
    foreach ($x in $Result.Ambiguous) { Write-Output "    $x" }
}

function Expand-Fixtures {
    param([string[]]$Sources, [string]$Dest)
    New-Item -ItemType Directory -Force $Dest | Out-Null
    foreach ($src in $Sources) {
        if ([IO.Directory]::Exists($src)) {
            Copy-Item -Path (Join-Path $src '*') -Destination $Dest -Recurse -Force
            continue
        }
        if (-not [IO.File]::Exists($src)) { Exit-Usage "fixture not found: $src" }
        $path = $null; $buf = [Collections.Generic.List[string]]::new(); $count = 0
        $lines = [IO.File]::ReadAllText($src).Replace("`r`n", "`n").Split("`n")
        foreach ($line in $lines + @('>>> ')) {
            if ($line.StartsWith('>>> ')) {
                if ($path) {
                    $full = [IO.Path]::GetFullPath([IO.Path]::Combine($Dest, $path))
                    New-Item -ItemType Directory -Force ([IO.Path]::GetDirectoryName($full)) | Out-Null
                    [IO.File]::WriteAllText($full, (($buf -join "`n").TrimEnd("`n") + "`n"))
                    $count++
                }
                $path = $line.Substring(4).Trim()
                if ($path -and ([IO.Path]::IsPathRooted($path) -or $path.Split('/') -contains '..')) { Exit-Usage "fixture ${src}: bad path '$path'" }
                $buf.Clear()
                continue
            }
            if ($path) { $buf.Add($line) }
        }
        if ($count -eq 0) { Exit-Usage "fixture $src holds no '>>> path' entry" }
    }
}

function Get-Counts {
    param($Result)
    $c = [ordered]@{}
    foreach ($a in $Arms) { $c[$a] = $Result.Arms[$a].Count }
    return $c
}

$table = Read-Table $TablePath

if ($ExpandTo) {
    Expand-Fixtures $FixturesPath $ExpandTo
    exit 0
}

if ($Poison -ne 'None') {
    $tmp = Join-Path ([IO.Path]::GetTempPath()) ('module_tier_poison_' + [guid]::NewGuid().ToString('N'))
    try {
        Expand-Fixtures $FixturesPath $tmp
        $model = Get-Model $tmp $table
        $clean = Invoke-Checks $model $table
        $cc = Get-Counts $clean
        $dirty = @($Arms | Where-Object { $cc[$_] -gt 0 })
        if ($dirty.Count -gt 0) {
            Write-Report $model $table $clean 'fixture copy before the poison'
            [Console]::Error.WriteLine("POISON: the fixture copy is not clean before planting ($($dirty -join ', ')); the arm cannot be judged.")
            exit 2
        }
        $recs = @($model.Records | Sort-Object Rel)
        if ($recs.Count -eq 0) { Exit-Usage 'POISON: the fixture copy holds no node file' }
        $desc = ''
        switch ($Poison) {
            'A1' {
                $low = ($recs | Sort-Object Tier, Rel)[0]
                $victim = @($recs | Where-Object { $_.IsHeader -and $_.Tier -gt $low.Tier -and $clean.OwnIncluders.ContainsKey($_.Rel) } |
                    Sort-Object @{ Expression = 'Tier'; Descending = $true }, Rel)[0]
                if (-not $victim) { Exit-Usage 'POISON A1: the fixture has no higher-tier header that its own group includes' }
                Add-Content -LiteralPath (Join-Path $tmp $low.Rel) -Value "#include `"$($victim.IncludePath)`""
                $desc = "$($low.Rel) ($($low.Node), tier $($low.Tier)) now includes $($victim.IncludePath) ($($victim.Node), tier $($victim.Tier))"
            }
            'A2' {
                $f = @($recs | Where-Object { $table.Nodes[$_.Node].EngineFree -and $table.Nodes[$_.Node].Boilerplate -notcontains $_.Base })[0]
                if (-not $f) { Exit-Usage 'POISON A2: the fixture has no engine-free file' }
                Add-Content -LiteralPath (Join-Path $tmp $f.Rel) -Value '#include "CoreMinimal.h"'
                $desc = "$($f.Rel) ($($f.Node)) now includes CoreMinimal.h"
            }
            'A3' {
                if ($model.ParentDirs.Count -eq 0) { Exit-Usage 'POISON A3: no SourceParents directory in the fixture' }
                $parent = $model.ParentDirs[0] -replace '/[^/]+$', ''
                $dir = Join-Path $tmp $parent 'ModuleTierPoison'
                New-Item -ItemType Directory -Force $dir | Out-Null
                Set-Content -LiteralPath (Join-Path $dir 'ModuleTierPoison.h') -Value '#pragma once'
                $desc = "new source root $parent/ModuleTierPoison with one header"
            }
            'A4' {
                $from = $null; $victim = $null
                foreach ($f in $recs) {
                    $banned = $table.Nodes[$f.Node].MayNotInclude
                    if ($banned.Count -eq 0) { continue }
                    $victim = @($recs | Where-Object { $_.IsHeader -and $banned -contains $_.Node -and $_.Tier -le $f.Tier -and
                            $clean.OwnIncluders.ContainsKey($_.Rel) })[0]
                    if ($victim) { $from = $f; break }
                }
                if (-not $from) { Exit-Usage 'POISON A4: the fixture has no MayNotInclude pair with an own-included header at a tier A1 allows' }
                Add-Content -LiteralPath (Join-Path $tmp $from.Rel) -Value "#include `"$($victim.IncludePath)`""
                $desc = "$($from.Rel) ($($from.Node)) now includes $($victim.IncludePath) ($($victim.Node), which it may not include)"
            }
            'R1' {
                $x = ($recs | Sort-Object Tier, Rel)[0]
                $y = @($recs | Where-Object { $_.Group -ne $x.Group -and $_.Tier -ge $x.Tier } | Sort-Object @{ Expression = 'Tier'; Descending = $true }, Rel)[0]
                if (-not $y) { Exit-Usage 'POISON R1: the fixture has no second group' }
                $leaf = $x.IncludePath.Split('/')[0]
                $within = $x.IncludePath.Substring($leaf.Length + 1)
                $rootDir = Join-Path $tmp $x.Rel.Substring(0, $x.Rel.Length - $within.Length - 1)
                Set-Content -LiteralPath (Join-Path $rootDir 'ModuleTierPoisonOrphan.h') -Value '#pragma once'
                Add-Content -LiteralPath (Join-Path $tmp $y.Rel) -Value "#include `"$leaf/ModuleTierPoisonOrphan.h`""
                $desc = "new $leaf/ModuleTierPoisonOrphan.h ($($x.Node)) included only by $($y.Rel) ($($y.Node))"
            }
            'R2' {
                if ($table.Reach.Count -eq 0) { Exit-Usage 'POISON R2: the table names no ReachClasses' }
                $c = $table.Reach[0]
                $f = @($recs | Where-Object { $c.DeclaredIn -notcontains $_.Base -and $_.IsHeader })[0]
                Add-Content -LiteralPath (Join-Path $tmp $f.Rel) -Value "struct ModuleTierPoisonReach { $($c.Class)<int>& m_poisonOwner; int f() { return m_poisonOwner.m_poisonMember; } };"
                $desc = "$($f.Rel) now reaches m_poisonOwner.m_poisonMember of $($c.Class)"
            }
        }
        $model = Get-Model $tmp $table
        $result = Invoke-Checks $model $table
        Write-Report $model $table $result "POISON [$Poison] on a temporary copy of $($FixturesPath -join ' + ')"
        Write-Output "  POISON [$Poison]: $desc"
        $pc = Get-Counts $result
        $leaks = @($Arms | Where-Object { $_ -ne $Poison -and $pc[$_] -gt 0 })
        if ($pc[$Poison] -eq 0) {
            [Console]::Error.WriteLine("POISON ARM DID NOT FIRE: $Poison stayed at 0. The arm is vacuous; this is an instrument failure.")
            exit 2
        }
        if ($leaks.Count -gt 0) {
            [Console]::Error.WriteLine("POISON LEAKED: [$Poison] also moved $($leaks -join ', '). The arms are not independent.")
            exit 2
        }
        Write-Output "  POISON ARM FIRED: $Poison = $($pc[$Poison]), every other arm 0. RESULT: FAIL (as required)"
        exit 1
    }
    finally {
        if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }
    }
}

if (-not [IO.Directory]::Exists($RepoRoot)) { Exit-Usage "repo root not found: $RepoRoot" }
$model = Get-Model $RepoRoot $table
if ($model.Records.Count -eq 0) { Exit-Usage "no table root resolves to a source file under $RepoRoot" }

if ($ListFiles) {
    foreach ($r in ($model.Records | Sort-Object Rel)) { Write-Output "$($r.Node)`t$($r.Rel)" }
    foreach ($u in ($model.UnlintedFiles | Sort-Object)) { Write-Output "(unlinted)`t$u" }
    foreach ($p in $model.Uncovered.Keys) { foreach ($u in $model.Uncovered[$p]) { Write-Output "(uncovered)`t$u" } }
    exit 0
}

$result = Invoke-Checks $model $table
Write-Report $model $table $result 'the tree'
$c = Get-Counts $result
$gate = $c.A1 + $c.A2 + $c.A3 + $c.A4
if ($gate -gt 0) {
    Write-Output "  RESULT: FAIL - A1 $($c.A1), A2 $($c.A2), A3 $($c.A3), A4 $($c.A4) (R1 $($c.R1), R2 $($c.R2) reported)"
    exit 1
}
Write-Output "  RESULT: CLEAN - A1, A2, A3, A4 at zero (R1 $($c.R1), R2 $($c.R2) reported)"
exit 0
