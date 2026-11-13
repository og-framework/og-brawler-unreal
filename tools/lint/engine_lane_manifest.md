<!-- SPDX-License-Identifier: MPL-2.0 -->
# Engine lane manifest

Every engine replication marker in the tree, classified by its readers; the fence is `engine_lane_fence.ps1`, and end-state E4 is its rule.

sim-rows: open

| Kind | Class | Member | Type | Lane class | Retired by | Readers |
|---|---|---|---|---|---|---|
| Property | AOGBrawlerUECharacter | RingoutScore | int32 | Presentation | - | scoreboardVisualizationUImpl::gatherScoreboardRows via GetRingoutScore; AOGBrawlerUECharacter::OnRep_RingoutScore (client log). Written by BrawlerRosterUImpl::pushRingoutScores via SetAuthoritativeRingoutScore |
| Property | AOGBrawlerUECharacter | SimCharacterIdValue | uint8 | Sim | P5 (-> Binding) | UOGBrawlerSimulatableComponent::resolveRegistrationKey via GetSimCharacterId (authority allocates, clients Retry until it arrives); also UOGBrawlerSimulatableComponent::simCharacterId, scoreboardVisualizationUImpl::gatherScoreboardRows, firstLocalCharacterId (InputHistoryVisualizationUImpl); AOGBrawlerUECharacter::GetBrawlerColor (the colour, paletteIndexFor) from ApplyBrawlerColor (OnRep_SimCharacterIdValue, SetAuthoritativeSimCharacterId, RebuildHumanoidMesh) and from scoreboardVisualizationUImpl::gatherScoreboardRows |
| Property | AOGBrawlerUECharacter | PhysicsSlotValue | uint8 | Sim | P5 | AOGBrawlerUECharacter::GetPhysicsSlot -> BrawlerTickDriverGameUImpl::callWithSlotRequest -> decideBodySlot (replicatedSlot, client) via TickSlotTable::acquire |
| Rpc | AOGBrawlerPlayerController | ServerLeaveLocalPlayer | - | Session | - | AOGBrawlerPlayerController::ServerLeaveLocalPlayer_Implementation (server; removes the last child connection), sent by LeaveLocalPlayer |
| ReplicatedClass | USimulatableNetSyncComponent | * | - | Sim | P5 | carrier of m_simulationStateCorrectionSyncedBuffer and ServerReceiveRemoteMove; bound by UESimulatableOwnerBinding |
| Property | USimulatableNetSyncComponent | m_simulationStateCorrectionSyncedBuffer | FSimulationStateSyncBuffer | Sim | P5 | USimulatableNetSyncComponent::OnRep_CorrectionState -> SimulationNetSync::onCorrectionReceived; UELatencyBudgetDiagnostics via getSyncedCorrectionStateBuffer |
| Rpc | USimulatableNetSyncComponent | ServerReceiveRemoteMove | const FInputRedundancyBundle& | Sim | P5 | USimulatableNetSyncComponent::ServerReceiveRemoteMove_Implementation -> UESimulatableOwnerBinding::receiveRemoteMoveBundle |
| ReplicatedClass | ASimulationInputRelay | * | - | Sim | P5 | carrier of m_relayedInputRing; UESimulationSession::onInputRelayHostReady -> USimulatableNetSyncComponent::attachInputRelayHost |
| Property | ASimulationInputRelay | m_relayedInputRing | FRelayedInputRing | Sim | P5 | ASimulationInputRelay::OnRep_RelayedInputRing -> SimulationNetSync::onRelayedInputReceived -> SimulationInputResolution::ingestRelayRing; bind-time read via getRelayedInputRing |
| ReplicatedClass | ASimulationTimingRelay | * | - | Sim | P5 | carrier of m_buffer and m_relayDelayFloorTicks; listener ISimulationTimingRelayListener (UESimulationSession) |
| Property | ASimulationTimingRelay | m_buffer | FSmallSimulationStateSyncBuffer | Sim | P5 | ASimulationTimingRelay::OnRep_Buffer -> ServerTickClock::readFromSyncedBuffer -> UESimulationSession::onTimingInfoReceived |
| Property | ASimulationTimingRelay | m_relayDelayFloorTicks | uint8 | Sim | P5 | ASimulationTimingRelay::OnRep_RelayDelayFloor and replayLatchedRelayDelayFloor -> UESimulationSession::onRelayDelayFloorReceived and onRelayDelayFloorReplayed |
| ReplicatedClass | ASimulationConnectionRelay | * | - | Sim | P5 | carrier of m_connectionTier; listener ISimulationConnectionRelayListener (UESimulationSession) |
| Property | ASimulationConnectionRelay | m_connectionTier | uint8 | Sim | P5 | ASimulationConnectionRelay::OnRep_ConnectionTier and replayLatchedTier -> UESimulationSession::onConnectionTierReceived and onConnectionTierReplayed -> applyReplicatedConnectionTier |
| IrisSerializer | - | SimulationStateSyncBuffer | - | Sim | P5 | the Iris wire of FSimulationStateSyncBuffer::NetSerialize (m_simulationStateCorrectionSyncedBuffer only; FSmallSimulationStateSyncBuffer has no NetSerialize and replicates member-wise) |
| IrisSerializer | - | RelayedInputRing | - | Sim | P5 | the Iris wire of FRelayedInputRing::NetSerialize (m_relayedInputRing) |
| EngineClass | AThirdPersonMap_C | * | - | Session | - | none: the level script actor of /Game/ThirdPerson/Maps/ThirdPersonMap (GameDefaultMap and EditorStartupMap); replicates through ALevelScriptActor and declares no replicated member |
