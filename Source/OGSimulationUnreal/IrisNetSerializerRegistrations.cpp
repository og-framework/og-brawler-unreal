// SPDX-License-Identifier: MPL-2.0

#include "SyncedSimulationStateBuffer.h"
#include "RelayedInputRing.h"

#if UE_WITH_IRIS
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#endif

// ---------------------------------------------------------------------------
// [og-netcode-v2-input-relay / T29] IRIS NETSERIALIZER REGISTRATIONS.
//
// THE PROBLEM THIS FILE SOLVES, stated as the failure it caused rather than as
// the mechanism it uses.
//
// This project runs Iris replication (`net.Iris.UseIrisReplication=1`,
// Config/DefaultEngine.ini; runtime proof in any server log:
// `LogNet: InitBase GameNetDriver ... using replication model Iris`). Iris
// resolves a struct's custom serializer BY NAME FROM A REGISTRY
// (FPropertyNetSerializerInfoRegistry::FindStructSerializerInfo, consulted from
// ReplicationStateDescriptorBuilder.cpp:2163-2189) — it does NOT look at
// `TStructOpsTypeTraits<...>::WithNetSerializer`, which is what these two structs
// declare and all the legacy system ever needed. With no registry entry Iris
// falls through to FStructNetSerializer, replicates the struct's UPROPERTY
// members one by one, and NEVER CALLS NetSerialize. It says so out loud, once per
// struct, at ReplicationStateDescriptorBuilder.cpp:2575-2581:
//
//     LogIris: Warning: Generating descriptor for struct SimulationStateSyncBuffer
//              that has custom serialization.
//     LogIris: Warning: Generating descriptor for struct RelayedInputRing
//              that has custom serialization.
//
// Both lines are in this project's own PIE logs. THREE things were dead behind
// them, and only the third is about bandwidth:
//
//   1. THE WIRE-FORMAT BUILD-MISMATCH FENCE (the reason this file exists).
//      FSimulationStateSyncBuffer::receivedWireFormatVersion is assigned at
//      exactly one site — the load path of NetSerialize — and initialises to the
//      LOCAL kWireFormatVersion so an unreplicated buffer never false-positives.
//      With NetSerialize never called, that assignment never ran, so
//      USimmableUpdateComponent::OnRep_CorrectionState was comparing the local
//      constant AGAINST ITSELF and could never be unequal. T4 bumped
//      kWireFormatVersion 1 -> 2 specifically to arm this fence; it was protecting
//      nothing. Two builds with incompatible state layouts would have silently
//      mis-parsed each other's bytes instead of raising the "get the latest
//      archive" toast — and playtest builds ship from Saved/Archive/, which is
//      precisely the scenario the fence was written for.
//   2. FRelayedInputRing's malformed-length rejection (T20 flagged it, left it).
//   3. The used-prefix trim on the 384-byte correction buffer, whose whole point
//      is to send `usedBytes` and not `kBufferBytes`.
//
// THE FIX. UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_AND_REGISTRY_DELEGATES
// declares an FNamedStructLastResortPropertyNetSerializerInfo for the named
// struct and registers it on the pre-freeze delegate. Iris then resolves
// FLastResortPropertyNetSerializer, whose Quantize calls
// `Property->NetSerializeItem(...)` into a scratch FNetBitWriter and whose
// Dequantize calls it again on the read side
// (LastResortPropertyNetSerializer.cpp:145-218) — i.e. the struct's own
// NetSerialize, save path and load path, is back in the loop.
//
// WHAT THIS DOES *NOT* CHANGE, deliberately:
//   * The bytes NetSerialize produces. This restores the encoder; it does not
//     touch the encoding. kWireFormatVersion is NOT bumped.
//   * Dirty detection. The poll-time compare for a struct member with a custom
//     serializer still lands on `FProperty::Identical`
//     (InternalPropertyReplicationState.cpp — the LastResort path is neither the
//     struct nor the array special case, so it takes the default arm), which is
//     the member-wise TArray compare that was already running. Note the standing
//     consequence: a member kept OUTSIDE the UPROPERTY is still invisible to
//     dirty detection. That is unchanged by this file, and it is why both structs
//     keep every byte that can change inside the array.
//
// RECEIVE-SIDE ESCALATION — what "the malformed-length guard is alive again"
// does and does not mean. Under Iris, the FArchive a load-path NetSerialize sees
// is NOT the bunch: Dequantize builds an FNetBitReader over a blob that
// Deserialize already read off the wire, and it does not inspect that archive's
// error state afterwards (LastResortPropertyNetSerializer.cpp:187-218). So
// FRelayedInputRing's `Ar.SetError()` now RUNS — the oversize length is detected
// and the oversized read is refused, which is the part that matters — but the
// consequence is a dropped ring update on that object rather than a failed
// bunch. The engine's own outer bound still applies first: Deserialize rejects a
// blob above 65535 bits. This is a strict improvement over the pre-T29 state, in
// which nothing in our code inspected the length at all, and it is recorded here
// so nobody later reads "guard restored" as "bunch is failed".
//
// STRUCTS DELIBERATELY NOT REGISTERED HERE — each verified, not assumed:
//   * FSmallSimulationStateSyncBuffer (the timing relay's buffer) has NO
//     NetSerialize and NO TStructOpsTypeTraits specialisation at all. It never had
//     a custom serializer for Iris to bypass, which is exactly why it never
//     warned: member-wise replication is what it always did and what it still
//     does. Registering it would CHANGE its wire format, not restore it.
//   * FInputRedundancyBundle, likewise, has no NetSerialize and no traits
//     specialisation. The backlog's stated reason — "it is an RPC parameter, not a
//     replicated property" — does NOT hold: Iris builds a ReplicationStateDescriptor
//     for a UFunction's parameters through the same member-property resolution
//     (FReplicationStateDescriptorBuilder::CreateDescriptorForFunction), so an RPC
//     parameter struct WITH a custom NetSerialize and no registration would be
//     bypassed exactly like a replicated one. The bundle is healthy for the other
//     reason: it has nothing to bypass.
//   * FSimulationInputSyncBuffer DOES declare WithNetSerializer = true, and is the
//     one type here that would be a casualty if it were still replicated. It is
//     not: T8 retired `m_replicatedInputSyncedBuffer`, and its one surviving
//     instance (USimmableUpdateComponent::m_clientToServerInputSyncedBuffer) is not
//     a UPROPERTY at all, so no descriptor is ever built for it and no warning is
//     ever emitted. If it is ever replicated again it must be added below in the
//     same edit.
//
// PROVING THIS IS LIVE. The absence of the LogIris warning is NOT sufficient
// evidence — it proves Iris found *a* serializer, not that it found *ours*. The
// one-shot `[IrisSerializerProof]` report in IrisNetSerializerProof.h is emitted
// from inside the NetSerialize bodies themselves and is the acceptance evidence.
// ---------------------------------------------------------------------------

#if UE_WITH_IRIS
UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_AND_REGISTRY_DELEGATES(SimulationStateSyncBuffer);
UE_NET_IMPLEMENT_NAMED_STRUCT_LASTRESORT_NETSERIALIZER_AND_REGISTRY_DELEGATES(RelayedInputRing);
#endif // UE_WITH_IRIS
