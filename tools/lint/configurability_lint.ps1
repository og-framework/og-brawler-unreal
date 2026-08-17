#Requires -Version 7.0
# SPDX-License-Identifier: MPL-2.0
<#
.SYNOPSIS
    R-P1 configurability lint (Synthesis Addendum Correction 5 binding rule).

.DESCRIPTION
    Scans the C++ source tree for TimeConfig-governed tuning values that have been
    re-declared (a second source of truth) outside their TimeConfig home. Per
    proposal §11 + risks_and_plan §6.1, every tuning parameter named in the
    synthesis MUST live as a `TimeConfig` field and be read from there at runtime.
    This lint is the structural enforcement arm of that rule; the Catch2
    default-drift gate (TimeConfigDefaultsTest.cpp, Task 6) is the complementary
    runtime arm.

    DESIGN — "field-anchored" matching (user-approved 2026-06-20; rationale and the
    rejected alternatives are documented in tools/lint/README.md "Design decision"):
      A violation is a config FIELD NAME used as a NON-member lvalue assigned its
      magic literal, e.g.

          int32_t rollbackWindowTicks = 12;   // FLAGGED — second source of truth
          redundancyDepthTicks        = 5;     // FLAGGED

      Legitimate USES of the config are NOT flagged, because they go through a
      member access on a TimeConfig instance, which a negative lookbehind excludes:

          cfg.tickFrequency            = 60.0; // OK — configuring an instance
          config.rollbackWindowTicks   = ...;  // OK
          if (drift <= m_config.hardResyncThresholdTicks) ... // OK

    To keep the false-positive rate at zero this lint also strips // line comments,
    /* block */ comments, and "string"/'char' literals before matching, so docs or
    log strings that mention a field name or "100 Hz" are never flagged.

    KNOWN LIMITATION (by design): a keyword-free magic number — e.g. `if (depth > 12)`
    or `int32 dummy = 12;` — is NOT flagged. A bare-value lint cannot run clean on
    this tree (12/20/60/100/5/3 appear constantly in unrelated code: LatSegs = 12,
    MaxWalkSpeed = 100.f, StateBufferSize = 60, Jolt geometry, ...). Those cases are
    covered by code review and the default-drift gate. See README for the full
    discussion.

    THE BLACKLIST IS A CONTRACT: every PR that adds/changes a TimeConfig field MUST
    add/update a matching entry here in the same change. See tools/lint/README.md.

.PARAMETER RepoRoot
    Repository root. Defaults to two levels above this script (tools/lint/..).

.PARAMETER ScanRoots
    Top-level directories to scan (relative to RepoRoot). Defaults to Plugins, Source.

.EXAMPLE
    pwsh tools/lint/configurability_lint.ps1

.OUTPUTS
    Exit 0 = clean. Exit 1 = at least one violation. Exit 2 = usage / IO error.
#>
[CmdletBinding()]
param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..' '..')).Path,
    [string[]]$ScanRoots = @('Plugins', 'Source')
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# CONFIG_LITERAL_BLACKLIST
#
# Schema — one hashtable per TimeConfig field that has a synthesis-named default:
#   Field   : the TimeConfig field this entry guards (for humans / messages).
#   Pattern : a .NET regex applied to each comment/string-stripped source line.
#             Field-anchored: (?<![\w.>]) excludes member access (foo.field /
#             foo->field) and substrings (myfield); '=' (not '==') matches a
#             declaration/assignment, not a comparison.
#   Allowed : file LEAF names (case-insensitive) where the declaration is the
#             legitimate source of truth. Empty => flagged everywhere.
#   Message : printed on a hit; names the field and references R-P1.
#   Note    : (optional) rationale for any non-obvious Allowed entry.
#
# CONTRACT: adding a TimeConfig field => add an entry here in the same PR.
# ---------------------------------------------------------------------------
$Blacklist = @(
    @{
        Field   = 'predOffsetFloorTicks'
        # Netcode Fix A (Task 23): minimum floor for getPredictionOffsetTicks().
        # TimeConfig.h is the source of truth; TimeConfigDefaultsTest.cpp asserts
        # the default. NetworkTimeEstimator.cpp consumes it via member access
        # (m_config.predOffsetFloorTicks), which the (?<![\w.>]) lookbehind auto-excludes.
        Pattern = '(?<![\w.>])predOffsetFloorTicks\s*=\s*4\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'predOffsetFloorTicks default (4) must live only in TimeConfig; read config.predOffsetFloorTicks instead of re-declaring it (R-P1)'
    },
    # -----------------------------------------------------------------------
    # Outlier RTT rejection (og-netcode-v2-input-relay T26b). Five knobs on the
    # NetworkTimeEstimator plausibility gate. Every consumer reads them via
    # member access on the stored `const TimeConfig&` (m_config.rttOutlier*),
    # which the (?<![\w.>]) lookbehind auto-excludes — these entries guard
    # against a NEW non-member re-declaration of the tuning value, which is
    # exactly the shape a future "just hardcode the bound at the call site"
    # change would take.
    # -----------------------------------------------------------------------
    @{
        Field   = 'rttOutlierMultiplier'
        Pattern = '(?<![\w.>])rttOutlierMultiplier\s*=\s*4\.0\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'rttOutlierMultiplier default (4.0) must live only in TimeConfig; read config.rttOutlierMultiplier instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'rttOutlierMarginSeconds'
        Pattern = '(?<![\w.>])rttOutlierMarginSeconds\s*=\s*0\.030\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'rttOutlierMarginSeconds default (0.030 s) must live only in TimeConfig; read config.rttOutlierMarginSeconds instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'rttOutlierColdStartCeilingSeconds'
        Pattern = '(?<![\w.>])rttOutlierColdStartCeilingSeconds\s*=\s*0\.5\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'rttOutlierColdStartCeilingSeconds default (0.5 s) must live only in TimeConfig; read config.rttOutlierColdStartCeilingSeconds instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'rttOutlierConsecutiveLimit'
        Pattern = '(?<![\w.>])rttOutlierConsecutiveLimit\s*=\s*30\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'rttOutlierConsecutiveLimit default (30 — the escape hatch that lets a genuine RTT step through) must live only in TimeConfig; read config.rttOutlierConsecutiveLimit instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'rttOutlierLogWindowSamples'
        Pattern = '(?<![\w.>])rttOutlierLogWindowSamples\s*=\s*600\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'rttOutlierLogWindowSamples default (600) must live only in TimeConfig; read config.rttOutlierLogWindowSamples instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'rollbackWindowTicks'
        Pattern = '(?<![\w.>])rollbackWindowTicks\s*=\s*12\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'rollbackWindowTicks default (12) must live only in TimeConfig; read config.rollbackWindowTicks instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'rollbackWindowHardCap'
        Pattern = '(?<![\w.>])rollbackWindowHardCap\s*=\s*20\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'rollbackWindowHardCap default (20) must live only in TimeConfig; read config.rollbackWindowHardCap instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'redundancyDepthTicks'
        # Catch the field and any sibling redundancyDepth* spelling assigned 3 or 5.
        Pattern = '(?<![\w.>])redundancyDepth\w*\s*=\s*[35]\b'
        # T7 adds the FInputRedundancyBundle header; allow both candidate basenames
        # (a dedicated header vs. SyncedSimulationStateBuffer.h) until T7 fixes the name.
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp', 'InputRedundancyBundle.h', 'FInputRedundancyBundle.h')
        Message = 'redundancyDepthTicks default (5 @100Hz interim / 3 @60Hz target) must live only in TimeConfig; read config.redundancyDepthTicks instead of re-declaring it (R-P1)'
    },
    # ⛔ RETIRED (item 63 / RN-13, 2026-08-16): the relay-ring retention-depth
    # lint entry that used to live here is gone along with the field it guarded
    # (its old identifier is on record in RN-13, ReviewNotes.md). Item 34's
    # bare-C1 flush-on-poll replaced the write path it sized. If a redundancy-
    # under-flush successor ships (gated on item 40), it is a NEW, differently-
    # named field and needs its own entry, not this one restored.
    @{
        Field   = 'relayDelayFloorTicks'
        # og-netcode-v2-input-relay T11: the session-scoped minimum effective input
        # delay (RelayDelaySpectrumDesign.md §6/§10). Ships at 0 (degenerate), tuned
        # by playtest — exactly the shape R-P1 exists to keep out of call sites.
        # Field-anchored, so only a non-member `relayDelayFloorTicks = 0` is flagged;
        # every consumer reads it as cfg.relayDelayFloorTicks (member access,
        # auto-excluded by the (?<![\w.>]) lookbehind), and the composition root's
        # ini override writes it through SimulationManager::setRelayDelayFloorTicks
        # rather than re-declaring a default.
        # The A5 CEILING (44 = ClientInputDelayLine capacity - rollbackWindowHardCap)
        # is intentionally not guarded here: it is derived from two other config
        # quantities at the clamp site, not written as a tuning literal anywhere.
        Pattern = '(?<![\w.>])relayDelayFloorTicks\s*=\s*0\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'relayDelayFloorTicks default (0 — degenerate, the floor lever ships off) must live only in TimeConfig; read config.relayDelayFloorTicks instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'correctionRotationK'
        # og-netcode-v2-input-relay T39: how many characters' correction-state
        # buffers SimulationNetSync::sendCorrectionAll writes per tick, round-robin
        # (design_task38_input_first_replication.md §5.4). Ships at 2 — every-frame
        # at two characters, 20 Hz at six — and is exactly the shape R-P1 exists to
        # keep out of call sites: a cadence number that a future "just write two of
        # them here" edit would re-declare locally.
        # Field-anchored, so only a NON-member `correctionRotationK = 2` is flagged;
        # every consumer reads it as cfg.correctionRotationK / m_timeConfig
        # .correctionRotationK (member access, auto-excluded by the (?<![\w.>])
        # lookbehind), and the composition root's ini override writes it through
        # SimulationManager::setCorrectionRotationK rather than re-declaring a
        # default.
        # The CLAMP BOUNDS (correctionRotation::kMinK/kMaxK = 1/16) are
        # intentionally not guarded here — R-P1 guards tuning values, not the
        # guard rails around them, the same call already made for
        # relayedInputRing::kMaxDepth.
        Pattern = '(?<![\w.>])correctionRotationK\s*=\s*2\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'correctionRotationK default (2 — every-frame at two characters, the archived-baseline-preserving choice) must live only in TimeConfig; read config.correctionRotationK instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'resimTriggerPolicy'
        # og-netcode-v2-input-relay item 45: which landed corrections open the resim
        # gate (design_task43_resim_gate_fix.md §3 candidate D). Enum-valued, so the
        # pattern anchors on the ENUMERATOR PREFIX rather than on a literal — the
        # harnessMode / sn1BroadcastPolicy shape — which also means it catches a
        # re-declaration of EITHER value rather than only of the shipped default.
        # That is the right strength here: this field's contract is "the default is
        # the legacy gate", and a local `resimTriggerPolicy = OnDisagreement` outside
        # TimeConfig would enable item 46's storm scenario in a build nobody thinks
        # they changed.
        # Field-anchored, so only a NON-member assignment is flagged; every consumer
        # reads it as cfg.resimTriggerPolicy / m_timeConfig.resimTriggerPolicy (member
        # access, auto-excluded by the (?<![\w.>]) lookbehind), the caches hold a
        # pushed copy in `m_resimTriggerPolicy` (underscore-prefixed, also excluded)
        # seeded from `TimeConfig{}.resimTriggerPolicy`, and the composition root's
        # ini override writes it through SimulationManager::setResimTriggerPolicy.
        Pattern = '(?<![\w.>])resimTriggerPolicy\s*=\s*(TimeConfig::)?ResimTriggerPolicy::'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'resimTriggerPolicy default (ResimTriggerPolicy::FrontierExact — the LEGACY gate, item 46 owns the flip) must live only in TimeConfig; read config.resimTriggerPolicy instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'tickFrequency'
        Pattern = '(?<![\w.>])tickFrequency\s*=\s*(60|100)\b'
        # NetworkTimeEstimator.cpp legitimately consumes the configured value. The
        # async-physics derive site (config.tickFrequency = 1.0/GetAsyncDeltaTime())
        # is member-access and auto-excluded; T14's Stage 2 audit adds its owning file
        # here only if that audit surfaces a literal Hz constant.
        Allowed = @('TimeConfig.h', 'NetworkTimeEstimator.cpp')
        Message = 'tickFrequency (60 ratified target / 100 interim) must be configured via TimeConfig and derived from solver->GetAsyncDeltaTime(); do not re-declare the Hz literal (R-P1)'
    },
    @{
        Field   = 'hardResyncThresholdTicks'
        # Old-value-must-not-reappear guard: bumped 15 -> 21 for R-D3 (failsafe must
        # stay strictly > rollbackWindowHardCap=20). Allowed is EMPTY so a revert to
        # 15 is caught even inside TimeConfig.h. Test fixtures that set
        # `cfg.hardResyncThresholdTicks = 15` are member-access and auto-excluded.
        Pattern = '(?<![\w.>])hardResyncThresholdTicks\s*=\s*15\b'
        Allowed = @()
        Message = 'hardResyncThresholdTicks old default 15 must not reappear — current default is 21 (R-P1 / R-D3)'
    },

    # -----------------------------------------------------------------------
    # Product-target session constants (og-netcode-v1-impl Phase 2a, Task 18).
    #
    # NOT TimeConfig fields — these live in the og-brawler core header
    # SessionConstants.h (namespace og::brawler::session) and define the ratified
    # player-count envelope (Tier 1 = 3, Tier 2 = 6, up to 3 LPs/client). Same
    # "single source of truth" rule as the TimeConfig entries above: the magic
    # 3/6 player-count literals must be read from SessionConstants.h, never
    # re-declared in game/UE/test code. Member-access reads
    # (og::brawler::session::maxPlayersPerServer) and qualified uses are auto-
    # excluded by the (?<![\w.>]) lookbehind; only a non-member lvalue assigned
    # the magic literal is flagged. CONTRACT: a future server-session gate that
    # legitimately re-houses one of these values adds its file leaf to Allowed in
    # the same change. A test fixture that exercises a literal cap adds itself too.
    # -----------------------------------------------------------------------
    @{
        Field   = 'maxPlayersPerServer'
        Pattern = '(?<![\w.>])maxPlayersPerServer\s*=\s*3\b'
        Allowed = @('SessionConstants.h')
        Message = 'maxPlayersPerServer Tier 1 default (3) must live only in SessionConstants.h; read og::brawler::session::maxPlayersPerServer instead of re-declaring it (R-P1 / Phase 2a)'
    },
    @{
        Field   = 'maxPlayersPerServerTier2'
        Pattern = '(?<![\w.>])maxPlayersPerServerTier2\s*=\s*6\b'
        Allowed = @('SessionConstants.h')
        Message = 'maxPlayersPerServerTier2 Tier 2 stretch target (6) must live only in SessionConstants.h; read og::brawler::session::maxPlayersPerServerTier2 instead of re-declaring it (R-P1 / Phase 2a)'
    },
    @{
        Field   = 'maxLocalPlayersPerClient'
        Pattern = '(?<![\w.>])maxLocalPlayersPerClient\s*=\s*3\b'
        Allowed = @('SessionConstants.h')
        Message = 'maxLocalPlayersPerClient (3 LPs/client) must live only in SessionConstants.h; read og::brawler::session::maxLocalPlayersPerClient instead of re-declaring it (R-P1 / Phase 2a)'
    },
    @{
        # Generic catch for hardcoded player-count caps that DON'T use the
        # SessionConstants names above (e.g. player_count = 3, maxPlayers = 6).
        # Overshoot is intentional per the Task 18 spec — any false positive is
        # surfaced for manual review rather than silently passing. The name
        # alternation does NOT match the SessionConstants fields (they have
        # suffixes after 'maxPlayers'), so the source-of-truth header is not hit.
        Field   = 'player-count literal (generic)'
        Pattern = '(?<![\w.>])(player_count|playerCount|PlayerCount|maxPlayers|numPlayers|NumPlayers)\s*[=:]\s*[36]\b'
        Allowed = @('SessionConstants.h')
        Message = 'hardcoded player-count literal (3 = Tier 1, 6 = Tier 2) must read from og::brawler::session in SessionConstants.h, not a magic 3/6 (R-P1 / Phase 2a)'
    },

    # -----------------------------------------------------------------------
    # Projectile pool size (OGBrawlerHadouken Phase 2, Task 12 — R-P1 config move).
    #
    # NOT a TimeConfig field — the runtime pool size lives in the og-brawler core
    # header BrawlerProjectileSimulation.h as brawlerProjectileSimulation::StaticData
    # ::projectilePoolSize (default 3), the single wire-affecting sizing knob read at
    # runtime via sd.projectilePoolSize. The compile-time CAPACITY constant
    # kMaxProjectilePoolSize (also 3) is intentionally NOT guarded here — the R-P1 lint
    # guards tuning values, not template-instantiation/array/SIM_VECTOR capacities.
    # Member-access reads (sd.projectilePoolSize) and the runtime-cap test fixture
    # (sd.projectilePoolSize = 1) are auto-excluded by the (?<![\w.>]) lookbehind and the
    # value anchor; only a non-member lvalue assigned the magic default 3 is flagged.
    # CONTRACT: if T15 re-houses this value onto simulatableBrawler::StaticData under a
    # different name, that file leaf is added to Allowed in the same change.
    # -----------------------------------------------------------------------
    @{
        Field   = 'projectilePoolSize'
        Pattern = '(?<![\w.>])projectilePoolSize\s*=\s*3\b'
        Allowed = @('BrawlerProjectileSimulation.h')
        Message = 'projectilePoolSize default (3) must live only in BrawlerProjectileSimulation.h StaticData; read sd.projectilePoolSize instead of re-declaring it (R-P1)'
    },

    # -----------------------------------------------------------------------
    # Pre-existing TimeConfig fields that had no blacklist entry (og-netcode-v2
    # Task 1). These were legal before the self-lint below existed; the self-lint
    # requires EVERY TimeConfig field to be represented, so they are added here.
    # All are member-access reads at their consumer sites, which the (?<![\w.>])
    # lookbehind auto-excludes — these entries guard against a NEW non-member
    # re-declaration, not against existing code.
    # -----------------------------------------------------------------------
    @{
        Field   = 'rttSmoothingAlpha'
        Pattern = '(?<![\w.>])rttSmoothingAlpha\s*=\s*0\.15\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'rttSmoothingAlpha default (0.15) must live only in TimeConfig; read config.rttSmoothingAlpha instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'jitterSmoothingAlpha'
        Pattern = '(?<![\w.>])jitterSmoothingAlpha\s*=\s*0\.15\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'jitterSmoothingAlpha default (0.15) must live only in TimeConfig; read config.jitterSmoothingAlpha instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'jitterMultiplier'
        Pattern = '(?<![\w.>])jitterMultiplier\s*=\s*2\.0\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'jitterMultiplier default (2.0) must live only in TimeConfig; read config.jitterMultiplier instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'softDriftThresholdTicks'
        Pattern = '(?<![\w.>])softDriftThresholdTicks\s*=\s*3\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'softDriftThresholdTicks default (3) must live only in TimeConfig; read config.softDriftThresholdTicks instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'gradualCorrectionRate'
        Pattern = '(?<![\w.>])gradualCorrectionRate\s*=\s*4\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'gradualCorrectionRate default (4) must live only in TimeConfig; read config.gradualCorrectionRate instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'minTicksBeforeDriftCheck'
        Pattern = '(?<![\w.>])minTicksBeforeDriftCheck\s*=\s*60\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'minTicksBeforeDriftCheck default (60) must live only in TimeConfig; read config.minTicksBeforeDriftCheck instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'harnessMode'
        Pattern = '(?<![\w.>])harnessMode\s*=\s*(TimeConfig::)?HarnessMode::'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp', 'DeterminismHarness.h')
        Message = 'harnessMode default (HarnessMode::Production) must live only in TimeConfig; read config.harnessMode instead of re-declaring it (R-P1)'
    },

    # -----------------------------------------------------------------------
    # C.2 tiered input delay — og-netcode-v2 Task 1 (Stage 5 fields).
    #
    # ConnectionTierTable.h (T4) is on the Allowed list for the six tier fields it
    # consumes. Belt-and-braces: T4 reads them via member access on the stored
    # `const TimeConfig&`, which the lookbehind already excludes — the Allowed
    # entry exists so a future in-header default/constant lives somewhere legal
    # rather than being silently forbidden. See Backlog T4 acceptance criteria.
    # -----------------------------------------------------------------------
    # The dedicated no-tier-baseline field's lint entry was RETIRED with the
    # field itself (og-netcode-v2-input-relay item 62 / RN-12, 2026-08-16; its
    # old identifier is on record in RN-12, ReviewNotes.md): the no-tier
    # fallback is now `rttTierInputDelays[kMaxConnectionTierIndex]`, already
    # covered by the entry below — there is no second field to lint.
    @{
        Field   = 'rttTierBoundariesMs'
        # Array field: the declaration carries a [4] extent between the name and
        # the '=', so the pattern tolerates an optional bracketed extent and
        # anchors on the magic brace-init list.
        Pattern = '(?<![\w.>])rttTierBoundariesMs\s*(\[[^\]]*\])?\s*=\s*\{\s*30\s*,\s*80\s*,\s*150\s*,\s*999\s*\}'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp', 'ConnectionTierTable.h')
        Message = 'rttTierBoundariesMs default ({30,80,150,999}) must live only in TimeConfig; read config.rttTierBoundariesMs instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'rttTierInputDelays'
        Pattern = '(?<![\w.>])rttTierInputDelays\s*(\[[^\]]*\])?\s*=\s*\{\s*1\s*,\s*2\s*,\s*3\s*,\s*4\s*\}'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp', 'ConnectionTierTable.h')
        Message = 'rttTierInputDelays default ({1,2,3,4}) must live only in TimeConfig; read config.rttTierInputDelays instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'rttTierRollbackCeilings'
        Pattern = '(?<![\w.>])rttTierRollbackCeilings\s*(\[[^\]]*\])?\s*=\s*\{\s*6\s*,\s*9\s*,\s*12\s*,\s*20\s*\}'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp', 'ConnectionTierTable.h')
        Message = 'rttTierRollbackCeilings default ({6,9,12,20}) must live only in TimeConfig; read config.rttTierRollbackCeilings instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'tierHysteresisMs'
        Pattern = '(?<![\w.>])tierHysteresisMs\s*=\s*10\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp', 'ConnectionTierTable.h')
        Message = 'tierHysteresisMs default (10) must live only in TimeConfig; read config.tierHysteresisMs instead of re-declaring it (R-P1 / R-A2)'
    },
    @{
        Field   = 'tierMinDwellTicks'
        Pattern = '(?<![\w.>])tierMinDwellTicks\s*=\s*60\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp', 'ConnectionTierTable.h')
        Message = 'tierMinDwellTicks default (60) must live only in TimeConfig; read config.tierMinDwellTicks instead of re-declaring it (R-P1 / R-A2)'
    },
    @{
        Field   = 'muteEchoOnDegradedTier'
        # No consumer until optional task T15; entry exists so T15 cannot hardcode it.
        Pattern = '(?<![\w.>])muteEchoOnDegradedTier\s*=\s*true\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp', 'ConnectionTierTable.h')
        Message = 'muteEchoOnDegradedTier default (true) must live only in TimeConfig; read config.muteEchoOnDegradedTier instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'lanZeroDelayOverride'
        Pattern = '(?<![\w.>])lanZeroDelayOverride\s*=\s*false\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp', 'ConnectionTierTable.h')
        Message = 'lanZeroDelayOverride default (false) must live only in TimeConfig; read config.lanZeroDelayOverride instead of re-declaring it (R-P1)'
    },

    # -----------------------------------------------------------------------
    # Stage 4 observability fields — og-netcode-v2 Task 1.
    #
    # NONE of these has a runtime consumer yet. That is precisely why they are
    # linted now: the Stage 4 initiative must read them from TimeConfig rather
    # than hardcoding the literal at the use site it is about to write.
    # For the enum-valued fields the pattern anchors on the ENUM TYPE PREFIX
    # rather than a specific enumerator, so ANY re-declaration of the field with
    # an enum default is caught, not just the current default.
    # -----------------------------------------------------------------------
    @{
        Field   = 'sn1BroadcastPolicy'
        Pattern = '(?<![\w.>])sn1BroadcastPolicy\s*=\s*(TimeConfig::)?Sn1BroadcastPolicy::'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'sn1BroadcastPolicy default (Sn1BroadcastPolicy::RttTiered) must live only in TimeConfig; read config.sn1BroadcastPolicy instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'sn1IdleBroadcastIntervalTicks'
        Pattern = '(?<![\w.>])sn1IdleBroadcastIntervalTicks\s*=\s*6\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'sn1IdleBroadcastIntervalTicks default (6) must live only in TimeConfig; read config.sn1IdleBroadcastIntervalTicks instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'hashBroadcastPolicy'
        Pattern = '(?<![\w.>])hashBroadcastPolicy\s*=\s*(TimeConfig::)?HashBroadcastPolicy::'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'hashBroadcastPolicy default (HashBroadcastPolicy::EveryTick) must live only in TimeConfig; read config.hashBroadcastPolicy instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'hashBroadcastIntervalTicks'
        Pattern = '(?<![\w.>])hashBroadcastIntervalTicks\s*=\s*1\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'hashBroadcastIntervalTicks default (1) must live only in TimeConfig; read config.hashBroadcastIntervalTicks instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'hashMismatchTickThreshold'
        Pattern = '(?<![\w.>])hashMismatchTickThreshold\s*=\s*5\b'
        # DesyncDiagnosticSink.h / SimulationReconciliation.h (T8) consume it via
        # the shouldEscalateToLayer2 helper, which is a member-access read.
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp', 'DesyncDiagnosticSink.h', 'SimulationReconciliation.h')
        Message = 'hashMismatchTickThreshold default (5) must live only in TimeConfig; read config.hashMismatchTickThreshold instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'hashMismatchReaction'
        Pattern = '(?<![\w.>])hashMismatchReaction\s*=\s*(TimeConfig::)?HashMismatchReaction::'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp', 'DesyncDiagnosticSink.h', 'SimulationReconciliation.h')
        Message = 'hashMismatchReaction default (HashMismatchReaction::LogOnly) must live only in TimeConfig; read config.hashMismatchReaction instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'sparseSaveMode'
        Pattern = '(?<![\w.>])sparseSaveMode\s*=\s*false\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'sparseSaveMode default (false) must live only in TimeConfig; read config.sparseSaveMode instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'recordSkipEvents'
        Pattern = '(?<![\w.>])recordSkipEvents\s*=\s*true\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'recordSkipEvents default (true) must live only in TimeConfig; read config.recordSkipEvents instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'recordStallEvents'
        Pattern = '(?<![\w.>])recordStallEvents\s*=\s*true\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'recordStallEvents default (true) must live only in TimeConfig; read config.recordStallEvents instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'recordSubstitutionEvents'
        Pattern = '(?<![\w.>])recordSubstitutionEvents\s*=\s*true\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'recordSubstitutionEvents default (true) must live only in TimeConfig; read config.recordSubstitutionEvents instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'recordRedundancyHits'
        Pattern = '(?<![\w.>])recordRedundancyHits\s*=\s*false\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'recordRedundancyHits default (false) must live only in TimeConfig; read config.recordRedundancyHits instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'aggregateSiblingInputBundles'
        Pattern = '(?<![\w.>])aggregateSiblingInputBundles\s*=\s*false\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'aggregateSiblingInputBundles default (false) must live only in TimeConfig; read config.aggregateSiblingInputBundles instead of re-declaring it (R-P1)'
    },
    @{
        Field   = 'hashLogRingCapacity'
        Pattern = '(?<![\w.>])hashLogRingCapacity\s*=\s*600\b'
        Allowed = @('TimeConfig.h', 'TimeConfigDefaultsTest.cpp')
        Message = 'hashLogRingCapacity default (600) must live only in TimeConfig; read config.hashLogRingCapacity instead of re-declaring it (R-P1 / B4)'
    }
)

# ---------------------------------------------------------------------------
# Path exclusions: build output, engine intermediate, and vendored third-party.
# Task 5 spec names Binaries / Intermediate / extern/*/build* / glm; ThirdParty
# (vendored Jolt) is excluded on the same "vendored code" rationale as glm.
# ---------------------------------------------------------------------------
$ExcludeRegex = '[\\/](Binaries|Intermediate|ThirdParty|glm|\.git|\.vs)[\\/]|[\\/]build([\\/_]|$)'

# ---------------------------------------------------------------------------
# Strip // comments, /* */ comments (incl. multi-line), and string/char literals
# from a single physical line. $InBlock carries block-comment state across lines.
# ---------------------------------------------------------------------------
function Remove-CommentsAndStrings {
    param(
        [string]$Line,
        [ref]$InBlock
    )

    $s = $Line

    if ($InBlock.Value) {
        $idx = $s.IndexOf('*/')
        if ($idx -ge 0) {
            $s = $s.Substring($idx + 2)
            $InBlock.Value = $false
        }
        else {
            return ''
        }
    }

    # Collapse inline /* ... */ blocks; open-ended /* flips block state.
    while ($true) {
        $open = $s.IndexOf('/*')
        if ($open -lt 0) { break }
        $close = $s.IndexOf('*/', $open + 2)
        if ($close -ge 0) {
            $s = $s.Substring(0, $open) + ' ' + $s.Substring($close + 2)
        }
        else {
            $s = $s.Substring(0, $open)
            $InBlock.Value = $true
            break
        }
    }

    # Blank out string and char literals (so "tick=60" or '1' never match).
    $s = [regex]::Replace($s, '"(\\.|[^"\\])*"', '""')
    $s = [regex]::Replace($s, "'(\\.|[^'\\])*'", "''")

    # Drop trailing // line comment.
    $li = $s.IndexOf('//')
    if ($li -ge 0) {
        $s = $s.Substring(0, $li)
    }

    return $s
}

# ---------------------------------------------------------------------------
# SELF-LINT — "the blacklist is a contract" enforced mechanically.
#
# The header comment above says every PR that adds a TimeConfig field must add a
# matching blacklist entry. Prose alone does not hold: the v1 tree already had
# seven TimeConfig fields with no entry. This pass parses TimeConfig.h, extracts
# every field name, and fails if any field has no representing blacklist entry —
# so a newly added field cannot silently escape the configurability contract.
#
# A field counts as REPRESENTED when some blacklist entry either names it in
# `Field` or mentions it literally in `Pattern` (the latter covers wildcarded
# entries such as `redundancyDepth\w*`).
#
# This does NOT verify that the entry's magic value matches the field's current
# default — the Catch2 default-drift gate (TimeConfigDefaultsTest.cpp) is the arm
# that catches value drift. This pass only guarantees coverage exists.
# ---------------------------------------------------------------------------
function Test-BlacklistCoversTimeConfig {
    param(
        [string]$TimeConfigPath,
        [object[]]$Entries
    )

    if (-not (Test-Path -LiteralPath $TimeConfigPath)) {
        Write-Host "Configurability self-lint: TimeConfig.h not found at $TimeConfigPath"
        return @{ Ok = $false; Missing = @(); NotFound = $true }
    }

    # Field declarations look like:  <type> <name>[ [extent] ] = <default>;
    # Enum TYPE declarations (`enum class Foo { ... };`) carry no '=' and are
    # skipped; the enum-typed FIELD that follows them is matched normally.
    $fieldRegex = '^\s*(?:[A-Za-z_][\w:]*\s+)+([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*=\s*[^=]'

    $fields = [System.Collections.Generic.List[string]]::new()
    $inBlock = $false
    $inStruct = $false
    foreach ($line in [System.IO.File]::ReadLines($TimeConfigPath)) {
        $code = Remove-CommentsAndStrings $line ([ref]$inBlock)
        if ([string]::IsNullOrWhiteSpace($code)) { continue }

        if (-not $inStruct) {
            if ($code -match '^\s*struct\s+TimeConfig\b') { $inStruct = $true }
            continue
        }
        # Struct ends at a closing brace in column 0.
        if ($code -match '^\};') { break }

        $m = [regex]::Match($code, $fieldRegex)
        if ($m.Success) { $fields.Add($m.Groups[1].Value) }
    }

    $missing = [System.Collections.Generic.List[string]]::new()
    foreach ($name in $fields) {
        $covered = $false
        foreach ($e in $Entries) {
            if ($e.Field -eq $name -or $e.Pattern -like "*$name*") { $covered = $true; break }
        }
        if (-not $covered) { $missing.Add($name) }
    }

    return @{ Ok = ($missing.Count -eq 0); Missing = $missing; Parsed = $fields.Count; NotFound = $false }
}

# ---------------------------------------------------------------------------
# Main scan
# ---------------------------------------------------------------------------
if (-not (Test-Path -LiteralPath $RepoRoot)) {
    Write-Error "RepoRoot not found: $RepoRoot"
    exit 2
}

$timeConfigPath = Join-Path $RepoRoot 'Plugins/OGSimulation/Source/OGSimulation/og-simulation/OGSimulation/PCTimeManagement/TimeConfig.h'
$selfLint = Test-BlacklistCoversTimeConfig -TimeConfigPath $timeConfigPath -Entries $Blacklist

if ($selfLint.NotFound) {
    Write-Host 'Configurability self-lint: FAILED — could not locate TimeConfig.h (path moved?).'
    exit 2
}
if (-not $selfLint.Ok) {
    foreach ($f in $selfLint.Missing) {
        Write-Host ("TimeConfig.h: field '{0}' has no CONFIG_LITERAL_BLACKLIST entry (R-P1 contract)" -f $f)
    }
    Write-Host ''
    Write-Host ("Configurability self-lint: FAILED ({0} uncovered field(s) of {1} parsed)" -f `
        $selfLint.Missing.Count, $selfLint.Parsed)
    Write-Host 'See tools/lint/README.md — adding a TimeConfig field requires a blacklist entry in the same change.'
    exit 1
}

$files = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
foreach ($root in $ScanRoots) {
    $full = Join-Path $RepoRoot $root
    if (Test-Path $full) {
        Get-ChildItem -Path $full -Recurse -File -Include *.h, *.cpp |
            ForEach-Object { $files.Add($_) }
    }
}

$scanned = 0
$violations = [System.Collections.Generic.List[object]]::new()

foreach ($f in $files) {
    if ($f.FullName -match $ExcludeRegex) { continue }
    $scanned++
    $base = $f.Name

    # Pre-compute which blacklist entries are active for this file.
    $active = $Blacklist | Where-Object { $_.Allowed -notcontains $base }
    if ($active.Count -eq 0) { continue }

    $inBlock = $false
    $lineNo = 0
    foreach ($line in [System.IO.File]::ReadLines($f.FullName)) {
        $lineNo++
        $code = Remove-CommentsAndStrings $line ([ref]$inBlock)
        if ([string]::IsNullOrWhiteSpace($code)) { continue }
        foreach ($entry in $active) {
            if ($code -match $entry.Pattern) {
                $rel = [System.IO.Path]::GetRelativePath($RepoRoot, $f.FullName)
                $violations.Add([pscustomobject]@{
                    File    = $rel
                    Line    = $lineNo
                    Field   = $entry.Field
                    Message = $entry.Message
                })
            }
        }
    }
}

# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------
if ($violations.Count -gt 0) {
    foreach ($v in $violations) {
        Write-Host ("{0}:{1}: {2} (Synthesis Addendum Correction 5 binding rule)" -f `
            $v.File, $v.Line, $v.Message)
    }
    Write-Host ''
    Write-Host ("Configurability lint: FAILED ({0} violation(s) across {1} files scanned)" -f `
        $violations.Count, $scanned)
    Write-Host 'See tools/lint/README.md — every TimeConfig value must be read from TimeConfig, not re-declared.'
    exit 1
}
else {
    Write-Host ("Configurability lint: clean ({0} files scanned)" -f $scanned)
    exit 0
}
