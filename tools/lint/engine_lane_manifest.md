<!-- SPDX-License-Identifier: MPL-2.0 -->
# Engine lane manifest

Every engine replication marker in the tree, classified by its readers; the fence is `engine_lane_fence.ps1`, and end-state E4 is its rule.

sim-rows: closed

| Kind | Class | Member | Type | Lane class | Retired by | Readers |
|---|---|---|---|---|---|---|
| Property | AOGBrawlerUECharacter | RingoutScore | int32 | Presentation | - | scoreboardVisualizationUImpl::gatherScoreboardRows via GetRingoutScore; AOGBrawlerUECharacter::OnRep_RingoutScore (client log). Written by BrawlerRosterUImpl::pushRingoutScores via SetAuthoritativeRingoutScore |
| Property | AOGBrawlerUECharacter | SimCharacterIdValue | uint8 | Binding | - | UOGBrawlerSimulatableComponent::resolveRegistrationKey via GetSimCharacterId (the registration key: the authority allocates it, a client Retries until it arrives); the bind gate, UESimulationSession::clientBindStep, which the same hook asks with that id on a client (the og-sim-net roster keys its entries by it); BrawlerTickDriverGameUImpl::callWithSlotRequest via GetSimCharacterId (a client's slot is its roster entry's, through UESimulationSession::physicsSlotFor); also UOGBrawlerSimulatableComponent::simCharacterId, scoreboardVisualizationUImpl::gatherScoreboardRows, firstLocalCharacterId (InputHistoryVisualizationUImpl); AOGBrawlerUECharacter::GetBrawlerColor (the colour, paletteIndexFor) from ApplyBrawlerColor (OnRep_SimCharacterIdValue, SetAuthoritativeSimCharacterId, RebuildHumanoidMesh) and from scoreboardVisualizationUImpl::gatherScoreboardRows |
| Rpc | AOGBrawlerPlayerController | ServerLeaveLocalPlayer | - | Session | - | AOGBrawlerPlayerController::ServerLeaveLocalPlayer_Implementation (server; removes the last child connection), sent by LeaveLocalPlayer |
| EngineClass | AThirdPersonMap_C | * | - | Session | - | none: the level script actor of /Game/ThirdPerson/Maps/ThirdPersonMap (GameDefaultMap and EditorStartupMap); replicates through ALevelScriptActor and declares no replicated member |
