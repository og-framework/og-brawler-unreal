# SPDX-License-Identifier: MPL-2.0
#Requires -Modules Pester
<#
    Pester spec for tools/lint/doc_anchor_lint.ps1.

    WHY THIS FILE EXISTS. The lint's whole value is that it can distinguish a
    document that is true from one that is not. That ability must be a TEST, not
    a claim - this project's signature defect is a check that cannot tell success
    from failure, and the task-5 prototype of this very lint shipped exactly that
    defect (it "passed" a dead symbol that survived only in comments).

    So every Context below pairs a MUST-BE-SILENT case with a MUST-FIRE case over
    the same fixture shape. The known-bad fixture seeds four real historical
    defects from the og-netcode-v2-input-relay initiative:

      row 1  dead owner, surviving only in comments        - item 91-J1
      row 2  file renamed away                             - items 72 / 76
      row 3  member renamed away                           - item 72-A
      row 4  positional anchor past end of file            - item 88's class

    Row 5 is TRUE and identical in shape, so an indiscriminate checker is visible.

    Fixtures are built in a throwaway directory, per the house pattern in
    tools/og-tools/tests/*.Tests.ps1. Nothing here reads the real repository, so
    the spec cannot go green or red because the tree moved.

    Run:  pwsh -NoProfile -Command "Invoke-Pester tools/lint/tests -Output Detailed"
#>

BeforeAll {
    $script:Lint = Join-Path (Split-Path -Parent $PSScriptRoot) 'doc_anchor_lint.ps1'
    if (-not (Test-Path $script:Lint)) { throw "lint not found at $script:Lint" }

    # The two default docs tiers, spelled exactly as the script's -DefaultDocDirs
    # spells them. They are SEPARATE because of licence, not topic: og-simulation
    # is MPL-2.0 and travels with that submodule, OGBrawlerUnreal is BUSL-1.1
    # (decision D9, 2026-08-23). If the script's list and these strings drift
    # apart the discovery Context below stops finding anything and goes red.
    $script:CoreTier    = 'Plugins/OGSimulation/Source/OGSimulation/og-simulation/OGSimulation/docs'
    $script:BrawlerTier = 'Source/OGBrawlerUnreal/docs'

    # Runs the lint against a fixture root. Returns the captured output and the
    # exit code. Write-Host goes to the information stream, hence 6>&1.
    function Invoke-Lint {
        param([string]$Root, [string[]]$Docs, [hashtable]$Extra = @{})
        $args = @{
            RepoRoot  = $Root
            DocPaths  = $Docs
            ScanRoots = @('src', 'cfg')
        }
        foreach ($k in $Extra.Keys) { $args[$k] = $Extra[$k] }
        $out = & $script:Lint @args 6>&1 | Out-String
        return [pscustomobject]@{ Out = $out; Code = $LASTEXITCODE }
    }

    function New-FixtureRoot {
        $dir = Join-Path ([IO.Path]::GetTempPath()) ('doclint-' + [Guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $dir | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $dir 'src') | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $dir 'cfg') | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $dir 'doc') | Out-Null

        # The two DEFAULT docs tiers, at their real relative paths, so the
        # discovery Context exercises the list the script actually ships rather
        # than a list the test hands it. Still nothing from the real repository.
        New-Item -ItemType Directory -Force -Path (Join-Path $dir $script:CoreTier) | Out-Null
        New-Item -ItemType Directory -Force -Path (Join-Path $dir $script:BrawlerTier) | Out-Null

        # A game-module header, so `<brawler>` has something real to expand onto.
        $brawlerH = @'
#pragma once
class ASimulationManagerUImpl
{
public:
    void onTimingInfoReceived();
};
'@
        Set-Content -LiteralPath (Join-Path $dir 'Source/OGBrawlerUnreal/ManagerUImpl.h') -Value $brawlerH

        # -- a header-only class, the shape this codebase actually uses --------
        # `retiredHelperName` appears ONCE, IN A COMMENT. That is the whole point
        # of the comment-stripping Context below.
        $cache = @'
#pragma once
namespace ogfix {

// HISTORY: pushPredictionTick used to call retiredHelperName before item 45
// retired it. The name survives here and nowhere else.
class StateCorrectionCache
{
public:
    void pushPredictionTick(unsigned int tick);
    unsigned int m_pendingResimAnchorTick = 0u;
};

enum class StepKind { Normal, Stall, HardResync };

}  // namespace ogfix
'@
        Set-Content -LiteralPath (Join-Path $dir 'src/CorrectionCache.h') -Value $cache

        # -- a UE-style exported class whose member is defined in the .cpp -----
        $splitH = @'
#pragma once
class OGFIX_API RelayActor
{
public:
    virtual void PreReplication();
};

class SplitOwner
{
public:
    void declaredHere();
};
'@
        Set-Content -LiteralPath (Join-Path $dir 'src/RelayActor.h') -Value $splitH

        $splitCpp = @'
#include "RelayActor.h"

void RelayActor::PreReplication() {}

// Defined out of line, and NOT declared in the header above.
void SplitOwner::definedOnlyInCpp() {}
'@
        Set-Content -LiteralPath (Join-Path $dir 'src/RelayActor.cpp') -Value $splitCpp

        # -- an ini whose ';' comments must be stripped ------------------------
        $ini = @'
[OGFixture]
; RetiredKey=1 was removed at item 62 and lives on only in this comment.
LiveKey=7
'@
        Set-Content -LiteralPath (Join-Path $dir 'cfg/fixture.ini') -Value $ini

        return $dir
    }

    # Five rows, identical in shape. Every anchor resolves.
    $script:TrueDoc = @'
<!-- SPDX-License-Identifier: MPL-2.0 -->
# True fixture

1. `StateCorrectionCache::pushPredictionTick`
2. `src/RelayActor.h`
3. `m_pendingResimAnchorTick`
4. `src/CorrectionCache.h:5`
5. `src/CorrectionCache.h` :: `pushPredictionTick`
'@

    # The same five rows. Rows 1-4 each seed one real historical defect; row 5 is
    # untouched and true, so a checker that fires on everything is visible.
    $script:BadDoc = @'
<!-- SPDX-License-Identifier: MPL-2.0 -->
# Known-bad fixture

1. `StateCorrectionCache::retiredHelperName`
2. `src/RemovedFile.h`
3. `m_retiredStore`
4. `src/CorrectionCache.h:9999`
5. `src/CorrectionCache.h` :: `pushPredictionTick`
'@
}

Describe 'doc_anchor_lint' {

    Context 'The negative control - it must distinguish the two inputs' {

        It 'is SILENT on the true fixture and exits 0' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/NC_true.md') -Value $script:TrueDoc
                $r = Invoke-Lint -Root $root -Docs @('doc/NC_true.md')
                $r.Out  | Should -Match 'RESULT: CLEAN'
                $r.Code | Should -Be 0
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'FIRES on all four seeded defects, and leaves the true row alone' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/NC_bad.md') -Value $script:BadDoc
                $r = Invoke-Lint -Root $root -Docs @('doc/NC_bad.md')
                $r.Code | Should -Be 1
                $r.Out  | Should -Match 'RESULT: 4 UNRESOLVED ANCHOR'
                $r.Out  | Should -Match 'retiredHelperName'      # item 91-J1
                $r.Out  | Should -Match 'RemovedFile\.h'         # items 72 / 76
                $r.Out  | Should -Match 'm_retiredStore'         # item 72-A
                $r.Out  | Should -Match 'LINE out of range'      # item 88's class
                # Row 5 is true and must NOT appear as a violation. It is named in
                # the header banner, so assert on the violation line shape only.
                $r.Out  | Should -Not -Match 'CorrectionCache\.h :: pushPredictionTick\s+--'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'Comment-stripping is load-bearing (requirement 1)' {

        It 'lets the dead owner silently pass when -NoCommentStrip is on' {
            # This is the prototype's shipped false negative, pinned as a test so
            # nobody can quietly remove the strip and still see a green run.
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/NC_bad.md') -Value $script:BadDoc
                $r = Invoke-Lint -Root $root -Docs @('doc/NC_bad.md') -Extra @{ NoCommentStrip = $true }
                $r.Code | Should -Be 1
                $r.Out  | Should -Match 'RESULT: 3 UNRESOLVED ANCHOR'
                $r.Out  | Should -Not -Match 'retiredHelperName'
                $r.Out  | Should -Match 'WARNING: -NoCommentStrip'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'strips ; comments in .ini, so a commented-out key does not resolve' {
            $root = New-FixtureRoot
            try {
                $doc = "# ini`n`nlive: ``LiveKey`` dead: ``RetiredKey``"
                Set-Content -LiteralPath (Join-Path $root 'doc/ini.md') -Value $doc
                $r = Invoke-Lint -Root $root -Docs @('doc/ini.md')
                $r.Code | Should -Be 1
                $r.Out  | Should -Match 'RetiredKey'
                $r.Out  | Should -Not -Match 'LiveKey\s+--'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'Scoped member resolution, not a whole-tree Contains (requirement 2)' {

        It 'resolves a member declared unqualified inside its own class' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/q.md') -Value "``StateCorrectionCache::pushPredictionTick``"
                (Invoke-Lint -Root $root -Docs @('doc/q.md')).Code | Should -Be 0
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'rejects a member that exists only on a DIFFERENT class' {
            # The whole-tree Contains() bug in reverse: `pushPredictionTick` is a
            # real name, but not on this type, and the lint must say so.
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/q.md') -Value "``RelayActor::pushPredictionTick``"
                $r = Invoke-Lint -Root $root -Docs @('doc/q.md')
                $r.Code | Should -Be 1
                $r.Out  | Should -Match 'QUALIFIED SYMBOL'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'sees through a UE-style export macro in the class declaration' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/q.md') -Value "``RelayActor::PreReplication``"
                (Invoke-Lint -Root $root -Docs @('doc/q.md')).Code | Should -Be 0
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'resolves an enum class enumerator' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/q.md') -Value "``StepKind::HardResync``"
                (Invoke-Lint -Root $root -Docs @('doc/q.md')).Code | Should -Be 0
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'resolves a member whose only definition is in a sibling .cpp' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/q.md') -Value "``SplitOwner::definedOnlyInCpp``"
                (Invoke-Lint -Root $root -Docs @('doc/q.md')).Code | Should -Be 0
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'matches symbols CASE-SENSITIVELY' {
            # PowerShell's -match is case-insensitive by default; if that leaks
            # into symbol resolution, a doc naming the wrong casing passes.
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/q.md') -Value "``StateCorrectionCache::PUSHPREDICTIONTICK``"
                (Invoke-Lint -Root $root -Docs @('doc/q.md')).Code | Should -Be 1
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'The UNCHECKED count is always printed (requirement 3)' {

        It 'prints the count on a clean run' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/NC_true.md') -Value $script:TrueDoc
                $r = Invoke-Lint -Root $root -Docs @('doc/NC_true.md')
                $r.Out | Should -Match 'tokens UNCHECKED\s+:'
                $r.Code | Should -Be 0
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'counts a token it cannot parse instead of ignoring it' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/u.md') -Value "prose with ``a b c d`` in it"
                $r = Invoke-Lint -Root $root -Docs @('doc/u.md')
                $r.Out  | Should -Match 'tokens UNCHECKED\s+: 1 \(distinct: 1\)'
                $r.Code | Should -Be 0
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'External namespaces are skipped AND enumerated (requirement 4)' {

        It 'does not flag std:: and names it in the skipped list' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/e.md') -Value "``std::atomic``"
                $r = Invoke-Lint -Root $root -Docs @('doc/e.md')
                $r.Code | Should -Be 0
                $r.Out  | Should -Match 'external ns SKIPPED\s+: 1 -> std::atomic'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'still flags a qualified name that is NOT declared external' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/e.md') -Value "``Nowhere::atAll``"
                (Invoke-Lint -Root $root -Docs @('doc/e.md')).Code | Should -Be 1
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'Escapes are honoured, enumerated, and cannot rot silently' {

        It 'suppresses a declared external reference and prints its reason' {
            $root = New-FixtureRoot
            try {
                $doc = "<!-- lint-external-ref: Archive.md -- private initiative material -->`n`nsee ``Archive.md``"
                Set-Content -LiteralPath (Join-Path $root 'doc/x.md') -Value $doc
                $r = Invoke-Lint -Root $root -Docs @('doc/x.md')
                $r.Code | Should -Be 0
                $r.Out  | Should -Match 'Archive\.md  x1  -- private initiative material'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'FAILS on a declaration that suppresses nothing' {
            # A pattern that matches nothing passes every check. An exemption for
            # a name the document no longer mentions must not survive quietly.
            $root = New-FixtureRoot
            try {
                $doc = "<!-- lint-external-ref: Vanished.md -- stale exemption -->`n`nno mention here"
                Set-Content -LiteralPath (Join-Path $root 'doc/x.md') -Value $doc
                $r = Invoke-Lint -Root $root -Docs @('doc/x.md')
                $r.Code | Should -Be 1
                $r.Out  | Should -Match 'UNUSED lint-external-ref'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'suppresses a line escape but not the line after it' {
            $root = New-FixtureRoot
            try {
                $doc = "bad ``m_goneForever`` <!-- lint-anchor-ignore: retired on purpose -->`nalso bad ``m_alsoGone``"
                Set-Content -LiteralPath (Join-Path $root 'doc/x.md') -Value $doc
                $r = Invoke-Lint -Root $root -Docs @('doc/x.md')
                $r.Code | Should -Be 1
                $r.Out  | Should -Not -Match 'm_goneForever'
                $r.Out  | Should -Match 'm_alsoGone'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'FAILS on a line escape that suppresses nothing' {
            $root = New-FixtureRoot
            try {
                $doc = "plain prose <!-- lint-anchor-ignore: excuses nothing -->"
                Set-Content -LiteralPath (Join-Path $root 'doc/x.md') -Value $doc
                $r = Invoke-Lint -Root $root -Docs @('doc/x.md')
                $r.Code | Should -Be 1
                $r.Out  | Should -Match 'UNUSED lint-anchor-ignore'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'does not let an escape be justified by its own reason text' {
            # The reason may name the very token it excuses. If that counted as a
            # suppressed hit, the unused-escape check above would never fire.
            $root = New-FixtureRoot
            try {
                $doc = "plain prose <!-- lint-anchor-ignore: about ``m_something`` -->"
                Set-Content -LiteralPath (Join-Path $root 'doc/x.md') -Value $doc
                (Invoke-Lint -Root $root -Docs @('doc/x.md')).Out | Should -Match 'UNUSED lint-anchor-ignore'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'The file :: symbol PAIR form, in both spellings' {

        It 'resolves the :: spelling and rejects the same symbol against the wrong file' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/p.md') -Value "``src/CorrectionCache.h`` :: ``pushPredictionTick``"
                (Invoke-Lint -Root $root -Docs @('doc/p.md')).Code | Should -Be 0

                Set-Content -LiteralPath (Join-Path $root 'doc/p.md') -Value "``src/RelayActor.h`` :: ``pushPredictionTick``"
                $r = Invoke-Lint -Root $root -Docs @('doc/p.md')
                $r.Code | Should -Be 1
                $r.Out  | Should -Match 'PAIR: symbol absent from that file'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'resolves the adjacent-table-cell spelling and rejects a wrong pairing' {
            $root = New-FixtureRoot
            try {
                $good = "| # | file | symbol |`n|---|---|---|`n| 1 | ``src/CorrectionCache.h`` | ``pushPredictionTick`` |"
                Set-Content -LiteralPath (Join-Path $root 'doc/p.md') -Value $good
                (Invoke-Lint -Root $root -Docs @('doc/p.md')).Code | Should -Be 0

                $bad = "| # | file | symbol |`n|---|---|---|`n| 1 | ``src/RelayActor.h`` | ``pushPredictionTick`` |"
                Set-Content -LiteralPath (Join-Path $root 'doc/p.md') -Value $bad
                (Invoke-Lint -Root $root -Docs @('doc/p.md')).Code | Should -Be 1
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'checks every symbol in a comma-separated pair list' {
            $root = New-FixtureRoot
            try {
                $doc = "``src/CorrectionCache.h`` :: ``pushPredictionTick``, ``m_pendingResimAnchorTick``"
                Set-Content -LiteralPath (Join-Path $root 'doc/p.md') -Value $doc
                (Invoke-Lint -Root $root -Docs @('doc/p.md')).Code | Should -Be 0

                $doc = "``src/CorrectionCache.h`` :: ``pushPredictionTick``, ``m_neverExisted``"
                Set-Content -LiteralPath (Join-Path $root 'doc/p.md') -Value $doc
                (Invoke-Lint -Root $root -Docs @('doc/p.md')).Code | Should -Be 1
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'Identifiers inside a backticked code snippet' {

        It 'flags a drifted identifier that no whole-token rule would ever see' {
            # The live instance: a shipped doc wrote `++deepAnchorSkips` where the
            # code says `++diagnosticDeepAnchorSkips`. As one token the snippet is
            # unparseable, so without this rule it counts as UNCHECKED and passes.
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/s.md') -Value "the call is ``if (x) { ++driftedName; }``"
                $r = Invoke-Lint -Root $root -Docs @('doc/s.md')
                $r.Code | Should -Be 1
                $r.Out  | Should -Match 'driftedName'
                $r.Out  | Should -Match 'inside snippet'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'stays silent when every identifier in the snippet resolves' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/s.md') -Value "the call is ``cache.pushPredictionTick(tick)``"
                (Invoke-Lint -Root $root -Docs @('doc/s.md')).Code | Should -Be 0
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'is disabled by -NoBareIdentifiers' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root 'doc/s.md') -Value "the call is ``if (x) { ++driftedName; }``"
                $r = Invoke-Lint -Root $root -Docs @('doc/s.md') -Extra @{ NoBareIdentifiers = $true }
                $r.Code | Should -Be 0
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'Default doc-tier discovery - BOTH licence tiers (decision D9)' {

        # THE REGRESSION THESE PIN. Before this fix the default doc set was one
        # hard-coded path, so a document under Source/OGBrawlerUnreal/docs was
        # never opened: not skipped with a warning, not counted as UNCHECKED -
        # simply never looked at, while the run printed CLEAN and exited 0. The
        # brawler tier was measured in that state on 2026-08-23 (12 docs linted,
        # exit 0) with a knowingly broken anchor sitting inside it.

        It 'discovers a doc in EACH tier when -DocPaths is not given' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root $script:CoreTier 'core.md') -Value "``m_pendingResimAnchorTick``"
                Set-Content -LiteralPath (Join-Path $root $script:BrawlerTier 'game.md') -Value "``m_pendingResimAnchorTick``"
                $r = Invoke-Lint -Root $root
                $r.Code | Should -Be 0
                $r.Out  | Should -Match 'docs linted\s+: 2'
                $r.Out  | Should -Match 'default doc tiers\s+: 2'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'FIRES on a broken anchor in the BRAWLER tier under default discovery' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root $script:CoreTier 'core.md') -Value "``m_pendingResimAnchorTick``"
                Set-Content -LiteralPath (Join-Path $root $script:BrawlerTier 'game.md') -Value "``m_neverExistedAnywhere``"
                $r = Invoke-Lint -Root $root
                $r.Code | Should -Be 1
                $r.Out  | Should -Match 'RESULT: 1 UNRESOLVED ANCHOR'
                $r.Out  | Should -Match 'm_neverExistedAnywhere'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'reports an ABSENT tier and still lints the one that is present' {
            # The brawler tier does not exist until its first document is written.
            # An absent tier must not be an error, and must not be invisible.
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root $script:CoreTier 'core.md') -Value "``m_pendingResimAnchorTick``"
                Remove-Item -LiteralPath (Join-Path $root $script:BrawlerTier) -Recurse -Force
                $r = Invoke-Lint -Root $root
                $r.Code | Should -Be 0
                $r.Out  | Should -Match 'ABSENT\s+Source/OGBrawlerUnreal/docs'
                $r.Out  | Should -Match 'docs linted\s+: 1'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'reports an EMPTY tier rather than passing over it in silence' {
            $root = New-FixtureRoot
            try {
                Set-Content -LiteralPath (Join-Path $root $script:CoreTier 'core.md') -Value "``m_pendingResimAnchorTick``"
                $r = Invoke-Lint -Root $root
                $r.Code | Should -Be 0
                $r.Out  | Should -Match 'EMPTY\s+Source/OGBrawlerUnreal/docs\s+\(0 md\)'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'expands the <brawler> alias, and still fires on a path under it that does not exist' {
            # An alias that resolved everything would be worse than none: it would
            # make every brawler-tier path anchor unfalsifiable.
            $root = New-FixtureRoot
            try {
                $doc = "good ``<brawler>ManagerUImpl.h```nbad ``<brawler>NotHere.h``"
                Set-Content -LiteralPath (Join-Path $root 'doc/a.md') -Value $doc
                $r = Invoke-Lint -Root $root -Docs @('doc/a.md') -Extra @{ ScanRoots = @('src', 'cfg', 'Source') }
                $r.Code | Should -Be 1
                $r.Out  | Should -Match 'RESULT: 1 UNRESOLVED ANCHOR'
                $r.Out  | Should -Match '<brawler>NotHere\.h'
                $r.Out  | Should -Not -Match '<brawler>ManagerUImpl\.h\s+--'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }

    Context 'Usage errors are exit 2, not a silent pass' {

        It 'exits 2 when NO default tier holds a document' {
            $root = New-FixtureRoot
            try {
                $r = Invoke-Lint -Root $root
                $r.Code | Should -Be 2
                $r.Out  | Should -Match 'no default docs tier holds a document'
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }

        It 'exits 2 when a named document does not exist' {
            $root = New-FixtureRoot
            try {
                $r = Invoke-Lint -Root $root -Docs @('doc/not_here.md')
                $r.Code | Should -Be 2
            } finally { Remove-Item -LiteralPath $root -Recurse -Force }
        }
    }
}
