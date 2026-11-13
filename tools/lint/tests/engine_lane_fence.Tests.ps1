# SPDX-License-Identifier: MPL-2.0
#Requires -Modules Pester
<#
    Pester spec for tools/lint/engine_lane_fence.ps1 (og-input-latency tasks P7-1, P7-3).

    WHY THIS FILE EXISTS. The fence's value is that it tells a classified engine
    lane from an unclassified one, and that must be a TEST, not a claim. Under the
    house rule of palette_legend_lint.Tests.ps1, every arm has a mutation of ONE
    clean fixture that fires that arm's tag and no other tag, so no arm can be
    vacuous while this spec is green; and every negative control has a twin that
    is the same text uncommented, so no control can be vacuous either.

    THE FIXTURE is a small `Source/Fix/` tree plus a manifest, built in TestDrive:
    for each case and passed through -RepoRoot, -ScanRoots Source and
    -ManifestPath. Nothing here reads or writes the repository tree. Its Sim rows
    reuse frozen key names (ASimulationInputRelay, m_relayedInputRing), so the spec
    needs no override of the lint's frozen list.

      Wrapper.h  USTRUCT FWrapper, in a header that includes "OGSimulation/FakeCodec.h"
                 (so FWrapper is a sim wire type)
      Fix.h      UCLASS AFix: Presentation Property Color (FLinearColor) and Session
                 Rpc ServerLeave(); UCLASS ASimulationInputRelay: Sim Property
                 m_relayedInputRing (FWrapper)
      Fix.cpp    both classes' DOREPLIFETIME, and ASimulationInputRelay's
                 bReplicates = true (the Sim ReplicatedClass)
      reflection.log  the Editor dump of the same fixture for -ReflectionLog
                 (Context (f), R1): both Properties, the Rpc, and both classes'
                 ReplicatedClass lines, behind a UE log prefix

    Every mutation is an exact-once text edit: an edit whose anchor is missing or
    repeated throws, so no case can silently test the clean fixture.

    Run:  pwsh -NoProfile -Command "Invoke-Pester tools/lint/tests/engine_lane_fence.Tests.ps1 -Output Detailed"
#>

BeforeDiscovery {
    $Anchor = @{
        HeaderAfterColor = "	FLinearColor Color = FLinearColor::White;`n"
        HeaderAfterRpc   = "	void ServerLeave();`n"
        HeaderUClassFix  = "UCLASS()`nclass FIX_API AFix"
        CppAfterDoRep    = "	DOREPLIFETIME(AFix, Color);`n"
        CppCtorBody      = "	PrimaryActorTick.bCanEverTick = false;`n"
        CppLogLine       = "	UE_LOG(LogTemp, Log, TEXT(`"leave`"));`n"
        CppEnd           = "	DOREPLIFETIME(ASimulationInputRelay, m_relayedInputRing);`n}`n"
        ManifestEnd      = "| ReplicatedClass | ASimulationInputRelay | * | - | Sim | P5 | relay host |`n"
        LogColor         = "Display: [EngineLane.Reflected] Property AFix Color FLinearColor`n"
        LogRpc           = "Display: [EngineLane.Reflected] Rpc AFix ServerLeave -`n"
        LogRelayClass    = "Display: [EngineLane.Reflected] ReplicatedClass ASimulationInputRelay * -`n"
        EngineClassRow   = "| EngineClass | AEngineOnly | * | - | Session | - | engine base APlayerState |`n"
        EngineOnlyLine   = "[2026.11.12-22.00.00:004][  0]LogAutomationController: Display: [EngineLane.Reflected] ReplicatedClass AEngineOnly * -`n"
    }
}

BeforeAll {
    $script:Lint = Join-Path (Split-Path -Parent $PSScriptRoot) 'engine_lane_fence.ps1'
    if (-not (Test-Path $script:Lint)) { throw "lint not found at $script:Lint" }
    $script:Pwsh = (Get-Process -Id $PID).Path

    $script:Base = [ordered]@{}
    $script:Base['Source/Fix/Wrapper.h'] = @'
#pragma once
// SPDX-License-Identifier: MPL-2.0
#include "CoreMinimal.h"
#include "OGSimulation/FakeCodec.h"
#include "Wrapper.generated.h"

USTRUCT()
struct FIX_API FWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Payload = 0;
};
'@
    $script:Base['Source/Fix/Fix.h'] = @'
#pragma once
// SPDX-License-Identifier: MPL-2.0
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Wrapper.h"
#include "Fix.generated.h"

UCLASS()
class FIX_API AFix : public AActor
{
	GENERATED_BODY()

public:
	AFix();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Color)
	FLinearColor Color = FLinearColor::White;

	UFUNCTION(Server, Reliable)
	void ServerLeave();

	UFUNCTION()
	void OnRep_Color();
};

UCLASS()
class FIX_API ASimulationInputRelay : public AActor
{
	GENERATED_BODY()

public:
	ASimulationInputRelay();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	FWrapper m_relayedInputRing;
};
'@
    $script:Base['Source/Fix/Fix.cpp'] = @'
// SPDX-License-Identifier: MPL-2.0
#include "Fix.h"
#include "Net/UnrealNetwork.h"

AFix::AFix()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AFix::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFix, Color);
}

void AFix::ServerLeave_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("leave"));
}

void AFix::OnRep_Color()
{
}

ASimulationInputRelay::ASimulationInputRelay()
{
	bReplicates = true;
}

void ASimulationInputRelay::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASimulationInputRelay, m_relayedInputRing);
}
'@
    $script:Base['manifest.md'] = @'
<!-- SPDX-License-Identifier: MPL-2.0 -->
# Fixture manifest

sim-rows: open

| Kind | Class | Member | Type | Lane class | Retired by | Readers |
|---|---|---|---|---|---|---|
| Property | AFix | Color | FLinearColor | Presentation |  | OnRep_Color |
| Rpc | AFix | ServerLeave | - | Session |  | ServerLeave_Implementation |
| Property | ASimulationInputRelay | m_relayedInputRing | FWrapper | Sim | P5 | relay reader |
| ReplicatedClass | ASimulationInputRelay | * | - | Sim | P5 | relay host |
'@
    $script:Base['reflection.log'] = @'
[2026.11.12-22.00.00:000][  0]LogAutomationController: Display: Test Started. Name={EngineLaneMembers}
[2026.11.12-22.00.00:001][  0]LogAutomationController: Display: [EngineLane.Reflected] Property AFix Color FLinearColor
[2026.11.12-22.00.00:002][  0]LogAutomationController: Display: [EngineLane.Reflected] Property ASimulationInputRelay m_relayedInputRing FWrapper
[2026.11.12-22.00.00:003][  0]LogAutomationController: Display: [EngineLane.Reflected] ReplicatedClass AFix * -
[2026.11.12-22.00.00:004][  0]LogAutomationController: Display: [EngineLane.Reflected] ReplicatedClass ASimulationInputRelay * -
[2026.11.12-22.00.00:005][  0]LogAutomationController: Display: [EngineLane.Reflected] Rpc AFix ServerLeave -
[2026.11.12-22.00.00:006][  0]LogAutomationController: Display: Test Completed. Result={Success}
'@


    function Edit-Once {
        param([string]$Text, [string]$Old, [string]$New, [string]$What)
        $n = [regex]::Matches($Text, [regex]::Escape($Old)).Count
        if ($n -ne 1) { throw "fixture edit '$What': expected exactly one occurrence of the anchor, found $n" }
        return $Text.Replace($Old, $New)
    }

    function Get-Fixture {
        param([object[]]$Edits = @())
        $files = [ordered]@{}
        foreach ($k in $script:Base.Keys) { $files[$k] = $script:Base[$k].Replace("`r`n", "`n") + "`n" }
        foreach ($e in $Edits) {
            $mode = if ($e.ContainsKey('Mode')) { $e.Mode } else { 'After' }
            $old = $e.Old
            $new = switch ($mode) {
                'After'   { $old + $e.New }
                'Before'  { $e.New + $old }
                'Replace' { $e.New }
            }
            $files[$e.File] = Edit-Once $files[$e.File] $old $new "$($e.File) $mode"
        }
        return $files
    }

    function New-FixtureRoot {
        param([object[]]$Edits = @(), [string[]]$Omit = @())
        $root = Join-Path $TestDrive ([Guid]::NewGuid().ToString('N'))
        $files = Get-Fixture -Edits $Edits
        foreach ($k in $files.Keys) {
            if ($Omit -contains $k) { continue }
            $p = Join-Path $root $k
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $p) | Out-Null
            [IO.File]::WriteAllText($p, $files[$k])
        }
        return $root
    }

    function Invoke-Lint {
        param([string]$Root, [hashtable]$Extra = @{})
        $a = @{
            RepoRoot     = $Root
            ScanRoots    = @('Source')
            ManifestPath = (Join-Path $Root 'manifest.md')
        }
        foreach ($k in $Extra.Keys) { $a[$k] = $Extra[$k] }
        $out = & $script:Lint @a *>&1 | Out-String
        $code = $LASTEXITCODE
        $tags = @([regex]::Matches($out, '\[E4-([A-Z]\d)\]') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
        return [pscustomobject]@{ Out = $out; Code = $code; Tags = $tags }
    }

    function Invoke-LintProcess {
        param([string[]]$LintArgs)
        $lines = @(& $script:Pwsh -NoProfile -File $script:Lint @LintArgs 2>&1 | ForEach-Object { "$_" } | Where-Object { $_.Trim() -ne '' })
        return [pscustomobject]@{ Lines = $lines; Code = $LASTEXITCODE }
    }

    $script:CleanSummary = 'rows=4 Sim=2 Presentation=1 Session=1 Binding=0 frozen=14 sim-rows=open'
}

Describe 'engine_lane_fence.ps1' {

    Context '(a) clean' {
        It 'exits 0 with no violation and the summary rows=4 Sim=2 Presentation=1 Session=1 Binding=0' {
            $r = Invoke-Lint (New-FixtureRoot)
            ($r.Tags -join ',') | Should -Be ''
            $r.Out | Should -Match ([regex]::Escape("[E4] $script:CleanSummary"))
            $r.Out | Should -Match 'files=3 markers=6 keys=4 violations=0 elapsed=\d+\.\d\d s'
            $r.Code | Should -Be 0
        }

        It 'a relative -ManifestPath is taken under -RepoRoot, not the current directory' {
            $root = New-FixtureRoot
            Push-Location $TestDrive
            try { $r = Invoke-Lint $root @{ ManifestPath = 'manifest.md' } }
            finally { Pop-Location }
            ($r.Tags -join ',') | Should -Be ''
            $r.Out | Should -Match ([regex]::Escape("[E4] $script:CleanSummary"))
            $r.Code | Should -Be 0
        }
    }

    Context '(b) negative controls exit 0, and each control''s uncommented twin fires' {
        It '<Name>: the control is clean' -TestCases @(
            @{ Name = '// UPROPERTY(Replicated) line'
               Control = @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterColor; New = "`n	// UPROPERTY(Replicated)`n	int32 NotReplicated = 0;`n" }
               Twin    = @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterColor; New = "`n	UPROPERTY(Replicated)`n	int32 NotReplicated = 0;`n" }
               TwinTags = @('J1', 'J3') }
            @{ Name = '/* DOREPLIFETIME(A, B); */ block'
               Control = @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppAfterDoRep; New = "	/* DOREPLIFETIME(A, B); */`n" }
               Twin    = @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppAfterDoRep; New = "	DOREPLIFETIME(A, B);`n" }
               TwinTags = @('J1', 'J3') }
            @{ Name = 'multi-line /* */ block holding DOREPLIFETIME(A, B)'
               Control = @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppAfterDoRep; New = "	/*`n	DOREPLIFETIME(A, B);`n	*/`n" }
               Twin    = @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppAfterDoRep; New = "	`n	DOREPLIFETIME(A, B);`n	`n" }
               TwinTags = @('J1', 'J3') }
            @{ Name = 'TEXT("(DOREPLIFETIME, x)") string'
               Control = @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppLogLine; New = "	UE_LOG(LogTemp, Log, TEXT(`"(DOREPLIFETIME, x)`"));`n" }
               Twin    = @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppLogLine; New = "	UE_LOG(LogTemp, Log, TEXT(DOREPLIFETIME(A, x)));`n" }
               TwinTags = @('J1', 'J3') }
            @{ Name = 'TEXT("DOREPLIFETIME(AFix, Ghost)") string (a string that would match unstripped)'
               Control = @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppLogLine; New = "	UE_LOG(LogTemp, Log, TEXT(`"DOREPLIFETIME(AFix, Ghost)`"));`n" }
               Twin    = @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppLogLine; New = "	UE_LOG(LogTemp, Log, TEXT(DOREPLIFETIME(AFix, Ghost)));`n" }
               TwinTags = @('J1', 'J3') }
            @{ Name = 'bReplicates = false;'
               Control = @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppCtorBody; New = "	bReplicates = false;`n" }
               Twin    = @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppCtorBody; New = "	bReplicates = true;`n" }
               TwinTags = @('J1') }
            @{ Name = 'class AForward; between the UCLASS( and the class'
               Control = @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderUClassFix; Mode = 'Replace'; New = "UCLASS()`nclass AForward;`nclass FIX_API AFix" }
               Twin    = @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderUClassFix; Mode = 'Replace'; New = "UCLASS()`nclass AForward {};`nclass FIX_API AFix" }
               TwinTags = @('J1', 'J2', 'J3') }
            @{ Name = 'Category = Client and Category = Replicated specifier values'
               Control = @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterRpc; New = "`n	UFUNCTION(BlueprintCallable, Category = Client)`n	void LocalOnly();`n`n	UPROPERTY(EditAnywhere, Category = Replicated)`n	int32 Tunable = 0;`n" }
               Twin    = @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterRpc; New = "`n	UFUNCTION(BlueprintCallable, Client)`n	void LocalOnly();`n" }
               TwinTags = @('J1') }
            @{ Name = 'NotReplicated specifier'
               Control = @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterColor; New = "`n	UPROPERTY(NotReplicated)`n	int32 Local = 0;`n" }
               Twin    = @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterColor; New = "`n	UPROPERTY(Replicated)`n	int32 Local = 0;`n" }
               TwinTags = @('J1', 'J3') }
        ) {
            param($Name, $Control, $Twin, $TwinTags)
            $r = Invoke-Lint (New-FixtureRoot -Edits @($Control))
            ($r.Tags -join ',') | Should -Be ''
            $r.Out | Should -Match ([regex]::Escape("[E4] $script:CleanSummary"))
            $r.Code | Should -Be 0

            $t = Invoke-Lint (New-FixtureRoot -Edits @($Twin))
            ($t.Tags -join ',') | Should -Be ($TwinTags -join ',')
            $t.Code | Should -Be 1
        }

        It 'all the spec''s five controls together are clean' -TestCases @(@{ Edits = @(
                @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterColor; New = "`n	// UPROPERTY(Replicated)`n	int32 NotReplicated = 0;`n" }
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppAfterDoRep; New = "	/* DOREPLIFETIME(A, B); */`n" }
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppLogLine; New = "	UE_LOG(LogTemp, Log, TEXT(`"(DOREPLIFETIME, x)`"));`n" }
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppCtorBody; New = "	bReplicates = false;`n" }
                @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderUClassFix; Mode = 'Replace'; New = "UCLASS()`nclass AForward;`nclass FIX_API AFix" }
            ) }) {
            param($Edits)
            $r = Invoke-Lint (New-FixtureRoot -Edits $Edits)
            ($r.Tags -join ',') | Should -Be ''
            $r.Out | Should -Match ([regex]::Escape("[E4] $script:CleanSummary"))
            $r.Code | Should -Be 0
        }
    }

    Context '(c) one mutation per arm exits 1 with exactly its tag' {
        It '<Arm>: <Name>' -TestCases @(
            @{ Arm = 'J1'; Name = 'an unlisted UPROPERTY(Replicated) and its DOREPLIFETIME'; Edits = @(
                @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterColor; New = "`n	UPROPERTY(Replicated)`n	int32 Extra = 0;`n" }
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppAfterDoRep; New = "	DOREPLIFETIME(AFix, Extra);`n" }) }
            @{ Arm = 'J2'; Name = 'a row with no marker'; Edits = @(
                @{ File = 'manifest.md'; Old = $Anchor.ManifestEnd; New = "| Property | AFix | Ghost | int32 | Presentation |  | nobody |`n" }) }
            @{ Arm = 'J3'; Name = 'a listed DOREPLIFETIME with no header half'; Edits = @(
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppAfterDoRep; New = "	DOREPLIFETIME(AFix, Half);`n" }
                @{ File = 'manifest.md'; Old = $Anchor.ManifestEnd; New = "| Property | AFix | Half | int32 | Presentation |  | hud |`n" }) }
            @{ Arm = 'C1'; Name = 'an existing row''s lane class changed to Cosmetic'; Edits = @(
                @{ File = 'manifest.md'; Old = '| FLinearColor | Presentation |'; Mode = 'Replace'; New = '| FLinearColor | Cosmetic |' }) }
            @{ Arm = 'C2'; Name = 'a Sim row Property|AFix|NewSim with its marker'; Edits = @(
                @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterColor; New = "`n	UPROPERTY(Replicated)`n	uint8 NewSim = 0;`n" }
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppAfterDoRep; New = "	DOREPLIFETIME(AFix, NewSim);`n" }
                @{ File = 'manifest.md'; Old = $Anchor.ManifestEnd; New = "| Property | AFix | NewSim | uint8 | Sim | P5 | a step |`n" }) }
            @{ Arm = 'C3'; Name = 'sim-rows: closed'; Edits = @(
                @{ File = 'manifest.md'; Old = 'sim-rows: open'; Mode = 'Replace'; New = 'sim-rows: closed' }) }
            @{ Arm = 'C4'; Name = 'a Binding row Property|AFix|Other with its marker'; Edits = @(
                @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterColor; New = "`n	UPROPERTY(Replicated)`n	uint8 Other = 0;`n" }
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppAfterDoRep; New = "	DOREPLIFETIME(AFix, Other);`n" }
                @{ File = 'manifest.md'; Old = $Anchor.ManifestEnd; New = "| Property | AFix | Other | uint8 | Binding |  | binder |`n" }) }
            @{ Arm = 'C5'; Name = 'a duplicated row'; Edits = @(
                @{ File = 'manifest.md'; Old = $Anchor.ManifestEnd; New = "| Property | AFix | Color | FLinearColor | Presentation |  | OnRep_Color |`n" }) }
            @{ Arm = 'T1'; Name = 'the Presentation property''s type changed to the wrapper (Type cell updated)'; Edits = @(
                @{ File = 'Source/Fix/Fix.h'; Old = "	FLinearColor Color = FLinearColor::White;"; Mode = 'Replace'; New = "	FWrapper Color;" }
                @{ File = 'manifest.md'; Old = '| Color | FLinearColor |'; Mode = 'Replace'; New = '| Color | FWrapper |' }) }
            @{ Arm = 'T1'; Name = 'the Session Rpc given a const FWrapper& parameter (Type cell updated)'; Edits = @(
                @{ File = 'Source/Fix/Fix.h'; Old = "	void ServerLeave();"; Mode = 'Replace'; New = "	void ServerLeave(const FWrapper& Payload);" }
                @{ File = 'manifest.md'; Old = '| ServerLeave | - |'; Mode = 'Replace'; New = '| ServerLeave | const FWrapper& |' }) }
            @{ Arm = 'T2'; Name = 'a Type cell int32 against a source uint8'; Edits = @(
                @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterColor; New = "`n	UPROPERTY(Replicated)`n	uint8 Level = 0;`n" }
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppAfterDoRep; New = "	DOREPLIFETIME(AFix, Level);`n" }
                @{ File = 'manifest.md'; Old = $Anchor.ManifestEnd; New = "| Property | AFix | Level | int32 | Presentation |  | hud |`n" }) }
        ) {
            param($Arm, $Name, $Edits)
            $r = Invoke-Lint (New-FixtureRoot -Edits $Edits)
            ($r.Tags -join ',') | Should -Be $Arm
            $r.Out | Should -Match '\[E4\] rows=\d+ Sim=\d+ Presentation=\d+ Session=\d+ Binding=\d+ frozen=14 sim-rows=(open|closed)'
            $r.Out | Should -Match ('(?m)^\S+:\d+: \[E4-' + $Arm + '\] ')
            $r.Code | Should -Be 1
        }

        It 'J3, not J2: deleting only a listed DOREPLIFETIME leaves the header half as the row''s marker (P7-2 F2)' -TestCases @(@{ DoRep = $Anchor.CppAfterDoRep }) {
            param($DoRep)
            $edits = @(@{ File = 'Source/Fix/Fix.cpp'; Old = $DoRep; Mode = 'Replace'; New = '' })
            $r = Invoke-Lint (New-FixtureRoot -Edits $edits)
            ($r.Tags -join ',') | Should -Be 'J3'
            $r.Out | Should -Match 'Property\|AFix\|Color has a replicated UPROPERTY but no DOREPLIFETIME'
            $r.Code | Should -Be 1

            $both = @($edits[0], @{ File = 'Source/Fix/Fix.h'; Old = 'UPROPERTY(ReplicatedUsing = OnRep_Color)'; Mode = 'Replace'; New = 'UPROPERTY()' })
            $r = Invoke-Lint (New-FixtureRoot -Edits $both)
            ($r.Tags -join ',') | Should -Be 'J2'
            $r.Code | Should -Be 1
        }

        It 'C4: a second Binding row fires C4 for the count as well' {
            $edits = @(
                @{ File = 'manifest.md'; Old = '| FLinearColor | Presentation |'; Mode = 'Replace'; New = '| FLinearColor | Binding |' }
                @{ File = 'manifest.md'; Old = '| ServerLeave | - | Session |'; Mode = 'Replace'; New = '| ServerLeave | - | Binding |' }
            )
            $r = Invoke-Lint (New-FixtureRoot -Edits $edits)
            ($r.Tags -join ',') | Should -Be 'C4'
            $r.Out | Should -Match '2 Binding rows; at most one is allowed'
            $r.Code | Should -Be 1
        }

        It 'C5: a Sim row with an empty Retired by' {
            $edits = @(@{ File = 'manifest.md'; Old = '| FWrapper | Sim | P5 |'; Mode = 'Replace'; New = '| FWrapper | Sim |  |' })
            $r = Invoke-Lint (New-FixtureRoot -Edits $edits)
            ($r.Tags -join ',') | Should -Be 'C5'
            $r.Out | Should -Match "empty 'Retired by'"
            $r.Code | Should -Be 1
        }

        It 'T1: an object reference to the wrapper is exempt, a TArray of it is not' {
            $ptr = @(
                @{ File = 'Source/Fix/Fix.h'; Old = "	FLinearColor Color = FLinearColor::White;"; Mode = 'Replace'; New = "	TObjectPtr<FWrapper> Color;" }
                @{ File = 'manifest.md'; Old = '| Color | FLinearColor |'; Mode = 'Replace'; New = '| Color | TObjectPtr<FWrapper> |' })
            $r = Invoke-Lint (New-FixtureRoot -Edits $ptr)
            ($r.Tags -join ',') | Should -Be ''
            $r.Code | Should -Be 0

            $arr = @(
                @{ File = 'Source/Fix/Fix.h'; Old = "	FLinearColor Color = FLinearColor::White;"; Mode = 'Replace'; New = "	TArray<FWrapper> Color;" }
                @{ File = 'manifest.md'; Old = '| Color | FLinearColor |'; Mode = 'Replace'; New = '| Color | TArray<FWrapper> |' })
            $r = Invoke-Lint (New-FixtureRoot -Edits $arr)
            ($r.Tags -join ',') | Should -Be 'T1'
            $r.Code | Should -Be 1
        }
    }

    Context '(d) every marker form is seen: each unlisted form fires J1 alone' {
        It '<Form> -> <Key>' -TestCases @(
            @{ Form = 'DOREPLIFETIME_CONDITION'; Key = 'Property|AFix|Cond'; Edits = @(
                @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterColor; New = "`n	UPROPERTY(Replicated)`n	int32 Cond = 0;`n" }
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppAfterDoRep; New = "	DOREPLIFETIME_CONDITION(AFix, Cond, COND_OwnerOnly);`n" }) }
            @{ Form = 'DOREPLIFETIME_WITH_PARAMS_FAST'; Key = 'Property|AFix|Fast'; Edits = @(
                @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterColor; New = "`n	UPROPERTY(Replicated, Transient)`n	int32 Fast = 0;`n" }
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppAfterDoRep; New = "	DOREPLIFETIME_WITH_PARAMS_FAST(AFix, Fast, Params);`n" }) }
            @{ Form = 'UFUNCTION(Client, Reliable)'; Key = 'Rpc|AFix|ClientPing'; Edits = @(
                @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterRpc; New = "`n	UFUNCTION(Client, Reliable)`n	void ClientPing();`n" }) }
            @{ Form = 'UFUNCTION(NetMulticast, Unreliable)'; Key = 'Rpc|AFix|MulticastBurst'; Edits = @(
                @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterRpc; New = "`n	UFUNCTION(NetMulticast, Unreliable)`n	void MulticastBurst(int32 Count);`n" }) }
            @{ Form = 'SetIsReplicatedByDefault(true)'; Key = 'ReplicatedClass|UFixComponent|*'; Edits = @(
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppEnd; New = "`nUFixComponent::UFixComponent()`n{`n	SetIsReplicatedByDefault(true);`n}`n" }) }
            @{ Form = 'SetIsReplicated(true)'; Key = 'ReplicatedClass|UFixOther|*'; Edits = @(
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppEnd; New = "`nvoid UFixOther::BeginPlay()`n{`n	Super::BeginPlay();`n	SetIsReplicated(true);`n}`n" }) }
            @{ Form = 'SetReplicates(true)'; Key = 'ReplicatedClass|AFix|*'; Edits = @(
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppCtorBody; New = "	SetReplicates(true);`n" }) }
            @{ Form = 'bReplicates = true in a header'; Key = 'ReplicatedClass|AFix|*'; Edits = @(
                @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterRpc; New = "`n	void Arm() { bReplicates = true; }`n" }) }
            @{ Form = 'AddReplicatedSubObject('; Key = 'SubObject|AFix|*'; Edits = @(
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppEnd; New = "`nvoid AFix::BeginPlay()`n{`n	Super::BeginPlay();`n	AddReplicatedSubObject(Sub);`n}`n" }) }
            @{ Form = 'ReplicateSubobject('; Key = 'SubObject|AFix|*'; Edits = @(
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppEnd; New = "`nbool AFix::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* Flags)`n{`n	return Channel->ReplicateSubobject(Sub, *Bunch, *Flags);`n}`n" }) }
            @{ Form = 'bReplicateUsingRegisteredSubObjectList = true'; Key = 'SubObject|AFix|*'; Edits = @(
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppCtorBody; New = "	bReplicateUsingRegisteredSubObjectList = true;`n" }) }
            @{ Form = 'RegisterReplicationFragments('; Key = 'SubObject|AFix|*'; Edits = @(
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppEnd; New = "`nvoid AFix::RegisterReplicationFragments(FFragmentRegistrationContext& Context, EFragmentRegistrationFlags Flags)`n{`n	Super::RegisterReplicationFragments(Context, Flags);`n}`n" }) }
            @{ Form = 'UE_NET_IMPLEMENT_...(Name)'; Key = 'IrisSerializer|-|FixWire'; Edits = @(
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppEnd; New = "`nUE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_AND_REGISTRY_DELEGATES(FixWire);`n" }) }
            @{ Form = 'UE_NET_DECLARE_...(Name)'; Key = 'IrisSerializer|-|FFixNetSerializer'; Edits = @(
                @{ File = 'Source/Fix/Wrapper.h'; Old = "};`n"; New = "`nUE_NET_DECLARE_SERIALIZER(FFixNetSerializer, FIX_API);`n" }) }
            @{ Form = 'a marker after a // comment holding /*'; Key = 'Rpc|AFix|ClientAfterSlashStar'; Edits = @(
                @{ File = 'Source/Fix/Fix.h'; Old = $Anchor.HeaderAfterRpc; New = "`n	// see Net/*.h`n	UFUNCTION(Client, Reliable)`n	void ClientAfterSlashStar();`n" }) }
            @{ Form = 'a marker after a string holding /*'; Key = 'ReplicatedClass|AFix|*'; Edits = @(
                @{ File = 'Source/Fix/Fix.cpp'; Old = $Anchor.CppCtorBody; New = "	UE_LOG(LogTemp, Log, TEXT(`"a/*b`"));`n	bReplicates = true;`n" }) }
        ) {
            param($Form, $Key, $Edits)
            $r = Invoke-Lint (New-FixtureRoot -Edits $Edits)
            ($r.Tags -join ',') | Should -Be 'J1'
            $r.Out | Should -Match ([regex]::Escape("marker $Key has no manifest row"))
            $r.Code | Should -Be 1
        }

        It 'Rpc parameter types are read from source: names and defaults dropped, & spacing normalised for T2' {
            $edits = @(
                @{ File = 'Source/Fix/Fix.h'; Old = "	void ServerLeave();"; Mode = 'Replace'; New = "	void ServerLeave(int32 Count, const FWrapper & Payload = FWrapper());" }
                @{ File = 'manifest.md'; Old = '| ServerLeave | - |'; Mode = 'Replace'; New = '| ServerLeave | int32, const FWrapper& |' })
            $r = Invoke-Lint (New-FixtureRoot -Edits $edits)
            ($r.Tags -join ',') | Should -Be 'T1'
            $r.Out | Should -Match "type 'const FWrapper ?&' carries sim wire type FWrapper"
            $r.Code | Should -Be 1
        }
    }

    Context '(e) exit 2: usage or IO, one message line and nothing else' {
        It '<Name>' -TestCases @(
            @{ Name = 'a missing manifest (explicit -ManifestPath)'; Omit = @(); Edits = @(); Manifest = 'missing.md'; Pattern = 'manifest not found: .*missing\.md' }
            @{ Name = 'a missing manifest (the default path under -RepoRoot)'; Omit = @(); Edits = @(); Manifest = $null; Pattern = 'manifest not found: .*tools[\\/]lint[\\/]engine_lane_manifest\.md' }
            @{ Name = 'a manifest without sim-rows:'; Omit = @(); Manifest = 'manifest.md'; Pattern = "no 'sim-rows:' line"; Edits = @(
                @{ File = 'manifest.md'; Old = "sim-rows: open`n"; Mode = 'Replace'; New = '' }) }
            @{ Name = 'a manifest with sim-rows: twice'; Omit = @(); Manifest = 'manifest.md'; Pattern = "'sim-rows:' appears 2 times"; Edits = @(
                @{ File = 'manifest.md'; Old = "sim-rows: open`n"; New = "sim-rows: open`n" }) }
            @{ Name = 'a sim-rows: value that is neither open nor closed'; Omit = @(); Manifest = 'manifest.md'; Pattern = "must be 'open' or 'closed'"; Edits = @(
                @{ File = 'manifest.md'; Old = 'sim-rows: open'; Mode = 'Replace'; New = 'sim-rows: shut' }) }
            @{ Name = 'a wrong table header'; Omit = @(); Manifest = 'manifest.md'; Pattern = 'the table header is not exactly'; Edits = @(
                @{ File = 'manifest.md'; Old = '| Lane class |'; Mode = 'Replace'; New = '| Lane |' }) }
            @{ Name = 'a row without 7 cells'; Omit = @(); Manifest = 'manifest.md'; Pattern = 'a row has 6 cells, not 7'; Edits = @(
                @{ File = 'manifest.md'; Old = '| Sim | P5 | relay host |'; Mode = 'Replace'; New = '| Sim | P5 |' }) }
            @{ Name = 'a missing scan root'; Omit = @(); Edits = @(); Manifest = 'manifest.md'; ScanRoot = 'Nowhere'; Pattern = 'scan root not found: Nowhere' }
        ) {
            param($Name, $Omit, $Edits, $Manifest, $Pattern, $ScanRoot)
            $root = New-FixtureRoot -Edits $Edits -Omit $Omit
            $lintArgs = @('-RepoRoot', $root, '-ScanRoots', $(if ($ScanRoot) { $ScanRoot } else { 'Source' }))
            if ($Manifest) { $lintArgs += @('-ManifestPath', (Join-Path $root $Manifest)) }
            $r = Invoke-LintProcess $lintArgs
            $r.Lines.Count | Should -Be 1
            $r.Lines[0] | Should -Match $Pattern
            $r.Code | Should -Be 2
        }
    }

    Context '(f) R1: the reflection dump (-ReflectionLog) against the manifest, both directions' {
        It 'clean: exit 0, the summary unchanged, and the reflection line counts lines, keys and classes' {
            $root = New-FixtureRoot
            $r = Invoke-Lint $root @{ ReflectionLog = (Join-Path $root 'reflection.log') }
            ($r.Tags -join ',') | Should -Be ''
            $r.Out | Should -Match ([regex]::Escape("[E4] $script:CleanSummary"))
            $r.Out | Should -Match ([regex]::Escape('[E4] reflection=reflection.log lines=5 keys=3 classes=2'))
            $r.Code | Should -Be 0
        }

        It 'a relative -ReflectionLog is taken under -RepoRoot' {
            $root = New-FixtureRoot
            $r = Invoke-Lint $root @{ ReflectionLog = 'reflection.log' }
            ($r.Tags -join ',') | Should -Be ''
            $r.Code | Should -Be 0
        }

        It 'without -ReflectionLog R1 does not run: a stale EngineClass row is not seen (J2 skips it), and no reflection line is printed' -TestCases @(
            @{ Edits = @(@{ File = 'manifest.md'; Old = $Anchor.ManifestEnd; New = $Anchor.EngineClassRow }) }
        ) {
            param($Edits)
            $r = Invoke-Lint (New-FixtureRoot -Edits $Edits)
            ($r.Tags -join ',') | Should -Be ''
            $r.Out | Should -Not -Match '\[E4\] reflection='
            $r.Out | Should -Match ([regex]::Escape('[E4] rows=5 Sim=2 Presentation=1 Session=2 Binding=0 frozen=14 sim-rows=open'))
            $r.Code | Should -Be 0
        }

        It '<Name>' -TestCases @(
            @{ Name = 'a repeated line (the controller''s second print) is one key: clean'; Tags = ''; Count = 'lines=6 keys=3 classes=2'; Edits = @(
                @{ File = 'reflection.log'; Old = $Anchor.LogColor; New = "[2026.11.12-22.00.09:000][  0]LogAutomationController: Display: [EngineLane.Reflected] Property AFix Color FLinearColor`n" }) }
            @{ Name = 'R1: an extra reflected Property with no row (a Blueprint or engine-hidden variable)'; Tags = 'R1'; Count = 'lines=6 keys=4 classes=2'; Pattern = 'reflected Property\|AFix\|Planted has no manifest row'; Edits = @(
                @{ File = 'reflection.log'; Old = $Anchor.LogColor; New = "[2026.11.12-22.00.09:000][  0]LogAutomationController: Display: [EngineLane.Reflected] Property AFix Planted uint8`n" }) }
            @{ Name = 'R1: a listed Property missing from the dump'; Tags = 'R1'; Count = 'lines=4 keys=2 classes=2'; Pattern = 'row Property\|AFix\|Color is not in the reflection dump'; Edits = @(
                @{ File = 'reflection.log'; Old = $Anchor.LogColor; Mode = 'Replace'; New = "Display: nothing`n" }) }
            @{ Name = 'R1: a listed Rpc missing from the dump'; Tags = 'R1'; Count = 'lines=4 keys=2 classes=2'; Pattern = 'row Rpc\|AFix\|ServerLeave is not in the reflection dump'; Edits = @(
                @{ File = 'reflection.log'; Old = $Anchor.LogRpc; Mode = 'Replace'; New = "Display: nothing`n" }) }
            @{ Name = 'R1: a reflected Rpc with no row'; Tags = 'R1'; Count = 'lines=6 keys=4 classes=2'; Pattern = 'reflected Rpc\|AFix\|ClientPing has no manifest row'; Edits = @(
                @{ File = 'reflection.log'; Old = $Anchor.LogRpc; New = "Display: [EngineLane.Reflected] Rpc AFix ClientPing int32`n" }) }
            @{ Name = 'R1: a reflected ReplicatedClass no row names'; Tags = 'R1'; Count = 'lines=6 keys=3 classes=3'; Pattern = 'reflected ReplicatedClass AEngineOnly is named by no manifest row'; Edits = @(
                @{ File = 'reflection.log'; Old = $Anchor.LogRelayClass; New = $Anchor.EngineOnlyLine }) }
            @{ Name = 'an EngineClass row names that class: clean (Session=2)'; Tags = ''; Count = 'lines=6 keys=3 classes=3'; Summary = 'rows=5 Sim=2 Presentation=1 Session=2 Binding=0 frozen=14 sim-rows=open'; Edits = @(
                @{ File = 'reflection.log'; Old = $Anchor.LogRelayClass; New = $Anchor.EngineOnlyLine }
                @{ File = 'manifest.md'; Old = $Anchor.ManifestEnd; New = $Anchor.EngineClassRow }) }
            @{ Name = 'R1: an EngineClass row whose class the dump does not replicate (stale)'; Tags = 'R1'; Count = 'lines=5 keys=3 classes=2'; Pattern = 'stale: EngineClass row EngineClass\|AEngineOnly\|\*'; Summary = 'rows=5 Sim=2 Presentation=1 Session=2 Binding=0 frozen=14 sim-rows=open'; Edits = @(
                @{ File = 'manifest.md'; Old = $Anchor.ManifestEnd; New = $Anchor.EngineClassRow }) }
            @{ Name = 'a class named only by its Property and Rpc rows needs no EngineClass row (AFix): clean'; Tags = ''; Count = 'lines=5 keys=3 classes=2'; Edits = @() }
        ) {
            param($Name, $Tags, $Count, $Pattern, $Summary, $Edits)
            $root = New-FixtureRoot -Edits $Edits
            $r = Invoke-Lint $root @{ ReflectionLog = (Join-Path $root 'reflection.log') }
            ($r.Tags -join ',') | Should -Be $Tags
            $r.Out | Should -Match ([regex]::Escape("[E4] reflection=reflection.log $Count"))
            $r.Out | Should -Match ([regex]::Escape("[E4] $(if ($Summary) { $Summary } else { $script:CleanSummary })"))
            if ($Pattern) { $r.Out | Should -Match ('(?m)^\S+:\d+: \[E4-R1\] ' + '.*' + $Pattern) }
            $r.Code | Should -Be $(if ($Tags) { 1 } else { 0 })
        }

        It 'EngineClass rows are checked without a log too: <Name>' -TestCases @(
            @{ Name = 'lane class Presentation -> C1'; Tag = 'C1'; Row = "| EngineClass | AEngineOnly | * | - | Presentation | - | x |`n" }
            @{ Name = 'a member other than * -> C1'; Tag = 'C1'; Row = "| EngineClass | AEngineOnly | Score | - | Session | - | x |`n" }
            @{ Name = 'a class another row names -> C5'; Tag = 'C5'; Row = "| EngineClass | AFix | * | - | Session | - | x |`n" }
        ) {
            param($Name, $Tag, $Row)
            $r = Invoke-Lint (New-FixtureRoot -Edits @(@{ File = 'manifest.md'; Old = "| ReplicatedClass | ASimulationInputRelay | * | - | Sim | P5 | relay host |`n"; New = $Row }))
            ($r.Tags -join ',') | Should -Be $Tag
            $r.Code | Should -Be 1
        }

        It 'exit 2: <Name>' -TestCases @(
            @{ Name = 'a missing log'; Log = 'missing.log'; Edits = @(); Pattern = 'reflection log not found: .*missing\.log' }
            @{ Name = 'a log with no [EngineLane.Reflected] line (a run that never reached the test)'; Log = 'reflection.log'; Pattern = 'no \[EngineLane\.Reflected\] line'; Edits = @(
                @{ File = 'reflection.log'; Old = '[EngineLane.Reflected] Property AFix Color'; Mode = 'Replace'; New = 'x' }
                @{ File = 'reflection.log'; Old = '[EngineLane.Reflected] Property ASimulationInputRelay'; Mode = 'Replace'; New = 'x' }
                @{ File = 'reflection.log'; Old = '[EngineLane.Reflected] ReplicatedClass AFix'; Mode = 'Replace'; New = 'x' }
                @{ File = 'reflection.log'; Old = '[EngineLane.Reflected] ReplicatedClass ASimulationInputRelay'; Mode = 'Replace'; New = 'x' }
                @{ File = 'reflection.log'; Old = '[EngineLane.Reflected] Rpc AFix'; Mode = 'Replace'; New = 'x' }) }
            @{ Name = 'a line that does not parse'; Log = 'reflection.log'; Pattern = 'reflection\.log:\d+: an \[EngineLane\.Reflected\] line that does not parse'; Edits = @(
                @{ File = 'reflection.log'; Old = '[EngineLane.Reflected] Rpc AFix ServerLeave -'; Mode = 'Replace'; New = '[EngineLane.Reflected] Function AFix ServerLeave -' }) }
        ) {
            param($Name, $Log, $Edits, $Pattern)
            $root = New-FixtureRoot -Edits $Edits
            $r = Invoke-LintProcess @('-RepoRoot', $root, '-ScanRoots', 'Source', '-ManifestPath', (Join-Path $root 'manifest.md'), '-ReflectionLog', (Join-Path $root $Log))
            $r.Lines.Count | Should -Be 1
            $r.Lines[0] | Should -Match $Pattern
            $r.Code | Should -Be 2
        }
    }
}
