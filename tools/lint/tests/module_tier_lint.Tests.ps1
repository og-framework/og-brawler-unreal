# SPDX-License-Identifier: MPL-2.0
#Requires -Modules Pester
# tools/lint/README.md "Module tier lint"

BeforeAll {
    $script:Lint = Join-Path (Split-Path -Parent $PSScriptRoot) 'module_tier_lint.ps1'
    $script:Table = Join-Path (Split-Path -Parent $PSScriptRoot) 'module_tiers.psd1'
    $script:Fx = Join-Path $PSScriptRoot 'fixtures' 'module_tier'
    if (-not (Test-Path $script:Lint)) { throw "lint not found at $script:Lint" }
    $script:Pwsh = (Get-Process -Id $PID).Path

    function script:Invoke-Lint {
        param([string]$ArgText)
        $out = & $script:Pwsh -NoProfile -Command "& '$script:Lint' $ArgText; exit `$LASTEXITCODE" 2>&1
        $text = ($out -join "`n")
        $counts = @{}
        foreach ($m in [regex]::Matches($text, '(?m)^  (A1|A2|A3|A4|R1|R2) [^:\n]+: (\d+)')) { $counts[$m.Groups[1].Value] = [int]$m.Groups[2].Value }
        return [pscustomobject]@{ Exit = $LASTEXITCODE; Text = $text; Counts = $counts }
    }

    function script:Get-BundleArg {
        param([string[]]$Names)
        return (($Names | ForEach-Object { "'" + (Join-Path $script:Fx "$_.tree.txt") + "'" }) -join ',')
    }

    function script:New-Tree {
        param([string[]]$Names)
        $dir = Join-Path $TestDrive ([guid]::NewGuid().ToString('N'))
        $r = Invoke-Lint "-FixturesPath $(Get-BundleArg $Names) -ExpandTo '$dir'"
        if ($r.Exit -ne 0) { throw "expanding $($Names -join '+') failed: $($r.Text)" }
        return $dir
    }

    function script:Invoke-OnTree {
        param([string[]]$Names, [string]$Extra = '')
        $dir = New-Tree $Names
        return Invoke-Lint "-RepoRoot '$dir' $Extra"
    }

    function script:New-TableVariant {
        param([string]$Old, [string]$New)
        $text = [IO.File]::ReadAllText($script:Table)
        if (([regex]::Matches($text, [regex]::Escape($Old))).Count -ne 1) { throw "table anchor not found exactly once: $Old" }
        $p = Join-Path $TestDrive ([guid]::NewGuid().ToString('N') + '.psd1')
        [IO.File]::WriteAllText($p, $text.Replace($Old, $New))
        return $p
    }
}

Describe 'module_tier_lint: the clean fixture' {
    BeforeAll { $script:Clean = Invoke-OnTree @('clean') }

    It 'exits 0 with every arm at zero' {
        $script:Clean.Exit | Should -Be 0
        foreach ($a in 'A1', 'A2', 'A3', 'A4', 'R1', 'R2') { $script:Clean.Counts[$a] | Should -Be 0 -Because "$a on the clean fixture" }
        $script:Clean.Text | Should -Match 'RESULT: CLEAN'
    }

    It 'finds every node of the fixture by its root globs, the session included' {
        foreach ($n in 'OGNet', 'OGSimulation', 'OGTickDriver', 'OGSimNet', 'OGSimSession', 'OGBrawler', 'OGSimulationUnreal', 'OGBrawlerUnreal') {
            $script:Clean.Text | Should -Match "(?m)^  $n +tier \d +\S+ +present"
        }
        $script:Clean.Text | Should -Match '(?m)^  OGNetUnreal +tier 4 +engine +absent'
    }

    It 'classes og-tickdriver by name inside og-simulation, and the two vocabulary headers as og-sim' {
        $r = Invoke-Lint "-RepoRoot '$(New-Tree @('clean'))' -ListFiles"
        $r.Text | Should -Match '(?m)^OGTickDriver\t\S+/TickDriverConcepts\.h$'
        $r.Text | Should -Match '(?m)^OGTickDriver\t\S+/SimulationTickDriver\.h$'
        $r.Text | Should -Match '(?m)^OGSimulation\t\S+/TickDriverGameTypes\.h$'
        $r.Text | Should -Match '(?m)^OGSimulation\t\S+/SimulationScheduler\.h$'
        $r.Text | Should -Match '(?m)^\(unlinted\)\tSource/OGSimulationTests/SomeSuiteTest\.cpp$'
        $r.Text | Should -Not -Match 'og-net/tests/|og-sim-net/tests/|/glm/glm\.hpp'
    }

    It 'lists the session headers that include OGSimNet (R1 watch) without counting them' {
        $script:Clean.Text | Should -Match '\[ModuleTier\.R1\.Watch\] OGSimSession -> OGSimNet: \S+/OGSimSession/SessionTypes\.h -> OGSimNet/SimNetIdle\.h'
    }
}

Describe 'module_tier_lint: each arm fires on its own fixture and leaves the others at zero' {
    It '<Arm> fires on <Bundle>' -ForEach @(
        @{ Arm = 'A1'; Bundle = 'a1'; Exit = 1; Line = 'SimulationLogRoutes\.h:3: \[ModuleTier\.A1\] OGSimulation \(tier 0\) includes "OGSimulation/TickDriverConcepts\.h" of OGTickDriver \(tier 1\)' }
        @{ Arm = 'A1'; Bundle = 'a1_bare'; Exit = 1; Line = 'SimulationLogRoutes\.h:3: \[ModuleTier\.A1\] OGSimulation \(tier 0\) includes "TickDriverConcepts\.h" of OGTickDriver' }
        @{ Arm = 'A1'; Bundle = 'a1_relative'; Exit = 1; Line = 'ClockDilationController\.h:4: \[ModuleTier\.A1\] OGSimulation \(tier 0\) includes "\.\./TickDriverConcepts\.h" of OGTickDriver' }
        @{ Arm = 'A2'; Bundle = 'a2'; Exit = 1; Line = 'SimNetClient\.h:2: \[ModuleTier\.A2\] OGSimNet is engine-free and includes the engine header "CoreMinimal\.h"' }
        @{ Arm = 'A3'; Bundle = 'a3'; Exit = 1; Line = 'Source/NewGameModule: \[ModuleTier\.A3\]' }
        @{ Arm = 'A4'; Bundle = 'a4'; Exit = 1; Line = 'SimNetIdle\.h:3: \[ModuleTier\.A4\] OGSimNet may not include OGTickDriver: "OGSimulation/TickDriverConcepts\.h"' }
        @{ Arm = 'A4'; Bundle = 'a4_session'; Exit = 1; Line = 'SessionTypes\.h:4: \[ModuleTier\.A4\] OGSimSession may not include OGSimulationJolt: "OGSimulationJolt/JoltWorld\.h"' }
        @{ Arm = 'R1'; Bundle = 'r1'; Exit = 0; Line = '\[ModuleTier\.R1\] OGSimulation/Network/SimInbox\.h \(OGSimulation\) is included by no file of its own group; used by OGSimNet \[passenger\]' }
        @{ Arm = 'R2'; Bundle = 'r2'; Exit = 0; Line = 'BrawlerTickDriverGameUImpl\.h:6: \[ModuleTier\.R2\] m_session\.m_manager reaches a member of UESimulationSession' }
    ) {
        $r = Invoke-OnTree @('clean', $Bundle)
        $r.Exit | Should -Be $Exit
        $r.Counts[$Arm] | Should -Be 1
        foreach ($other in @('A1', 'A2', 'A3', 'A4', 'R1', 'R2') | Where-Object { $_ -ne $Arm }) {
            $r.Counts[$other] | Should -Be 0 -Because "$other must stay at zero on the $Bundle fixture"
        }
        $r.Text | Should -Match $Line
    }

    It 'the two include lines task 18 removed from ClockDilationController.h make A1 fire twice' {
        $r = Invoke-OnTree @('clean', 'task18')
        $r.Exit | Should -Be 1
        $r.Counts['A1'] | Should -Be 2
        foreach ($other in 'A2', 'A3', 'A4', 'R1', 'R2') { $r.Counts[$other] | Should -Be 0 }
        $r.Text | Should -Match 'ClockDilationController\.h:4: \[ModuleTier\.A1\] OGSimulation \(tier 0\) includes "OGSimulation/TickDriverConcepts\.h"'
        $r.Text | Should -Match 'ClockDilationController\.h:5: \[ModuleTier\.A1\] OGSimulation \(tier 0\) includes "OGSimulation/TickDriverLog\.h"'
    }

    It 'every include task 18 removed: three upward lines are A1; og-sim-net -> og-tickdriver is not upward, so it is A4' {
        $r = Invoke-OnTree @('clean', 'task18_all')
        $r.Counts['A1'] | Should -Be 3
        $r.Counts['A4'] | Should -Be 1
        $r.Text | Should -Not -Match 'SimNetIdle\.h:\d+: \[ModuleTier\.A1\]'
        $r.Text | Should -Match 'SimNetIdle\.h:3: \[ModuleTier\.A4\] OGSimNet may not include OGTickDriver'
    }

    It 'an include A1 already reports is not reported again by A4, though the pair is also in MayNotInclude' {
        (Import-PowerShellDataFile $script:Table).Nodes.OGSimSession.MayNotInclude | Should -Contain 'OGSimulationUnreal' -Because 'the case needs a pair both arms name'
        $r = Invoke-OnTree @('clean', 'a1_maynot')
        $r.Exit | Should -Be 1
        $r.Counts['A1'] | Should -Be 1
        $r.Counts['A4'] | Should -Be 0
        $r.Text | Should -Match 'SessionHost\.h:3: \[ModuleTier\.A1\] OGSimSession \(tier 3\) includes "UESimulationSession\.h" of OGSimulationUnreal \(tier 5\)'
        $r.Text | Should -Not -Match '\[ModuleTier\.A4\]'
    }
}

Describe 'module_tier_lint: passengers are computed by reachability through og-sim headers' {
    BeforeAll { $script:Reach = Invoke-OnTree @('clean', 'passengers_reach') }

    It 'reports the bundle clean, with three orphan headers and two passengers' {
        $script:Reach.Exit | Should -Be 0
        foreach ($a in 'A1', 'A2', 'A3', 'A4', 'R2') { $script:Reach.Counts[$a] | Should -Be 0 -Because "$a on the passengers_reach fixture" }
        $script:Reach.Counts['R1'] | Should -Be 3
        $script:Reach.Text | Should -Match '\[ModuleTier\.R1\.Passengers\] headers of OGSimulation that only OGSimNet, OGSimSession, OGSimulationUnreal reach: 2'
    }

    It 'a header the game reaches through another og-sim header is not a passenger, though only OGSimNet includes it' {
        $script:Reach.Text | Should -Match '\[ModuleTier\.R1\] OGSimulation/Network/SimTransport\.h \(OGSimulation\) is included by no file of its own group; used by OGSimNet\n'
        $script:Reach.Text | Should -Not -Match 'SimTransport\.h <- '
    }

    It 'a header only a passenger includes is a passenger too' {
        $script:Reach.Text | Should -Match '(?m)^      OGSimulation/Network/SimInbox\.h <- OGSimNet$'
        $script:Reach.Text | Should -Match '(?m)^      OGSimulation/Network/SimInboxSlot\.h <- only through other passengers$'
    }
}

Describe 'module_tier_lint: -Poison plants one violation in a temporary copy of the fixtures' {
    It '-Poison <Arm> fires its arm and only its arm' -ForEach @(
        @{ Arm = 'A1' }, @{ Arm = 'A2' }, @{ Arm = 'A3' }, @{ Arm = 'A4' }, @{ Arm = 'R1' }, @{ Arm = 'R2' }
    ) {
        $r = Invoke-Lint "-Poison $Arm"
        $r.Exit | Should -Be 1
        $r.Text | Should -Match "POISON ARM FIRED: $Arm = 1, every other arm 0"
    }

    It 'refuses a fixture that is not clean before planting (exit 2)' {
        $r = Invoke-Lint "-Poison A1 -FixturesPath $(Get-BundleArg @('clean', 'a2'))"
        $r.Exit | Should -Be 2
        $r.Text | Should -Match 'not clean before planting \(A2\)'
    }

    It 'leaves no temporary tree behind' {
        $before = @(Get-ChildItem ([IO.Path]::GetTempPath()) -Directory -Filter 'module_tier_poison_*').Count
        Invoke-Lint '-Poison R2' | Out-Null
        @(Get-ChildItem ([IO.Path]::GetTempPath()) -Directory -Filter 'module_tier_poison_*').Count | Should -Be $before
    }
}

Describe 'module_tier_lint: the same table on another layout' {
    It 'resolves a Godot-shaped tree (libraries under thirdparty/, the two folders at the top)' {
        $r = Invoke-OnTree @('godot')
        $r.Exit | Should -Be 0
        foreach ($n in 'OGNet', 'OGSimulation', 'OGTickDriver', 'OGSimNet', 'OGSimSession') { $r.Text | Should -Match "(?m)^  $n +tier \d +\S+ +present" }
        foreach ($a in 'A1', 'A2', 'A3', 'A4', 'R1', 'R2') { $r.Counts[$a] | Should -Be 0 }
        $r.Text | Should -Match '\[ModuleTier\.R1\.Watch\] OGSimSession -> OGSimNet: og-sim-session/OGSimSession/SessionTypes\.h -> OGSimNet/SimNetIdle\.h'
    }

    It 'gates an upward include there too' {
        $r = Invoke-OnTree @('godot', 'godot_a1')
        $r.Exit | Should -Be 1
        $r.Counts['A1'] | Should -Be 1
        $r.Text | Should -Match 'thirdparty/og-simulation/OGSimulation/OGAssert\.h:3: \[ModuleTier\.A1\] OGSimulation \(tier 0\) includes "OGSimSession/SessionTypes\.h" of OGSimSession \(tier 3\)'
    }
}

Describe 'module_tier_lint: misconfiguration exits 2' {
    It 'a missing table' {
        $r = Invoke-Lint "-RepoRoot '$(New-Tree @('clean'))' -TablePath '$(Join-Path $TestDrive 'none.psd1')'"
        $r.Exit | Should -Be 2
        $r.Text | Should -Match 'table not found: \S+none\.psd1'
    }
    It 'an unknown top-level key' {
        $t = New-TableVariant 'Extensions      =' 'Extension       ='
        $r = Invoke-Lint "-RepoRoot '$(New-Tree @('clean'))' -TablePath '$t'"
        $r.Exit | Should -Be 2
        $r.Text | Should -Match "table: unknown key 'Extension'"
    }
    It 'a node without a Tier' {
        $t = New-TableVariant 'OGNetUnreal            = @{ Tier = 4; EngineFree = $false;' 'OGNetUnreal            = @{ EngineFree = $false;'
        $r = Invoke-Lint "-RepoRoot '$(New-Tree @('clean'))' -TablePath '$t'"
        $r.Exit | Should -Be 2
        $r.Text | Should -Match "table: node 'OGNetUnreal' needs an integer Tier >= 0"
    }
    It 'two nodes claiming one file at the same depth' {
        $t = New-TableVariant 'OGNetUnreal            = @{' "Twin = @{ Tier = 0; EngineFree = `$true; Roots = @('**/og-net/OGNet') }`n        OGNetUnreal            = @{"
        $r = Invoke-Lint "-RepoRoot '$(New-Tree @('clean'))' -TablePath '$t'"
        $r.Exit | Should -Be 2
        $r.Text | Should -Match 'claimed by OGNet and Twin'
    }
    It 'a MayNotInclude entry naming no node' {
        $t = New-TableVariant "MayNotInclude = @('OGTickDriver', 'OGSimulationJolt', 'OGBrawler')" "MayNotInclude = @('OGTickDrivr', 'OGSimulationJolt', 'OGBrawler')"
        $r = Invoke-Lint "-RepoRoot '$(New-Tree @('clean'))' -TablePath '$t'"
        $r.Exit | Should -Be 2
        $r.Text | Should -Match "MayNotInclude names 'OGTickDrivr'"
    }
    It 'a Watch entry naming no node' {
        $t = New-TableVariant "@{ From = 'OGSimSession'; To = 'OGSimNet' }" "@{ From = 'OGSession'; To = 'OGSimNet' }"
        $r = Invoke-Lint "-RepoRoot '$(New-Tree @('clean'))' -TablePath '$t'"
        $r.Exit | Should -Be 2
        $r.Text | Should -Match "table: Watch 'OGSession -> OGSimNet' names an unknown node"
    }
    It 'a repository root under which no table root resolves' {
        $empty = Join-Path $TestDrive 'empty'
        New-Item -ItemType Directory -Force $empty | Out-Null
        $r = Invoke-Lint "-RepoRoot '$empty'"
        $r.Exit | Should -Be 2
        $r.Text | Should -Match 'no table root resolves to a source file under \S+empty'
    }
}
