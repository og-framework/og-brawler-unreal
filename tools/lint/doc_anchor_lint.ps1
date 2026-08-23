#Requires -Version 7.0
# SPDX-License-Identifier: MPL-2.0
<#
.SYNOPSIS
    Doc-anchor lint - every machine-checkable claim a document makes about the tree
    must resolve against the tree.

.DESCRIPTION
    A docs tier describes code that lives beside it. A document that names a
    file, a class member or a symbol is asserting that thing exists.
    This lint resolves every such assertion it can parse, and - just as important -
    COUNTS AND PRINTS the ones it cannot parse, so "the lint is clean" never gets
    read as "the document is verified".

    THERE IS MORE THAN ONE TIER, AND THAT IS A LICENCE FACT, NOT A PREFERENCE.
    `og-simulation` docs are MPL-2.0 and travel with that submodule;
    `Source/OGBrawlerUnreal/docs` is BUSL-1.1 and cannot live there (decision D9,
    2026-08-23). Both are discovered by default - see -DefaultDocDirs - because a
    tier nobody passes on the command line is not reported as unlinted, it is
    simply never looked at, and the run still says CLEAN.

    Ruled in by the og-source-doc-extraction initiative, gate G2 (2026-08-21),
    mechanism M1 of `StalenessMechanism.md`. It exists because six recorded
    staleness defects in the source initiative were all BROKEN JOINS: one fact
    asserted in two places, where the edit that broke it touched one place and
    could not see the other.

    ------------------------------------------------------------------------
    ANCHOR GRAMMAR - what this lint can check
    ------------------------------------------------------------------------
      FILE      `Some/Path/File.h`        the path SUFFIX must match a file under a
                                          scan root.
      FILE:LINE `File.h:1120`             ... and the line (or `1120-1124` range)
                                          must be inside that file.
      PAIR      `File.h` :: `symbol`      the symbol must occur in THAT file, on a
                or a markdown table cell   line that is not a comment. Two spellings:
                holding `File.h` followed  an explicit ` :: ` between two backticked
                by a cell starting with    tokens, and adjacent table cells (the two
                `symbol`                   perspective docs use one each).
                                          A trailing `, `sym2`` list is checked too,
                                          each symbol against the same file.
      QSYM      `Class::member`           some file must DECLARE that class (or
                                          namespace, or enum class) and mention the
                                          member in non-comment code - or some file
                                          must carry the literal `Class::member` in
                                          non-comment code (out-of-line definitions,
                                          split .h/.cpp).
      MEMBER    `m_someMember`            must occur in non-comment code somewhere.
      IDENT     `someCamelCaseName`       must occur in non-comment code somewhere.
                                          Deliberately narrow: lowerCamelCase with at
                                          least one internal capital. A single
                                          lowercase word (`get`, `now`, `today`) is
                                          prose, not an anchor, and is UNCHECKED.
                                          Disable with -NoBareIdentifiers.

    Everything else inside backticks is COUNTED AND REPORTED AS UNCHECKED.

    ------------------------------------------------------------------------
    WHY COMMENT-STRIPPING IS LOAD-BEARING (do not remove it)
    ------------------------------------------------------------------------
    Symbol resolution runs against source text with `//`, `/* */` (and, in .ini,
    `;`) comments removed - the same discipline as configurability_lint.ps1.
    Measured, not assumed: `SimulationNetSync::collectInputAll` is a dead owner
    (the method moved to SimulationInputResolution), yet `SimulationNetSync.h`
    still mentions `collectInputAll` four times - ALL FOUR IN COMMENTS. Without
    the strip this lint resolves that anchor and silently passes the single
    defect the initiative most wants caught. -NoCommentStrip exists ONLY to
    demonstrate that false negative; never run it in anger.

    ------------------------------------------------------------------------
    ESCAPES - a lint reads names, not sentences
    ------------------------------------------------------------------------
    A document that says "`m_relayedInputStores` was retired at item 72" is
    CORRECT precisely because the name does not resolve. The lint cannot tell a
    mention from an assertion, so documents declare the exception - and every
    escape is ENUMERATED WITH ITS REASON in the output, because an escape nobody
    reads is a way to silence a real hit.

      <!-- lint-external-ref: TOKEN -- reason -->
          Declares one exact backticked token as an intentionally unresolvable
          reference for THIS document (a retired name, a doc that lives only in a
          private initiative archive, an engine file outside every scan root).
          Scoped per document, never global.

      <!-- lint-anchor-ignore: reason -->
          Skips every token on the line the marker appears on.

      <!-- lint-anchor-ignore-begin: reason -->  ...  <!-- lint-anchor-ignore-end -->
          Skips every token in the block.

    An escape or declaration that matches NOTHING is itself a violation. A
    pattern that matches nothing passes every check, which is this project's
    signature defect; a stale exemption must not rot silently.

    ------------------------------------------------------------------------
    KNOWN LIMITATIONS (by design - stated, not hidden)
    ------------------------------------------------------------------------
    * It checks that a name EXISTS. It never checks that a sentence is TRUE. A
      resolvable anchor on a false claim passes (`SimulationManager.h:26-37` was
      a live instance on 2026-08-21).
    * It cannot see an omission, a doc contradicting itself, or a move that
      silently edited what it copied.
    * String literals are NOT stripped, only comments. A symbol surviving only
      inside a log format string resolves.
    * A `file:NNN` anchor is checked for range, not for content: `:371` proves
      the file has 371 lines, not that line 371 still says what the doc claims.

.PARAMETER DocPaths
    Markdown files to lint. Defaults to every `.md` in every tier of
    -DefaultDocDirs that exists.

.PARAMETER DefaultDocDirs
    The docs tiers discovered when -DocPaths is not given, one per licence
    subtree. Which tiers were present, empty or absent is PRINTED on every
    defaulted run.

.PARAMETER ScanRoots
    Directories searched for files and symbols. Relative paths resolve against
    -RepoRoot.

.PARAMETER RepoRoot
    Repository root. Defaults to two levels above this script (tools/lint/..).

.PARAMETER PathAlias
    Prefix aliases used inside anchors. `<core>` is expanded by default because
    both perspective docs abbreviate the core path with it; `<brawler>` is its
    counterpart for the BUSL-1.1 game module, so the new tier can abbreviate its
    own paths the same way rather than inventing a second convention.

.PARAMETER ExternalNamespaces
    Qualified-name roots that are out of scope for a tree lint. SKIPPED AND
    ENUMERATED, never silently passed.

.PARAMETER NoBareIdentifiers
    Turn off the IDENT rule (bare lowerCamelCase tokens).

.PARAMETER NoCommentStrip
    Resolve symbols against raw source text. FOR THE NEGATIVE CONTROL ONLY - it
    reintroduces a known false negative on purpose.

.EXAMPLE
    pwsh tools/lint/doc_anchor_lint.ps1

.EXAMPLE
    pwsh tools/lint/doc_anchor_lint.ps1 -DocPaths some/doc.md -ScanRoots Plugins,Source

.OUTPUTS
    Exit 0 = every parsed anchor resolved and every escape was used.
    Exit 1 = at least one unresolved anchor or unused escape.
    Exit 2 = usage / IO error.
#>
[CmdletBinding()]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..' '..')).Path,
    [string[]]$DocPaths,
    # One entry per LICENCE SUBTREE, not per topic: OGBrawlerUnreal is BUSL-1.1
    # and its docs cannot sit in the MPL-2.0 og-simulation tier (decision D9).
    # A tier absent from this list is never looked at and the run still reports
    # CLEAN - silence that reads as success, which is the defect this lint is
    # for. So discovery is the default; a caller must not have to remember.
    [string[]]$DefaultDocDirs = @(
        'Plugins/OGSimulation/Source/OGSimulation/og-simulation/OGSimulation/docs',
        'Source/OGBrawlerUnreal/docs'
    ),
    [string[]]$ScanRoots = @(
        'Plugins',
        'Source',
        'Config',
        'docs',
        'tools'
    ),
    [hashtable]$PathAlias = @{
        '<core>'    = 'Plugins/OGSimulation/Source/OGSimulation/og-simulation/OGSimulation/'
        '<brawler>' = 'Source/OGBrawlerUnreal/'
    },
    [string[]]$ExternalNamespaces = @(
        'std', 'Chaos', 'FMath', 'UE', 'TArray', 'TMap', 'FString', 'FVector', 'FQuat'
    ),
    [switch]$NoBareIdentifiers,
    [switch]$NoCommentStrip,
    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# 0. Resolve inputs
# ---------------------------------------------------------------------------
# Discovery is REPORTED, never assumed. An absent tier and an empty tier both
# contribute zero documents; neither may be indistinguishable from a clean run.
$tierReport = [System.Collections.Generic.List[string]]::new()
if (-not $DocPaths -or $DocPaths.Count -eq 0) {
    $discovered = [System.Collections.Generic.List[string]]::new()
    foreach ($rel in $DefaultDocDirs) {
        $dir = if ([IO.Path]::IsPathRooted($rel)) { $rel } else { Join-Path $RepoRoot $rel }
        if (-not (Test-Path $dir)) { $tierReport.Add(('{0,-7}  {1}' -f 'ABSENT', $rel)); continue }
        $md = @(Get-ChildItem -Path $dir -Filter *.md -File | Sort-Object Name)
        $state = $md.Count -gt 0 ? 'present' : 'EMPTY'
        $tierReport.Add(('{0,-7}  {1}  ({2} md)' -f $state, $rel, $md.Count))
        foreach ($f in $md) { $discovered.Add($f.FullName) }
    }
    if ($discovered.Count -eq 0) {
        Write-Host "ERROR: no default docs tier holds a document."
        foreach ($t in $tierReport) { Write-Host "    $t" }
        exit 2
    }
    $DocPaths = $discovered.ToArray()
}
$resolvedDocs = foreach ($d in $DocPaths) {
    $p = if ([IO.Path]::IsPathRooted($d)) { $d } else { Join-Path $RepoRoot $d }
    if (-not (Test-Path $p)) { Write-Host "ERROR: doc not found: $d"; exit 2 }
    (Resolve-Path $p).Path
}

$resolvedRoots = foreach ($r in $ScanRoots) {
    $p = if ([IO.Path]::IsPathRooted($r)) { $r } else { Join-Path $RepoRoot $r }
    if (Test-Path $p) { (Resolve-Path $p).Path }
}
if (-not $resolvedRoots) { Write-Host 'ERROR: no scan root resolved.'; exit 2 }

# ---------------------------------------------------------------------------
# 1. Index the tree once
#
# Build-output trees are excluded: they hold generated copies of real headers,
# and a stale generated copy would resolve an anchor the live tree no longer
# carries - a false negative of exactly the class this lint exists to catch.
# ---------------------------------------------------------------------------
$excludedDirs = @('Intermediate', 'Binaries', 'DerivedDataCache', 'Saved', '.git', '.vs', 'ThirdParty', 'glm')
$srcFiles = foreach ($r in $resolvedRoots) {
    Get-ChildItem -Path $r -Recurse -File -Include *.h, *.hpp, *.cpp, *.ini, *.cs, *.md -ErrorAction SilentlyContinue |
        Where-Object {
            $parts = $_.FullName.Substring($RepoRoot.Length) -split '[\\/]'
            -not ($parts | Where-Object { $excludedDirs -contains $_ -or $_ -like 'build*' })
        }
}
$srcFiles = $srcFiles | Sort-Object FullName -Unique
if (-not $srcFiles) { Write-Host 'ERROR: scan roots indexed zero files.'; exit 2 }

$byPath = [ordered]@{}
foreach ($f in $srcFiles) { $byPath[($f.FullName -replace '\\', '/')] = $f }

function Remove-Comments {
    param([string]$Text, [string]$Extension)
    $t = [regex]::Replace($Text, '/\*[\s\S]*?\*/', ' ')
    $t = [regex]::Replace($t, '(?m)//.*$', ' ')
    if ($Extension -eq '.ini') { $t = [regex]::Replace($t, '(?m)^\s*;.*$', ' ') }
    return $t
}

# Per-file raw text and comment-stripped code text. Markdown is indexed for path
# resolution but never used as symbol evidence: a doc is not a declaration.
$fileText = @{}
$fileCode = @{}
foreach ($f in $srcFiles) {
    if ($f.Extension -eq '.md') { continue }
    $t = [IO.File]::ReadAllText($f.FullName)
    $fileText[$f.FullName] = $t
    $fileCode[$f.FullName] = Remove-Comments -Text $t -Extension $f.Extension
}
# The text symbol lookups run against. -NoCommentStrip swaps in the raw text to
# demonstrate the false negative; it must never be the default.
$symbolText = if ($NoCommentStrip) { $fileText } else { $fileCode }
$blobCode = ($symbolText.Values) -join "`n"

# ---------------------------------------------------------------------------
# 2. Grammar
# ---------------------------------------------------------------------------
$rxFile = '^(?<p>(?:<[a-z]+>)?[A-Za-z0-9_./\\+-]+\.(?:h|hpp|cpp|ini|cs|md|yml|disabled))(?::(?<l1>\d+)(?:-(?<l2>\d+))?)?$'
$rxQSym = '^[A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)+$'
$rxMember = '^m_[A-Za-z_][A-Za-z0-9_]*$'
# lowerCamelCase with at least one internal capital. `now`/`get`/`today` do not
# match, and that is the point: a single lowercase word is prose.
$rxIdent = '^[a-z][a-z0-9]*[A-Z][A-Za-z0-9]*$'
# PascalCase type / enumerator. Needs a lowercase letter after the first, so a
# one-letter label (`A`) and an all-caps abbreviation (`GT`, `PT`) stay prose.
$rxType = '^[A-Z][A-Za-z0-9]*[a-z][A-Za-z0-9]*$'

function Expand-Alias {
    param([string]$Path)
    foreach ($k in $PathAlias.Keys) {
        if ($Path.StartsWith($k)) { return ($PathAlias[$k] + $Path.Substring($k.Length)) }
    }
    return $Path
}

function Resolve-FilePath {
    param([string]$Path)
    $p = (Expand-Alias -Path $Path) -replace '\\', '/'
    foreach ($k in $byPath.Keys) {
        if ($k -eq $p -or $k.EndsWith('/' + $p)) { return $k }
    }
    return $null
}

function Test-DeclaresType {
    param([string]$Code, [string]$TypeName)
    $n = [regex]::Escape($TypeName)
    # Optional leading `template<...>` (same line or the line above), optional
    # UE-style ALLCAPS export macro, and `enum class` / `enum struct` alongside
    # class/struct/union/namespace. All four shapes are live in this tree.
    $rx = '(?m)^\s*(?:template\s*<[^\n]*>\s*)?(?:template[^\n]*\n\s*)?' +
          '(?:class|struct|union|namespace|enum\s+class|enum\s+struct|enum)\s+' +
          '(?:[A-Z_][A-Z0-9_]*\s+)?' + $n + '\b'
    return [regex]::IsMatch($Code, $rx)
}

function Test-SymbolInCode {
    param([string]$Code, [string]$Symbol)
    # [regex]::IsMatch is CASE-SENSITIVE; PowerShell's -match is not. A symbol
    # anchor that resolved case-insensitively would pass on a different name.
    return [regex]::IsMatch($Code, ('\b' + [regex]::Escape($Symbol) + '\b'))
}

# ---------------------------------------------------------------------------
# 3. Walk the documents
# ---------------------------------------------------------------------------
$violations = [System.Collections.Generic.List[object]]::new()
$checked = 0
$unchecked = 0
$uncheckedDistinct = [ordered]@{}
$externalSkipped = [ordered]@{}
$declarations = [System.Collections.Generic.List[object]]::new()
$escapes = [System.Collections.Generic.List[object]]::new()

function Add-Violation {
    param($Doc, $Line, $Token, $Why)
    $violations.Add([pscustomobject]@{
        Doc = (Split-Path $Doc -Leaf); Line = $Line; Token = $Token; Why = $Why
    })
}

foreach ($doc in $resolvedDocs) {
    $lines = [IO.File]::ReadAllLines($doc)
    $docLeaf = Split-Path $doc -Leaf

    # -- pass 1: collect this document's declarations and block escapes ------
    $declByToken = @{}
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $m = [regex]::Match($lines[$i], '<!--\s*lint-external-ref:\s*(?<tok>.+?)\s*--\s*(?<why>.+?)\s*-->')
        if ($m.Success) {
            $rec = [pscustomobject]@{
                Doc = $docLeaf; Line = $i + 1
                Token = $m.Groups['tok'].Value.Trim('`', ' ')
                Reason = $m.Groups['why'].Value; Hits = 0
            }
            $declarations.Add($rec)
            $declByToken[$rec.Token] = $rec
        }
    }

    $blockStack = [System.Collections.Generic.List[object]]::new()

    # -- pass 2: the anchors --------------------------------------------------
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        $lineNo = $i + 1

        $mb = [regex]::Match($line, '<!--\s*lint-anchor-ignore-begin:\s*(?<why>.+?)\s*-->')
        if ($mb.Success) {
            $rec = [pscustomobject]@{ Doc = $docLeaf; Line = $lineNo; Scope = 'block'; Reason = $mb.Groups['why'].Value; Hits = 0 }
            $escapes.Add($rec); $blockStack.Add($rec); continue
        }
        if ($line -match '<!--\s*lint-anchor-ignore-end\s*-->') {
            if ($blockStack.Count -gt 0) { $blockStack.RemoveAt($blockStack.Count - 1) }
            continue
        }
        if ($line -match '<!--\s*lint-external-ref:') { continue }

        $lineEscape = $null
        $ml = [regex]::Match($line, '<!--\s*lint-anchor-ignore:\s*(?<why>.+?)\s*-->')
        if ($ml.Success) {
            $lineEscape = [pscustomobject]@{ Doc = $docLeaf; Line = $lineNo; Scope = 'line'; Reason = $ml.Groups['why'].Value; Hits = 0 }
            $escapes.Add($lineEscape)
        }
        $activeEscape = if ($lineEscape) { $lineEscape } elseif ($blockStack.Count -gt 0) { $blockStack[$blockStack.Count - 1] } else { $null }

        # ---- find every backticked token with its offset --------------------
        # HTML comments are blanked first (length-preserving, so offsets still
        # line up). An escape's REASON text may itself name the token it excuses;
        # counting that as a suppressed hit would let an escape that suppresses
        # nothing real still look used, defeating the unused-escape check below.
        $line = [regex]::Replace($line, '<!--[\s\S]*?-->', { param($m) ' ' * $m.Value.Length })
        $toks = [regex]::Matches($line, '`([^`\n]+)`')
        if ($toks.Count -eq 0) { continue }
        $consumed = New-Object bool[] $toks.Count
        $pairs = [System.Collections.Generic.List[object]]::new()

        # ---- PAIR spelling 1: `File.h` :: `sym`[, `sym2`] -------------------
        for ($t = 0; $t -lt $toks.Count - 1; $t++) {
            $a = $toks[$t]
            if ($a.Groups[1].Value.Trim() -notmatch $rxFile) { continue }
            $between = $line.Substring($a.Index + $a.Length, $toks[$t + 1].Index - ($a.Index + $a.Length))
            if ($between -notmatch '^\s*::\s*$') { continue }
            $fileTok = $a.Groups[1].Value.Trim()
            $consumed[$t] = $true
            $u = $t + 1
            while ($u -lt $toks.Count) {
                $pairs.Add([pscustomobject]@{ File = $fileTok; Symbol = $toks[$u].Groups[1].Value.Trim() })
                $consumed[$u] = $true
                if ($u + 1 -ge $toks.Count) { break }
                $sep = $line.Substring($toks[$u].Index + $toks[$u].Length, $toks[$u + 1].Index - ($toks[$u].Index + $toks[$u].Length))
                if ($sep -notmatch '^\s*,\s*$') { break }
                $u++
            }
            $t = $u
        }

        # ---- PAIR spelling 2: adjacent markdown table cells -----------------
        if ($line.TrimStart().StartsWith('|')) {
            $cells = $line -split '\|'
            for ($c = 1; $c -lt $cells.Count - 1; $c++) {
                $cell = $cells[$c].Trim()
                if ($cell -notmatch '^`([^`]+)`$') { continue }
                $fileTok = $Matches[1].Trim()
                if ($fileTok -notmatch $rxFile) { continue }
                $next = $cells[$c + 1].Trim()
                if ($next -notmatch '^`([^`]+)`') { continue }
                $symTok = $Matches[1].Trim()
                $pairs.Add([pscustomobject]@{ File = $fileTok; Symbol = $symTok })
                for ($t = 0; $t -lt $toks.Count; $t++) {
                    $v = $toks[$t].Groups[1].Value.Trim()
                    if (-not $consumed[$t] -and ($v -eq $fileTok -or $v -eq $symTok)) { $consumed[$t] = $true }
                }
            }
        }

        # ---- resolve the pairs ----------------------------------------------
        foreach ($p in $pairs) {
            if ($activeEscape) { $activeEscape.Hits++; continue }
            if ($declByToken.ContainsKey($p.File)) { $declByToken[$p.File].Hits++; continue }
            if ($declByToken.ContainsKey($p.Symbol)) { $declByToken[$p.Symbol].Hits++; continue }
            $checked++
            $hit = Resolve-FilePath -Path ($p.File -replace ':\d+(-\d+)?$', '')
            if (-not $hit) {
                Add-Violation $doc $lineNo ("{0} :: {1}" -f $p.File, $p.Symbol) 'PAIR: file does not resolve under any scan root'
                continue
            }
            $full = $byPath[$hit].FullName
            $code = $symbolText[$full]
            if ($null -eq $code) {
                Add-Violation $doc $lineNo ("{0} :: {1}" -f $p.File, $p.Symbol) 'PAIR: target is not a source file, so it cannot carry a symbol'
                continue
            }
            # `Class::member` in a pair cell resolves on its last segment: this
            # tree declares members unqualified inside header-only classes.
            $leaf = ($p.Symbol -split '::')[-1]
            if (-not (Test-SymbolInCode -Code $code -Symbol $leaf)) {
                Add-Violation $doc $lineNo ("{0} :: {1}" -f $p.File, $p.Symbol) 'PAIR: symbol absent from that file, or present only in comments'
            }
        }

        # ---- the remaining single tokens ------------------------------------
        for ($t = 0; $t -lt $toks.Count; $t++) {
            if ($consumed[$t]) { continue }
            $tok = $toks[$t].Groups[1].Value.Trim()

            if ($activeEscape) { $activeEscape.Hits++; continue }
            if ($declByToken.ContainsKey($tok)) { $declByToken[$tok].Hits++; continue }

            if ($tok -match $rxFile) {
                $p = $Matches['p']; $l1raw = $Matches['l1']; $l2raw = $Matches['l2']
                if ($declByToken.ContainsKey($p)) { $declByToken[$p].Hits++; continue }
                $checked++
                $hit = Resolve-FilePath -Path $p
                if (-not $hit) {
                    Add-Violation $doc $lineNo $tok 'FILE does not resolve under any scan root'
                }
                elseif ($l1raw) {
                    $n = [IO.File]::ReadAllLines($byPath[$hit].FullName).Count
                    $l1 = [int]$l1raw
                    $l2 = if ($l2raw) { [int]$l2raw } else { $l1 }
                    if ($l1 -lt 1 -or $l2 -gt $n -or $l2 -lt $l1) {
                        Add-Violation $doc $lineNo $tok "LINE out of range (target file has $n lines)"
                    }
                }
                continue
            }

            if ($tok -match $rxQSym) {
                $seg = $tok -split '::'
                $cls = $seg[0]; $mem = $seg[-1]
                if ($ExternalNamespaces -contains $cls) { $externalSkipped[$tok] = $true; continue }
                $checked++
                $ok = $false
                # (a) a file declares the type AND mentions the member in code
                foreach ($kv in $symbolText.GetEnumerator()) {
                    if ((Test-DeclaresType -Code $kv.Value -TypeName $cls) -and (Test-SymbolInCode -Code $kv.Value -Symbol $mem)) { $ok = $true; break }
                }
                # (b) or some file carries the qualified name itself in code
                #     (out-of-line definitions, split .h/.cpp, namespaced free
                #     functions - all live shapes in this tree)
                if (-not $ok -and [regex]::IsMatch($blobCode, ('\b' + [regex]::Escape($tok) + '\b'))) { $ok = $true }
                if (-not $ok) {
                    Add-Violation $doc $lineNo $tok 'QUALIFIED SYMBOL: no file declares that type and carries that member in code'
                }
                continue
            }

            if ($tok -cmatch $rxMember) {
                $checked++
                if (-not (Test-SymbolInCode -Code $blobCode -Symbol $tok)) {
                    Add-Violation $doc $lineNo $tok 'MEMBER not found in non-comment code under any scan root'
                }
                continue
            }

            # -cmatch, not -match: `Diagnostic` must NOT read as lowerCamelCase.
            if (-not $NoBareIdentifiers -and $tok -cmatch $rxIdent) {
                $checked++
                if (-not (Test-SymbolInCode -Code $blobCode -Symbol $tok)) {
                    Add-Violation $doc $lineNo $tok 'IDENTIFIER not found in non-comment code under any scan root'
                }
                continue
            }

            # TYPE: `StateCorrectionCache`, `ResimSweepDiagnostics`, `Written`.
            # PascalCase with at least one lowercase after the first letter, so
            # `A`, `GT`, `PT` and other ALLCAPS/one-letter labels stay prose.
            if (-not $NoBareIdentifiers -and $tok -cmatch $rxType) {
                $checked++
                if (-not (Test-SymbolInCode -Code $blobCode -Symbol $tok)) {
                    Add-Violation $doc $lineNo $tok 'TYPE/ENUMERATOR not found in non-comment code under any scan root'
                }
                continue
            }

            # SNIPPET: a backticked code fragment (`if (!withinDepth) { ++x; }`)
            # is not one anchor, but the identifiers inside it are claims all the
            # same - and this is exactly where a name silently drifts, because no
            # whole-token rule ever looks inside. Measured live: a shipped doc
            # wrote `++deepAnchorSkips` where the code says
            # `++diagnosticDeepAnchorSkips`, invisible to every whole-token rule.
            if (-not $NoBareIdentifiers -and $tok -match '[;{}()=]|\s') {
                $inner = [regex]::Matches($tok, '(?-i)\b(?:m_[A-Za-z_][A-Za-z0-9_]*|[a-z][a-z0-9]*[A-Z][A-Za-z0-9]*)\b')
                if ($inner.Count -gt 0) {
                    $seenInner = @{}
                    foreach ($im in $inner) {
                        $id = $im.Value
                        if ($seenInner.ContainsKey($id)) { continue }
                        $seenInner[$id] = $true
                        if ($declByToken.ContainsKey($id)) { $declByToken[$id].Hits++; continue }
                        $checked++
                        if (-not (Test-SymbolInCode -Code $blobCode -Symbol $id)) {
                            Add-Violation $doc $lineNo $id "IDENTIFIER not found in non-comment code (inside snippet ``$tok``)"
                        }
                    }
                    continue
                }
            }

            $unchecked++
            if (-not $uncheckedDistinct.Contains($tok)) { $uncheckedDistinct[$tok] = 0 }
            $uncheckedDistinct[$tok] = $uncheckedDistinct[$tok] + 1
        }
    }
}

# ---------------------------------------------------------------------------
# 4. An exemption that matches nothing is a defect, not a pass
# ---------------------------------------------------------------------------
foreach ($d in $declarations) {
    if ($d.Hits -eq 0) {
        $violations.Add([pscustomobject]@{
            Doc = $d.Doc; Line = $d.Line; Token = $d.Token
            Why = 'UNUSED lint-external-ref declaration - it matches no token in this document'
        })
    }
}
foreach ($e in $escapes) {
    if ($e.Hits -eq 0) {
        $violations.Add([pscustomobject]@{
            Doc = $e.Doc; Line = $e.Line; Token = ('({0} escape)' -f $e.Scope)
            Why = 'UNUSED lint-anchor-ignore - it suppresses no token'
        })
    }
}

# ---------------------------------------------------------------------------
# 5. Report. The UNCHECKED count is ALWAYS printed - a lint that hides what it
#    could not parse is the defect this project keeps shipping.
# ---------------------------------------------------------------------------
if (-not $Quiet) {
    if ($tierReport.Count -gt 0) {
        Write-Host "default doc tiers     : $($tierReport.Count)"
        foreach ($t in $tierReport) { Write-Host "    $t" }
    }
    Write-Host "docs linted           : $($resolvedDocs.Count)"
    Write-Host "source files indexed  : $($srcFiles.Count)"
    Write-Host "anchors CHECKED       : $checked"
    Write-Host "tokens UNCHECKED      : $unchecked (distinct: $($uncheckedDistinct.Count)) - not parseable as an anchor"
    Write-Host "external ns SKIPPED   : $($externalSkipped.Count) -> $((($externalSkipped.Keys | Sort-Object) -join ', '))"
    Write-Host "external refs DECLARED: $($declarations.Count)"
    foreach ($d in ($declarations | Sort-Object Doc, Line)) {
        Write-Host ('    {0}:{1}  {2}  x{3}  -- {4}' -f $d.Doc, $d.Line, $d.Token, $d.Hits, $d.Reason)
    }
    Write-Host "escapes HONOURED      : $($escapes.Count)"
    foreach ($e in ($escapes | Sort-Object Doc, Line)) {
        Write-Host ('    {0}:{1}  ({2})  x{3}  -- {4}' -f $e.Doc, $e.Line, $e.Scope, $e.Hits, $e.Reason)
    }
    if ($NoCommentStrip) { Write-Host 'WARNING: -NoCommentStrip is on. Symbol resolution is knowingly unsound.' }
}

if ($violations.Count -eq 0) {
    if (-not $Quiet) { Write-Host 'RESULT: CLEAN - every parsed anchor resolves and every escape is used.' }
    exit 0
}
Write-Host "RESULT: $($violations.Count) UNRESOLVED ANCHOR(S)"
foreach ($v in ($violations | Sort-Object Doc, Line)) {
    Write-Host ('  {0}:{1}   {2}   -- {3}' -f $v.Doc, $v.Line, $v.Token, $v.Why)
}
exit 1
