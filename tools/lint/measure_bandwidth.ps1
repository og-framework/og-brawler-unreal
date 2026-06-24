#Requires -Version 7.0
# SPDX-License-Identifier: MPL-2.0
<#
.SYNOPSIS
    Phase 2a bandwidth-ceiling measurement harness (og-netcode-v1-impl, Task 19;
    Task 20 adds the -NetworkProfile cellular link-emulation option).

.DESCRIPTION
    Sibling of tools/lint/configurability_lint.ps1. Drives ONE `MaxClientRate`
    cap-ramp measurement run at one of the four T18-locked product-target
    topologies, on the post-Stage-2 60 Hz runtime, and captures the engine
    network/timing stats for that run.

    SAFETY CONTRACT — ephemeral Config/DefaultEngine.ini edit with mandatory
    restore (the load-bearing reason this is a script, not a manual edit):

      1. BEFORE any edit, the original file's SHA256 is captured into a
         script-local variable AND the verbatim original bytes are written to
         Config/DefaultEngine.ini.measure_bandwidth.bak.
      2. The `[/Script/OnlineSubsystemUtils.IpNetDriver]` section is written ephemerally with
         the requested cap (appended — the project ships no such section).
      3. The PIE session runs for -DurationSec seconds.
      4. In a `finally` block (so it runs on success, PIE crash, Ctrl-C, or any
         throw): the .bak is copied back over the INI, the restored file's SHA256
         is recomputed and compared to the pre-task hash.
            - MATCH   -> the .bak is deleted; the tree is provably clean.
            - MISMATCH-> the script ABORTS with a loud error and LEAVES the .bak
                         in place for manual recovery. It does NOT delete it.

      Net effect: the only DefaultEngine.ini state change is internal to this
      script and is reverted before the script returns. Per the initiative
      no-git rule, the implementer MUST NOT commit any DefaultEngine.ini change.

    MEASUREMENT MECHANISM — known limitation, READ THIS (flagged for T19 review):
      `stat net` and `stat unit` render to the in-viewport overlay; they do NOT
      print their KB/s / frame-ms numbers to stdout or the log. To make the
      capture file non-empty with real numbers this script ALSO:
        - enables `-LogCmds="LogNet Verbose, LogNetTraffic Verbose"` so per-bunch
          byte counts land in the log/stdout, and
        - starts the CSV profiler (`csvprofile start` ... `csvprofile stop`),
          whose Net category emits parseable InBytes/OutBytes/Ping columns to
          Saved/Profiling/CSV/*.csv (path echoed into the capture file).
      The `-ExecCmds="stat unit, stat net"` is still issued per the dispatch so
      the overlay is on screen for any manual observation. See tools/lint/README.md
      "Bandwidth measurement" for the full rationale + the per-tier metric mapping.

.PARAMETER MaxClientRate
    The per-client server-side bandwidth cap (bytes/sec) to set ephemerally in
    [/Script/OnlineSubsystemUtils.IpNetDriver].MaxClientRate (+ MaxInternetClientRate)
    for this run. The project default-inherits 100000 from BaseEngine.ini (T18 §4).
    NOTE: this is the IpNetDriver class section (the project's concrete net driver),
    NOT the inert [/Script/Engine.GameNetDriver] DefName section the Backlog literal
    named — see the binding-section correction comment in the ephemeral-edit block.

.PARAMETER Topology
    One of the four T18-locked scenarios: 1x3, 3x1, 2x3, 6x1 (clients x LPs).

.PARAMETER NetworkProfile
    Link-emulation profile applied to the CLIENT side only (T20 extension):
      * none     (default) — no emulation; identical to T19 behaviour.
      * cellular — prepends UE's built-in net-emulation console commands
                   `NetEmulation.PktLag 150, NetEmulation.PktLagVariance 30,
                    NetEmulation.PktLoss 5, NetEmulation.PktIncomingLoss 5` to the
                   client `-ExecCmds` (BEFORE the JoinLocalPlayer/open commands).
                   Maps to the C.2 tier-3 cellular profile per
                   OGBrawlerNetworkModelResearch/arch/risks_and_plan.md §1
                   R-A1/R-A2/R-A3 (~150 ms RTT, 30 ms jitter, 5% loss BOTH ways).
                   See the $netEmuPrefix block for the two mechanism corrections vs
                   the dispatch literal (`NetEmulation.*` not `Net Pkt*`; + incoming
                   loss for the downlink). Server side stays un-emulated; the cap +
                   SHA256-restore core are unchanged.

.PARAMETER OutputPath
    File to write the captured run output to (server + per-client log tails +
    a run-header summary block). Created if absent; parent dir must exist.

.PARAMETER DurationSec
    Seconds to let the session run under load before teardown. Default 60.

.PARAMETER RepoRoot
    Repo root. Defaults to two levels above this script (tools/lint/..).

.PARAMETER EngineDir
    UE 5.6 source-build root. Default C:\dev\UnrealEngine (matches playtest_*.bat).

.PARAMETER Port
    Dedicated-server UDP port. Default 7777.

.EXAMPLE
    pwsh tools/lint/measure_bandwidth.ps1 -MaxClientRate 100000 -Topology 1x3 `
        -OutputPath impl/ramp_100000_1x3.txt -DurationSec 60

.OUTPUTS
    Exit 0 = run completed and INI restored+verified.
    Exit 1 = run error (INI still restored+verified in finally).
    Exit 2 = usage / IO error (no edit performed) OR — critically — restore
             verification FAILED and the .bak was left for manual recovery.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][int]$MaxClientRate,
    [Parameter(Mandatory)][ValidateSet('1x3', '3x1', '2x3', '6x1')][string]$Topology,
    [Parameter(Mandatory)][string]$OutputPath,
    [ValidateSet('none', 'cellular')][string]$NetworkProfile = 'none',
    [int]$DurationSec = 60,
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..' '..')).Path,
    [string]$EngineDir = 'C:\dev\UnrealEngine',
    [int]$Port = 7777
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Resolve + validate paths up front (exit 2 on any missing prerequisite — no
# edit has happened yet, so the tree is untouched).
# ---------------------------------------------------------------------------
$iniPath   = Join-Path $RepoRoot 'Config\DefaultEngine.ini'
$bakPath   = "$iniPath.measure_bandwidth.bak"
$project   = Join-Path $RepoRoot 'OGBrawlerUnreal.uproject'
$editorExe = Join-Path $EngineDir 'Engine\Binaries\Win64\UnrealEditor.exe'
$map       = '/Game/ThirdPerson/Maps/ThirdPersonMap'

foreach ($p in @($iniPath, $project, $editorExe)) {
    if (-not (Test-Path -LiteralPath $p)) {
        Write-Error "Required path not found: $p"
        exit 2
    }
}

# Topology -> (clients, localPlayersPerClient).
$topoMap = @{
    '1x3' = @{ Clients = 1; Lps = 3; Tier = 'Tier 1'; Axis = 'densest' }
    '3x1' = @{ Clients = 3; Lps = 1; Tier = 'Tier 1'; Axis = 'most-connections' }
    '2x3' = @{ Clients = 2; Lps = 3; Tier = 'Tier 2'; Axis = 'densest' }
    '6x1' = @{ Clients = 6; Lps = 1; Tier = 'Tier 2'; Axis = 'most-connections' }
}
$topo    = $topoMap[$Topology]
$nClients = $topo.Clients
$nLps     = $topo.Lps

# A run-scoped scratch dir for per-process logs (under Saved so it's git-ignored).
$runStamp = (Get-Date -Format 'yyyyMMdd_HHmmss')
$runDir   = Join-Path $RepoRoot ("Saved\BandwidthRuns\{0}_{1}_{2}" -f $MaxClientRate, $Topology, $runStamp)
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

# Stat/exec commands. `stat net`/`stat unit` are the dispatch-prescribed overlay
# stats; the CSV profiler is what actually persists numbers to a parseable file.
$execCmds = 'stat unit, stat net, csvprofile start'
$logCmds  = 'LogNet Verbose, LogNetTraffic Verbose'

# -----------------------------------------------------------------------------
# T20 cellular link-emulation prefix (CLIENT side only). When -NetworkProfile is
# 'cellular', UE's built-in net-emulation console commands are PREPENDED to every
# client's -ExecCmds (ahead of the JoinLocalPlayer/open commands) so the wire rates,
# ping, jitter, and packet-loss counters reflect a ~150 ms RTT / 30 ms jitter / 5%
# loss cellular link. Source: OGBrawlerNetworkModelResearch/arch/risks_and_plan.md
# §1 R-A1/R-A2/R-A3 (C.2 tier-3 cellular profile: 150 ms RTT, 30 ms jitter, 5% loss).
#
# ⚠ TWO MECHANISM CORRECTIONS vs the Backlog/dispatch literal `Net PktLag=150 ...`
# (T20 full pass — empirically validated; see impl_notes_phase2a_task20.md §Corrections):
#
#   (1) `NetEmulation.PktLag 150` (etc.), NOT the UE4-era `Net PktLag=150` form. The
#       `Net Pkt*` exec routes through UNetDriver::Exec and needs a LIVE net driver;
#       it fires from the client's frame-1 -ExecCmds ~30 frames BEFORE the connection
#       exists (and for the multi-LP densest topologies the client boots STANDALONE,
#       which has no client net driver until the `open` travel) -> it silently no-ops.
#       A validation 1x3 cellular run with the literal form measured 0% loss / 10 ms
#       ping (identical to LAN) -> proof it was inert. The modern `NetEmulation.*`
#       console commands instead store into the engine-global PersistentPacketSimulation-
#       Settings (DO_ENABLE_NET_TEST dev builds), which UNetDriver re-applies to EVERY
#       connection created later (NetDriver.cpp ~L648 ApplyPersistentPacketEmulation-
#       Settings) — so emulation set at frame 1 survives the standalone->open travel.
#       ParseSettings is additive (each token sets only its field), so the knobs are
#       issued as separate commands.
#
#   (2) `PktLag`/`PktLoss` are OUTGOING-only (UNetConnection send path, NetConnection.cpp
#       ~L2517/L2553), i.e. on a CLIENT they emulate the UPLINK (client->server) only.
#       The task must measure the cellular DOWNSTREAM ceiling (the bandwidth-heavy
#       server->client broadcast), which an outgoing-only profile leaves pristine. So a
#       4th knob `NetEmulation.PktIncomingLoss 5` (receive path, NetConnection.cpp ~L2993)
#       degrades the DOWNLINK by 5% too — making the "effective downstream gap" (what the
#       server sent vs what arrived) measurable. Lag is kept ONE-WAY (outgoing PktLag=150)
#       so RTT stays ~150 ms per R-A1 rather than doubling; loss is applied to BOTH
#       directions at 5% each per R-A3 (one bad cellular link, both directions, modelled
#       entirely client-side — the dedicated server stays on a clean wired link).
#
# UE units: lag/variance in ms, loss in percent. The ephemeral-cap + SHA256-restore
# safety core is UNTOUCHED by this profile (server side and INI path unchanged).
$netEmuPrefix = ''
if ($NetworkProfile -eq 'cellular') {
    $netEmuPrefix = 'NetEmulation.PktLag 150, NetEmulation.PktLagVariance 30, NetEmulation.PktLoss 5, NetEmulation.PktIncomingLoss 5, '
}
Write-Host ("[measure_bandwidth] network profile = {0}{1}" -f $NetworkProfile,
    $(if ($netEmuPrefix) { " (client ExecCmds prefix: '$($netEmuPrefix.TrimEnd(', '))')" } else { '' }))

$processes = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()

# ---------------------------------------------------------------------------
# Pre-task snapshot — capture hash + write the .bak BEFORE touching the file.
# ---------------------------------------------------------------------------
$preHash        = (Get-FileHash -Algorithm SHA256 -LiteralPath $iniPath).Hash
$originalContent = Get-Content -Raw -LiteralPath $iniPath
[System.IO.File]::WriteAllText($bakPath, $originalContent)

# Sanity: the .bak we just wrote must hash-match the original.
$bakHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $bakPath).Hash
if ($bakHash -ne $preHash) {
    Write-Error "Backup write failed integrity check (.bak hash != original). Aborting before any edit. Backup left at: $bakPath"
    exit 2
}

Write-Host ("[measure_bandwidth] pre-task SHA256 = {0}" -f $preHash)
Write-Host ("[measure_bandwidth] backup written  = {0}" -f $bakPath)
Write-Host ("[measure_bandwidth] cap={0} topology={1} ({2}x{3} LPs, {4} {5}) duration={6}s" -f `
        $MaxClientRate, $Topology, $nClients, $nLps, $topo.Tier, $topo.Axis, $DurationSec)

# Snapshot the CSV-profiler dir BEFORE launch so we can identify *this run's* CSVs
# afterwards (each process writes Profile(<stamp>).csv into the shared project
# Saved/Profiling/CSV; the new files post-run are ours).
$csvDir = Join-Path $RepoRoot 'Saved\Profiling\CSV'
$csvBefore = @()
if (Test-Path -LiteralPath $csvDir) {
    $csvBefore = @(Get-ChildItem -LiteralPath $csvDir -Filter '*.csv' -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty Name)
}

$runError = $null
try {
    # -----------------------------------------------------------------------
    # Ephemeral edit: append a [/Script/OnlineSubsystemUtils.IpNetDriver] section
    # carrying the cap.
    #
    # ⚠ BINDING-SECTION CORRECTION (T19 full pass, 2026-06-24): the Backlog/
    # dispatch literal said edit [/Script/Engine.GameNetDriver], and the baseline
    # phase wrote that section — but it is INERT. There is no UGameNetDriver class;
    # "GameNetDriver" is only a NetDriverDefinitions *DefName*. The project's
    # concrete driver is UIpNetDriver, whose MaxClientRate UPROPERTY(Config) is read
    # from [/Script/OnlineSubsystemUtils.IpNetDriver] (T18 §4 documented this as the
    # effective-settings section). Empirical proof the old section did not bind: a
    # cap=15000 1x3 run still delivered ~40 KB/s downstream (8x over the supposed
    # cap). Writing the IpNetDriver section makes the cap actually bind. The project
    # ships NO such section (T18 §4), so we append; if one is ever found we ABORT
    # rather than guess at merge semantics (the finally block still restores).
    # -----------------------------------------------------------------------
    if ($originalContent -match '(?m)^\s*\[/Script/OnlineSubsystemUtils\.IpNetDriver\]') {
        throw "DefaultEngine.ini already has a [/Script/OnlineSubsystemUtils.IpNetDriver] section; refusing to guess merge semantics. Edit the script's edit logic deliberately before re-running."
    }

    $section = @"

; === measure_bandwidth.ps1 EPHEMERAL EDIT — DO NOT COMMIT ===
; Written $runStamp for a Phase 2a T19 cap-ramp run; reverted at end of script.
[/Script/OnlineSubsystemUtils.IpNetDriver]
MaxClientRate=$MaxClientRate
MaxInternetClientRate=$MaxClientRate
; === end ephemeral edit ===
"@
    [System.IO.File]::WriteAllText($iniPath, $originalContent + $section)
    Write-Host ("[measure_bandwidth] ephemeral [/Script/OnlineSubsystemUtils.IpNetDriver] MaxClientRate={0} appended." -f $MaxClientRate)

    # -----------------------------------------------------------------------
    # Launch the dedicated server (headless: -nullrhi -nosound -unattended).
    # -----------------------------------------------------------------------
    $serverLog = Join-Path $runDir 'server.log'
    $serverOut = Join-Path $runDir 'server.stdout.txt'
    $serverArgs = @(
        "`"$project`"", $map, '-server', "-port=$Port",
        '-log', '-stdout', '-nullrhi', '-nosound', '-unattended',
        "-abslog=`"$serverLog`"",
        "-ExecCmds=`"$execCmds`"", "-LogCmds=`"$logCmds`""
    )
    Write-Host "[measure_bandwidth] launching dedicated server ..."
    $proc = Start-Process -FilePath $editorExe -ArgumentList $serverArgs -PassThru `
        -RedirectStandardOutput $serverOut -WindowStyle Hidden
    $processes.Add($proc)

    # Let the server bind before clients dial in.
    Start-Sleep -Seconds 8

    # -----------------------------------------------------------------------
    # Launch N clients. Multi-LP fan-out: extra local players are added on each
    # client via the OGBrawlerMultiPlayerPerClient entry point `JoinLocalPlayer`
    # — a UFUNCTION(Exec) on AOGBrawlerPlayerController (shipped 2026-05-29) that
    # wraps UGameplayStatics::CreatePlayer AND reinforces the splitscreen-disable
    # at join time (the documented UE-bug workaround). For an N-LP topology we
    # invoke it exactly (N-1) times (LP0 is the primary, already present).
    #
    # The generic engine `debug.CreatePlayer` console command is NOT used: it is
    # no-opped by OGBrawlerUEGameMode::BeginPlay's SetForceDisableSplitscreen(true)
    # guard (the in-mode CreatePlayer block is `if(false)`), which is exactly why
    # the baseline 1x3 capture degraded to an effective 1x1.
    #
    # Mechanism (empirically determined — see impl_notes_phase2a_task19_full.md):
    #   * SINGLE-LP clients (nLps == 1: the 3x1 / 6x1 most-connections topologies)
    #     connect directly via the `127.0.0.1:<port>` command-line URL. No
    #     JoinLocalPlayer needed.
    #   * MULTI-LP clients (nLps > 1: the 1x3 / 2x3 densest topologies) use the
    #     canonical couch-co-op-then-join flow. Issuing JoinLocalPlayer through a
    #     directly-connecting client's startup -ExecCmds does NOT work: the
    #     deferred ExecCmds fire on frame 1, ~30 frames BEFORE the server has
    #     replicated the local APlayerController, so the Exec routes to a null PC
    #     and no-ops (verified: csvprofile started frame 1 @ t+0ms, "Welcomed by
    #     server" frame 32 @ t+475ms). Stdin injection also fails (a -game client
    #     does not read console commands from stdin). Instead the client BOOTS
    #     STANDALONE to the local map (so PC0 exists at frame 1), runs
    #     JoinLocalPlayer (N-1) times to create the extra ULocalPlayers
    #     synchronously, then `open 127.0.0.1:<port>` travels to the dedicated
    #     server carrying all N LPs — UE issues a Join for LP0 + a JoinSplit
    #     (UChildConnection) per secondary LP, producing the 1-connection-carries-
    #     N-characters densest topology on the wire.
    # -----------------------------------------------------------------------
    $clientOuts = @()
    for ($c = 0; $c -lt $nClients; $c++) {
        $clientLog = Join-Path $runDir "client$c.log"
        $clientOut = Join-Path $runDir "client$c.stdout.txt"
        $clientOuts += $clientOut

        if ($nLps -le 1) {
            # Single-LP client: direct connect via command-line URL.
            # $netEmuPrefix is empty for -NetworkProfile none (T19 behaviour).
            $clientExec = "$netEmuPrefix$execCmds"
            $clientUrl  = "127.0.0.1:$Port"
        }
        else {
            # Multi-LP client: boot standalone to the map, create (N-1) extra
            # local players, THEN open to the server (all on frame 1, in order).
            # The cellular emulation prefix (if any) goes FIRST — before the
            # JoinLocalPlayer/open commands — per the T20 spec.
            $clientExec = "$netEmuPrefix$execCmds"
            for ($lp = 1; $lp -lt $nLps; $lp++) {
                $clientExec += ', JoinLocalPlayer'
            }
            $clientExec += ", open 127.0.0.1:$Port"
            $clientUrl   = $map
        }

        $clientArgs = @(
            "`"$project`"", $clientUrl, '-game',
            '-log', '-stdout', '-windowed', '-ResX=640', '-ResY=360',
            '-nosound',
            "-abslog=`"$clientLog`"",
            "-ExecCmds=`"$clientExec`"", "-LogCmds=`"$logCmds`""
        )
        Write-Host ("[measure_bandwidth] launching client {0}/{1} ({2} LP[s], url={3}) ..." -f ($c + 1), $nClients, $nLps, $clientUrl)
        $proc = Start-Process -FilePath $editorExe -ArgumentList $clientArgs -PassThru `
            -RedirectStandardOutput $clientOut -WindowStyle Minimized
        $processes.Add($proc)
        Start-Sleep -Seconds 3
    }

    # -----------------------------------------------------------------------
    # Hold under load for DurationSec, then force teardown. The load budget is a
    # hard ceiling so a cold shader-compile or a hung process can't wedge the
    # harness indefinitely.
    # -----------------------------------------------------------------------
    Write-Host ("[measure_bandwidth] holding for {0}s under load ..." -f $DurationSec)
    Start-Sleep -Seconds $DurationSec

    # Graceful close first (CloseMainWindow -> WM_CLOSE -> RequestEngineExit) so the
    # windowed CLIENT processes run their shutdown path and CsvProfiler::EndCapture
    # WRITES the per-client CSV (the authoritative source — see extractor note: the
    # headless -nullrhi server has no window and does NOT flush on force-kill, so we
    # extract from the clients). Wait long enough for all (up to 6) clients to flush.
    Write-Host "[measure_bandwidth] tearing down processes (graceful close + flush) ..."
    foreach ($p in $processes) {
        if (-not $p.HasExited) {
            try { $p.CloseMainWindow() | Out-Null } catch {}
        }
    }
    Start-Sleep -Seconds 10
    foreach ($p in $processes) {
        if (-not $p.HasExited) {
            try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
        }
    }

    # -----------------------------------------------------------------------
    # Collect THIS run's CSV-profiler files into $runDir (the new *.csv that
    # appeared since the pre-launch snapshot). Makes each run self-contained for
    # later metric extraction. The WIDE CSVs are the windowed clients (full RHI
    # stat columns) and are the authoritative source — they flush CsvProfiler on
    # graceful CloseMainWindow shutdown. The narrow -nullrhi server CSV (if present)
    # does NOT reliably flush on force-kill, so extraction uses the clients:
    # Replication/InRate = per-client downstream, OutRate = per-client upstream.
    # -----------------------------------------------------------------------
    $runCsvNames = @()
    if (Test-Path -LiteralPath $csvDir) {
        $runCsvs = @(Get-ChildItem -LiteralPath $csvDir -Filter '*.csv' -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -notin $csvBefore })
        foreach ($cf in $runCsvs) {
            try {
                Copy-Item -LiteralPath $cf.FullName -Destination (Join-Path $runDir $cf.Name) -Force
                $runCsvNames += $cf.Name
            }
            catch {}
        }
    }
    Write-Host ("[measure_bandwidth] collected {0} CSV profiler file(s) for this run." -f $runCsvNames.Count)

    # -----------------------------------------------------------------------
    # Assemble the capture file: a header block + tails of every process output.
    # -----------------------------------------------------------------------
    $header = @"
# ===========================================================================
# Phase 2a T19 bandwidth measurement capture
# ===========================================================================
# Generated      : $runStamp
# MaxClientRate  : $MaxClientRate B/s  (ephemeral [/Script/OnlineSubsystemUtils.IpNetDriver])
# Topology       : $Topology  ($nClients client[s] x $nLps LP[s], $($topo.Tier) $($topo.Axis))
# NetworkProfile : $NetworkProfile$(if ($netEmuPrefix) { "  (client NetEmulation.PktLag 150 / PktLagVariance 30 / PktLoss 5 / PktIncomingLoss 5 — C.2 tier-3 cellular, both-way 5% loss, ~150ms RTT)" })
# DurationSec    : $DurationSec
# Pre-task SHA256: $preHash
# Run scratch    : $runDir  (this run's CSV profiler files copied here)
# Run CSV files  : $($runCsvNames -join ', ')
#
# NOTE: `stat net` / `stat unit` are viewport-overlay stats and do NOT print to
# stdout. Per-client KB/s come from the CSV profiler Net category (above) and/or
# the LogNetTraffic Verbose lines tailed below. See tools/lint/README.md.
# ===========================================================================

"@
    Set-Content -LiteralPath $OutputPath -Value $header -Encoding utf8

    function Add-Tail([string]$label, [string]$file) {
        Add-Content -LiteralPath $OutputPath -Value "`n----- $label : $file -----" -Encoding utf8
        if (Test-Path -LiteralPath $file) {
            $lines = Get-Content -LiteralPath $file -Tail 400 -ErrorAction SilentlyContinue
            Add-Content -LiteralPath $OutputPath -Value $lines -Encoding utf8
        }
        else {
            Add-Content -LiteralPath $OutputPath -Value "(no output captured)" -Encoding utf8
        }
    }

    Add-Tail 'SERVER stdout' $serverOut
    Add-Tail 'SERVER log'    $serverLog
    for ($c = 0; $c -lt $nClients; $c++) {
        Add-Tail "CLIENT $c stdout" (Join-Path $runDir "client$c.stdout.txt")
        Add-Tail "CLIENT $c log"    (Join-Path $runDir "client$c.log")
    }
    Write-Host ("[measure_bandwidth] capture written -> {0}" -f $OutputPath)
}
catch {
    $runError = $_
    Write-Warning ("[measure_bandwidth] run error: {0}" -f $_.Exception.Message)
}
finally {
    # -----------------------------------------------------------------------
    # MANDATORY RESTORE — runs on success, error, or interrupt.
    # -----------------------------------------------------------------------
    # Best-effort: make sure nothing is still holding the INI / running.
    foreach ($p in $processes) {
        if ($p -and -not $p.HasExited) {
            try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
        }
    }

    if (Test-Path -LiteralPath $bakPath) {
        Copy-Item -LiteralPath $bakPath -Destination $iniPath -Force
        $postHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $iniPath).Hash
        if ($postHash -eq $preHash) {
            Remove-Item -LiteralPath $bakPath -Force
            Write-Host ("[measure_bandwidth] RESTORE OK — post-task SHA256 = {0} (matches pre-task); .bak deleted." -f $postHash)
        }
        else {
            Write-Error "[measure_bandwidth] RESTORE VERIFICATION FAILED. post=$postHash pre=$preHash. Config/DefaultEngine.ini may be CORRUPT. The backup has been LEFT IN PLACE for manual recovery: $bakPath"
            exit 2
        }
    }
    else {
        Write-Error "[measure_bandwidth] backup file missing at restore time ($bakPath); cannot verify INI state. Inspect Config/DefaultEngine.ini manually."
        exit 2
    }
}

if ($runError) { exit 1 }
exit 0
