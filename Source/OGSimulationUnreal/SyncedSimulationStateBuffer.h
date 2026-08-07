// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "OGSimulationUnreal.h"
#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include <stdexcept>

#include "OGSimulation/PCTimeManagement/ServerTickClock.h"
#include "OGSimulation/SimulationComposite.h"
#include "OGSimulation/OGAssert.h"
#include "OGSimulation/InputRedundancyBundleCodec.h"
#include "OGSimulation/CorrectionStateBufferCodec.h"

#include "IrisNetSerializerProof.h"

#include "SyncedSimulationStateBuffer.generated.h"

// ---------------------------------------------------------------------------
// [Task 7 / Phase 1 — Stage 1] FInputRedundancyBundle
//
// Payload for the unreliable + redundancy input channel that replaces the
// reliable FSimulationStateSyncBuffer RPC (proposal §3.1 step 8). The client
// re-sends the last `redundancyDepthTicks` ticks of input every frame so a
// dropped UDP datagram self-heals on the next send instead of stalling a
// reliable resend (the R-T1 streeting-saturation mitigation).
//
// Wire format (length-prefixed):
//   [version_byte (u8)]  offset 0  == kWireFormatVersion
//   [slot_count   (u8)]  offset 1  == number of slots actually appended
//   [slots...]           offset 2  each slot is:
//       [capture_tick (u32)] [input_serialized_bytes (fixed per InputType)]
//
// Runtime slot count = TimeConfig::redundancyDepthTicks. Default tracks the
// runtime tick rate per proposal §11: 5 at 100 Hz interim (Stage 1 ship),
// 3 at 60 Hz target (post-Stage-2 ship). Hard upper bound = kMaxSlots = 8 for
// wire safety; the runtime depth is min(redundancyDepthTicks, kMaxSlots).
//
// INVARIANT (R-T5 / D3.11): inputs written into this bundle are append-only and
// immutable per capture-tick. The client MUST NOT revise the input value for a
// previously-emitted capture-tick; any producer that does so silently loses the
// corrected value at the dedup-by-capture-tick step on the server. If a future
// feature requires input revision post-capture, the wire format needs an
// explicit revision-number field and the dedup contract must be re-designed.
//
// The server's dedup-by-capture-tick logic (RemoteMoveQueue, Task 10) silently
// drops a duplicate capture_tick — that is correct ONLY under this invariant.
// appendSlot() therefore OG_CHECK-fails on a duplicate capture_tick in checked
// builds, and silently drops it (first arrival wins) in shipping builds where
// the OG_CHECK compiles out. The kMaxSlots wire budget is enforced the same two
// ways (T16), so no build config can emit a bundle above the budget. See the
// ENFORCEMENT block in OGSimulation/InputRedundancyBundleCodec.h.
//
// The version byte exists so pre/post-Stage-1 builds refuse to interop loudly
// (wire-format compat fence per risks_and_plan.md §5.2; the mismatch check
// itself lives in Task 11).
//
// ⛔ [T29] THIS TYPE DELIBERATELY HAS NO `NetSerialize` AND NO
// `TStructOpsTypeTraits` SPECIALISATION, and that is the ONLY reason it survived
// the Iris migration intact — it had no custom serializer to lose. Being an RPC
// parameter did NOT protect it: Iris builds a ReplicationStateDescriptor for a
// UFunction's parameters through the same resolution it uses for replicated
// members (FReplicationStateDescriptorBuilder::CreateDescriptorForFunction), so
// RPC parameter structs are NOT exempt — the backlog's claim that they are is
// false, and inheriting it is how this bug class comes back.
//
// If you ever add a custom `NetSerialize` here, you MUST also register it in
// Source/OGSimulationUnreal/IrisNetSerializerRegistrations.cpp in the same edit.
// Without that entry Iris will silently keep replicating `wireBytes` member-wise
// and your serializer will never be called.
// ---------------------------------------------------------------------------
USTRUCT()
struct OGSIMULATIONUNREAL_API FInputRedundancyBundle
{
public:
	GENERATED_BODY()

	// Wire-format constants live in the engine-agnostic codec
	// (OGSimulation/InputRedundancyBundleCodec.h, Task 12). These aliases
	// preserve the FInputRedundancyBundle::kWireFormatVersion / ::kMaxSlots
	// public surface that buildRedundancyBundle (Task 9) and the compat fence
	// (Task 11) reference by name.
	static constexpr uint8 kWireFormatVersion = inputRedundancyBundle::kWireFormatVersion;
	static constexpr uint8 kMaxSlots          = inputRedundancyBundle::kMaxSlots;

	// Length-prefixed serialized bundle per the schema above.
	UPROPERTY()
	TArray<uint8> wireBytes;

	// Byte-buffer adapter surface required by the codec templates
	// (InputRedundancyBundleCodec.h "BUFFER CONCEPT"). The codec owns the wire
	// layout / dedup / iteration logic; this USTRUCT supplies storage +
	// replication (UPROPERTY wireBytes) and these four accessors.
	int32 bundleByteNum() const { return wireBytes.Num(); }
	void  bundleAddZeroedBytes(int32 Count) { wireBytes.AddZeroed(Count); }

	// Raw byte accessors — the generic SimulationSerialization.h free functions
	// (writeToSyncedBuffer / writeCompositeToSyncedBuffer and their read
	// counterparts, invoked from the codec) serialize a slot's input directly
	// into wireBytes. appendSlot grows the array before writing, so the bounds
	// checks below always pass on the happy path.
	template <typename T>
	void writeToBuffer(uint32 ByteIndex, const T& Value)
	{
		checkf(ByteIndex + sizeof(T) <= static_cast<uint32>(wireBytes.Num()),
			TEXT("FInputRedundancyBundle write OOB: offset=%u size=%llu capacity=%d"),
			ByteIndex, static_cast<uint64>(sizeof(T)), wireBytes.Num());

		const uint8* ValueAsBytes = reinterpret_cast<const uint8*>(&Value);
		for (int32 i = 0; i < sizeof(T); ++i)
			wireBytes[ByteIndex + i] = ValueAsBytes[i];
	}

	template <typename T>
	T readFromBuffer(uint32 ByteIndex) const
	{
		checkf(ByteIndex + sizeof(T) <= static_cast<uint32>(wireBytes.Num()),
			TEXT("FInputRedundancyBundle read OOB: offset=%u size=%llu capacity=%d"),
			ByteIndex, static_cast<uint64>(sizeof(T)), wireBytes.Num());

		T Value;
		uint8* ValueAsBytes = reinterpret_cast<uint8*>(&Value);
		for (int32 i = 0; i < sizeof(T); ++i)
			ValueAsBytes[i] = wireBytes[ByteIndex + i];
		return Value;
	}

	// Producer-side append of one (capture_tick, input) slot. ENFORCES R-T5 and
	// the kMaxSlots wire-safety bound: OG_CHECK on a duplicate capture_tick or an
	// overflow in checked builds, silent drop in shipping. Thin delegation to the
	// engine-agnostic codec (Task 12) so the production path and the pure-C++
	// Low-Level-Tests exercise the same logic.
	template <typename InputType>
	void appendSlot(uint32 capture_tick, const InputType& input)
	{
		inputRedundancyBundle::appendSlot<InputType>(*this, capture_tick, input);
	}

	// Append variant that reports whether the slot was actually written. Returns
	// false when `capture_tick` was already emitted (the R-T5 silent drop; the
	// first-arrival input is preserved) or when the bundle is already at kMaxSlots
	// (the T16 wire-budget drop). In both cases the buffer is left byte-for-byte
	// untouched. Use this instead of appendSlot() only where such a drop is an
	// expected outcome rather than a producer bug.
	template <typename InputType>
	[[nodiscard]] bool tryAppendSlot(uint32 capture_tick, const InputType& input)
	{
		return inputRedundancyBundle::tryAppendSlot<InputType>(*this, capture_tick, input);
	}

	// Consumer-side iteration: invokes callback(uint32 capture_tick, const
	// InputType& input) for each slot in arrival order. The server feeds these
	// into RemoteMoveQueue::queueMove (Task 10). No-op on an empty bundle.
	template <typename InputType, typename Callback>
	void forEachSlot(Callback&& callback) const
	{
		inputRedundancyBundle::forEachSlot<InputType>(*this, Forward<Callback>(callback));
	}

	// Reads the version byte (first byte of wireBytes). Returns 0 on an empty
	// bundle (no header written yet).
	uint8 getWireFormatVersion() const
	{
		return inputRedundancyBundle::getWireFormatVersion(*this);
	}
};

USTRUCT()
struct OGSIMULATIONUNREAL_API FSimulationStateSyncBuffer
{
public:
	GENERATED_BODY()

	// Wire format budget. Must hold the largest composite the project replicates
	// through this buffer (simulatableBrawler::State correction payload). NOTE:
	// raising this is *wire-cheap* — NetSerialize watermark-trims to usedBytes,
	// so a larger capacity does NOT increase per-tick bytes-on-wire; it only
	// raises the ceiling before the checkf OOB path is hit. The actual per-second
	// bandwidth is governed by MaxClientRate (Config/DefaultEngine.ini, owned by
	// the bandwidth re-measure task), NOT by this constant.
	//
	// History:
	//   2026-06-10: briefly 1024 to fit the original PhysicsBodyState-per-slot
	//     projectile State+IC; that 100Hz Reliable-RPC era hit ~7x MaxClientRate
	//     overrun (OGBrawlerNetworkModelResearch/research/spike_input_rpc_saturation.md),
	//     so the projectile sub-sim was un-wired and this reverted to 256.
	//   2026-06-24 (Phase 2 / Task 14): bumped 256 -> 384 to fit the re-wired
	//     composite under the closed-form projectile redesign (Task 13).
	//
	// Sizing math (2026-06-24), all numbers compile-time syncSize<> measured via
	// a temporary LLT (un-wired State composite + the closed-form projectile
	// additions T15 will append to simulatableBrawler::State):
	//     un-wired composite               = 157 B
	//       (radialIC 24 + radialSt 61 + guardSt 56 + guardIC 0 + machineSt 16)
	//   + projectile InitialConditions     =  28 B
	//   + projectile State (T13 closed-form, SIM_VECTOR cap 3 = 4 + 3*37) = 115 B
	//   = re-wired State composite         = 300 B
	//   + tick header (uint32 at offset 0) =   4 B
	//   = re-wired wire footprint          = 304 B   (exact; composite is byte-packed)
	//
	//   2026-08-03 (og-netcode-v2-input-relay T4): + the applied-capture-tick ref
	//   (uint32 at offset 4) = 308 B. Headroom 384 - 308 = 76 B (~25%); no bump
	//   needed. This is the ONLY per-tick byte this task adds — the ref replaces a
	//   whole replicated input value once T8 removes the old correction-input
	//   write, so the increment's net wire delta is negative.
	//
	// 384 = smallest 64-byte multiple above 304 + ~25% margin (304*1.25 = 380):
	//   headroom = 384 - 304 = 80 B (~26%) for future composite growth. 320 was
	//   rejected (only 16 B / ~5% headroom — one extra vec3 would overflow again);
	//   512 was deemed over-budget vs the measured footprint (kept the budget-
	//   discipline signal tight). Re-measure if the State composite grows.
	static constexpr int32 kBufferBytes = 384;

	// Wire-format version. Parallels FSimulationInputSyncBuffer::kWireFormatVersion
	// and FInputRedundancyBundle::kWireFormatVersion (Stage 1 wire format = 1).
	// The version-byte prepend + pre/post-Stage-1 compat fence landed in Task 11
	// (see OGBrawlerNetworkModelResearch risks_and_plan §5.2); the refusal path is
	// USimmableUpdateComponent::OnRep_CorrectionState.
	//
	// [og-netcode-v2-input-relay T4] BUMPED 1 -> 2, and the constant now lives in
	// the engine-agnostic codec beside the layout it fences (the same aliasing the
	// bundle above uses). 2 = the payload gained the applied-capture-tick ref at
	// offset 4. This is the SINGLE wire fence of the input-relay increment: the
	// relay ring, the RPC and the input buffer all ride behind it, so ONE
	// mismatched build is refused loudly at the first correction OnRep.
	static constexpr uint8 kWireFormatVersion = correctionStateBuffer::kWireFormatVersion;

	FSimulationStateSyncBuffer()
		: buffer( {0} )
	{
		buffer.SetNum(kBufferBytes);
	}

	template <typename T>
	void writeToBuffer(uint32 ByteIndex, const T& Value)
	{
		// Note: pre-fix bounds check was constructing std::invalid_argument
		// without throwing — silent no-op that let the underlying TArray
		// bounds-assert fire instead. checkf surfaces the overflow cleanly with
		// the offending offset/size in the log.
		checkf(ByteIndex + sizeof(T) <= static_cast<uint32>(buffer.Num()),
			TEXT("FSimulationStateSyncBuffer write OOB: offset=%u size=%llu capacity=%d (raise kBufferBytes)"),
			ByteIndex, static_cast<uint64>(sizeof(T)), buffer.Num());

		// Copy the value into the buffer as raw bytes
		const uint8* ValueAsBytes = reinterpret_cast<const uint8*>(&Value);
		for (int32 i = 0; i < sizeof(T); ++i)
			buffer[ByteIndex + i] = ValueAsBytes[i];

		// Track the high-water mark so NetSerialize only emits the used prefix
		// (watermark trim per spike_input_rpc_saturation.md Option 2). usedBytes
		// is reset to 0 at the start of write(composite, tick) before a re-publish.
		const uint32 endByte = ByteIndex + static_cast<uint32>(sizeof(T));
		if (endByte > usedBytes)
			usedBytes = static_cast<uint16>(endByte);
	}

	template <typename T>
	T readFromBuffer(uint32 ByteIndex) const
	{
		checkf(ByteIndex + sizeof(T) <= static_cast<uint32>(buffer.Num()),
			TEXT("FSimulationStateSyncBuffer read OOB: offset=%u size=%llu capacity=%d"),
			ByteIndex, static_cast<uint64>(sizeof(T)), buffer.Num());

		// Read the value from the buffer as raw bytes
		T Value;
		uint8* ValueAsBytes = reinterpret_cast<uint8*>(&Value);
		for (int32 i = 0; i < sizeof(T); ++i)
		{
			ValueAsBytes[i] = buffer[ByteIndex + i];
		}
		return Value;
	}

	// Wire format (OGSimulation/CorrectionStateBufferCodec.h — this USTRUCT is
	// storage + replication only and delegates the layout there, exactly as
	// FInputRedundancyBundle delegates to its codec):
	//   tick (uint32) @0, appliedCaptureTick (uint32) @4, composite @8.
	// Generic on SimulationComposite<Ts...> so the same method handles every
	// simulatable's State without pulling simulation-specific writer functions
	// into the transport.
	//
	// [T4] `appliedCaptureTick` is the join key: the capture tick of the input the
	// authority applied when it produced `value`, or kNoInputCaptureTick when it
	// substituted one. Passed WITH the state so a publish cannot pair this tick's
	// state with a stale ref.
	template <typename... Ts>
	void write(const SimulationComposite<Ts...>& value, uint32_t tick, uint32 appliedCaptureTick)
	{
		usedBytes = 0;  // fresh publish — reset the watermark before (re)writing
		correctionStateBuffer::write(*this, value, tick, appliedCaptureTick);
	}

	// Ref-less publish. Kept so the buffer still satisfies the shared
	// CompositeSyncedBufferConcept (and so any non-correction publisher can use
	// it); it writes the SENTINEL rather than leaving a previous publish's ref in
	// place, because "this publisher named no capture" and "the authority applied
	// no real input" are the same statement to every reader.
	template <typename... Ts>
	void write(const SimulationComposite<Ts...>& value, uint32_t tick)
	{
		write(value, tick, kNoInputCaptureTick);
	}

	// Symmetric counterpart to write(composite, tick, ref). Deduces Ts... from the
	// out-parameter and returns the tick. The ref is read separately through
	// getAppliedCaptureTick() so the common tick-only read stays a one-liner.
	template <typename... Ts>
	uint32_t readInto(SimulationComposite<Ts...>& outValue) const
	{
		uint32 ignoredAppliedCaptureTick = kNoInputCaptureTick;
		return correctionStateBuffer::readInto(*this, outValue, ignoredAppliedCaptureTick);
	}

	// [T4] The received join key for the tick this buffer currently carries.
	//
	// A buffer that has never been written or received (usedBytes below the
	// header) answers the SENTINEL rather than the zero bytes of its own default
	// storage — zero is a perfectly ordinary capture tick, and reporting it would
	// invent a reference that no authority ever sent.
	uint32 getAppliedCaptureTick() const
	{
		if (usedBytes < static_cast<uint16>(correctionStateBuffer::kHeaderBytes))
			return kNoInputCaptureTick;

		return correctionStateBuffer::readAppliedCaptureTick(*this);
	}

	// Version byte received from the wire (Task 11 compat fence). The first byte
	// serialized by NetSerialize is the sender's kWireFormatVersion; on load it is
	// stored here so the OnRep handler can detect a pre/post-Stage-1 build mismatch
	// (risks_and_plan.md §5.2). Defaults to the local kWireFormatVersion so a
	// never-replicated buffer never reads as a mismatch.
	uint8 getReceivedWireFormatVersion() const { return receivedWireFormatVersion; }

	// Watermark-trimmed network serialization: emit the wire-format version byte,
	// then the uint16 used-byte count, then only that many payload bytes — never
	// the full kBufferBytes. This replaces the default full-array replication that
	// previously sent all 256 bytes every tick (spike_input_rpc_saturation.md
	// Option 2). The version byte is the first byte on the wire (Task 11 compat
	// fence) so receivers can detect a build mismatch before trusting the payload.
	//
	// [T29] THIS FUNCTION WAS NOT BEING CALLED. Between Iris going live and T29
	// this whole body was unreachable: Iris resolves custom serializers from a
	// name registry, found no entry for this struct, and replicated `buffer`
	// member-wise instead. Everything the body does — the version fence's ONLY
	// write site, the watermark trim — was dead. The registration that revives it
	// is in IrisNetSerializerRegistrations.cpp; the one-shot report below is the
	// evidence that it is running, because "it compiles" and "the LogIris warning
	// went away" both fail to prove that Iris picked OUR serializer.
	bool NetSerialize(FArchive& Ar, class UPackageMap* /*Map*/, bool& bOutSuccess)
	{
		if (buffer.Num() != kBufferBytes)
			buffer.SetNum(kBufferBytes);

		// Wire-format version byte — first field on the wire (Task 11). Saving
		// writes the local kWireFormatVersion; loading captures the sender's value
		// for the OnRep compat-fence check.
		uint8 version = kWireFormatVersion;

		// [T29] Debug lever, default OFF and byte-identical when off. Every PIE
		// process runs the SAME BINARY and therefore the same compile-time
		// kWireFormatVersion, so perturbing what the SAVE path writes is the only way
		// to exercise the fence in PIE at all. Set it at runtime, in the process that
		// SAVES (the server), and never from an .ini or -ExecCmds — see the forcing
		// lever's block in IrisNetSerializerProof.h for both reasons.
		if (Ar.IsSaving())
			version = ogIrisNetSerializerProof::applyForcedWireVersionOffset(version);

		Ar << version;
		if (Ar.IsLoading())
			receivedWireFormatVersion = version;

		uint16 used = usedBytes;
		if (used > kBufferBytes)
			used = static_cast<uint16>(kBufferBytes);
		Ar << used;  // saving writes the count; loading overwrites it from the wire

		if (Ar.IsLoading())
		{
			if (used > kBufferBytes)
				used = static_cast<uint16>(kBufferBytes);
			usedBytes = used;
		}

		if (used > 0)
			Ar.Serialize(buffer.GetData(), used);

		// [T29] One-shot runtime proof, per direction AND per live/default payload.
		// Payload = version byte + uint16 length + `used` payload bytes; the
		// member-wise alternative would have carried buffer.Num() elements. Both
		// numbers come from THIS call.
		//
		// `used` is the GATE: Iris quantizes the CDO through this function while
		// building the class descriptor, before any replication, on server and
		// clients alike, and the CDO's usedBytes is 0. Gating the acceptance latch on
		// used > 0 is what keeps that artefact from consuming the one-shot — see the
		// LIVE-PAYLOAD GATE block in IrisNetSerializerProof.h.
		{
			static ogIrisNetSerializerProof::FNetSerializeReportLatch reportLatch;
			ogIrisNetSerializerProof::reportNetSerializeOnce(
				reportLatch,
				TEXT("SimulationStateSyncBuffer"), Ar.IsLoading(),
				static_cast<int32>(used),
				static_cast<int32>(sizeof(uint8) + sizeof(uint16)) + static_cast<int32>(used),
				buffer.Num());
		}

		bOutSuccess = true;
		return true;
	}

private:
	UPROPERTY()
	TArray<uint8> buffer;

	// High-water mark of written bytes; replicated as the wire payload length so
	// NetSerialize trims the unused tail. Not a UPROPERTY — NetSerialize carries
	// it explicitly on the wire.
	uint16 usedBytes = 0;

	// Sender's wire-format version captured on the most recent NetSerialize load
	// (Task 11 compat fence). Not a UPROPERTY — carried explicitly on the wire.
	// Initialized to the local version so a default-constructed (never-replicated)
	// buffer is never flagged as a mismatch.
	uint8 receivedWireFormatVersion = kWireFormatVersion;
};

// ⛔ [T29] UNDER IRIS, `WithNetSerializer` ALONE DOES NOTHING. Iris does not read
// this traits block to find a custom serializer — it resolves them from a NAME
// REGISTRY (FPropertyNetSerializerInfoRegistry). With no entry it builds a
// member-wise descriptor and NEVER CALLS NetSerialize, silently: no error, no
// compile failure, and the function still reads as live in source. That is exactly
// how the wire-format version fence and the malformed-length guard sat dead here
// from the day Iris went live until T29.
//
// So: if this struct is used as a REPLICATED PROPERTY **OR AS AN RPC PARAMETER**
// (Iris builds a descriptor for a UFunction's parameters through the same
// resolution — RPC params are NOT exempt, the backlog's claim that they are is
// false), it MUST have a matching registration in
// Source/OGSimulationUnreal/IrisNetSerializerRegistrations.cpp, added IN THE SAME
// EDIT. This one is registered.
template<>
struct TStructOpsTypeTraits<FSimulationStateSyncBuffer> : public TStructOpsTypeTraitsBase2<FSimulationStateSyncBuffer>
{
	enum { WithNetSerializer = true };
};

// ---------------------------------------------------------------------------
// [Task 8 / Phase 1 — Stage 1] FSimulationInputSyncBuffer
//
// Input-only sync buffer, split out from FSimulationStateSyncBuffer per
// proposal §1.1 row 3. The input role carries only a simulatable's PlayerInput
// composite, which is far smaller than the full State correction payload, so
// the wire budget is kBufferBytes = 128 (vs the correction buffer's 384).
//
// Mirrors FSimulationStateSyncBuffer's template surface
// (writeToBuffer / readFromBuffer / write(composite, tick) / readInto) so a
// call-site can swap one buffer type for the other with no other change — this
// is what lets SimmableUpdateComponent re-type the input-role members without
// touching their use-sites.
//
// Watermark trim: only the written prefix is replicated. A uint16 usedBytes
// header precedes the payload on the wire; custom NetSerialize emits
// sizeof(uint16) + usedBytes, never the full kBufferBytes
// (spike_input_rpc_saturation.md Option 2).
// ---------------------------------------------------------------------------
USTRUCT()
struct OGSIMULATIONUNREAL_API FSimulationInputSyncBuffer
{
public:
	GENERATED_BODY()

	// Input-only wire budget (~128 B per proposal §1.1 row 3). Do NOT bump
	// without coordinating with the OGBrawlerNetworkModelResearch wire-format
	// decisions.
	static constexpr int32 kBufferBytes = 128;

	// Wire-format version — parallels FSimulationStateSyncBuffer::kWireFormatVersion
	// and FInputRedundancyBundle::kWireFormatVersion. Version-byte prepend +
	// compat fence land in Task 11.
	static constexpr uint8 kWireFormatVersion = 1;

	FSimulationInputSyncBuffer()
		: buffer( {0} )
	{
		buffer.SetNum(kBufferBytes);
	}

	template <typename T>
	void writeToBuffer(uint32 ByteIndex, const T& Value)
	{
		checkf(ByteIndex + sizeof(T) <= static_cast<uint32>(buffer.Num()),
			TEXT("FSimulationInputSyncBuffer write OOB: offset=%u size=%llu capacity=%d (raise kBufferBytes)"),
			ByteIndex, static_cast<uint64>(sizeof(T)), buffer.Num());

		// Copy the value into the buffer as raw bytes
		const uint8* ValueAsBytes = reinterpret_cast<const uint8*>(&Value);
		for (int32 i = 0; i < sizeof(T); ++i)
			buffer[ByteIndex + i] = ValueAsBytes[i];

		// Track the high-water mark so NetSerialize only emits the used prefix.
		const uint32 endByte = ByteIndex + static_cast<uint32>(sizeof(T));
		if (endByte > usedBytes)
			usedBytes = static_cast<uint16>(endByte);
	}

	template <typename T>
	T readFromBuffer(uint32 ByteIndex) const
	{
		checkf(ByteIndex + sizeof(T) <= static_cast<uint32>(buffer.Num()),
			TEXT("FSimulationInputSyncBuffer read OOB: offset=%u size=%llu capacity=%d"),
			ByteIndex, static_cast<uint64>(sizeof(T)), buffer.Num());

		// Read the value from the buffer as raw bytes
		T Value;
		uint8* ValueAsBytes = reinterpret_cast<uint8*>(&Value);
		for (int32 i = 0; i < sizeof(T); ++i)
		{
			ValueAsBytes[i] = buffer[ByteIndex + i];
		}
		return Value;
	}

	// Wire format: tick (uint32) at offset 0, then per-field serialized composite.
	// Generic on SimulationComposite<Ts...> — identical contract to
	// FSimulationStateSyncBuffer::write so the two are drop-in interchangeable.
	template <typename... Ts>
	void write(const SimulationComposite<Ts...>& value, uint32_t tick)
	{
		usedBytes = 0;  // fresh publish — reset the watermark before (re)writing
		uint32 offset = 0;
		writeToBuffer(offset, tick);
		offset += sizeof(uint32);
		writeCompositeToSyncedBuffer(value, *this, offset);
	}

	// Symmetric counterpart to write(composite, tick). Returns the tick read from
	// byte 0.
	template <typename... Ts>
	uint32_t readInto(SimulationComposite<Ts...>& outValue) const
	{
		uint32 offset = 0;
		const uint32 tick = readFromBuffer<uint32>(offset);
		offset += sizeof(uint32);
		readCompositeFromSyncedBuffer(outValue, *this, offset);
		return tick;
	}

	// Watermark-trimmed network serialization: emit the uint16 used-byte count
	// then only that many payload bytes — never the full kBufferBytes.
	bool NetSerialize(FArchive& Ar, class UPackageMap* /*Map*/, bool& bOutSuccess)
	{
		if (buffer.Num() != kBufferBytes)
			buffer.SetNum(kBufferBytes);

		uint16 used = usedBytes;
		if (used > kBufferBytes)
			used = static_cast<uint16>(kBufferBytes);
		Ar << used;  // saving writes the count; loading overwrites it from the wire

		if (Ar.IsLoading())
		{
			if (used > kBufferBytes)
				used = static_cast<uint16>(kBufferBytes);
			usedBytes = used;
		}

		if (used > 0)
			Ar.Serialize(buffer.GetData(), used);

		bOutSuccess = true;
		return true;
	}

private:
	UPROPERTY()
	TArray<uint8> buffer;

	// High-water mark of written bytes; replicated as the wire payload length.
	// Not a UPROPERTY — NetSerialize carries it explicitly on the wire.
	uint16 usedBytes = 0;
};

// ⛔⛔ [T29] READ THE SAME WARNING ON FSimulationStateSyncBuffer'S TRAITS BLOCK
// ABOVE — AND NOTE THAT THIS TYPE IS THE SHARPEST CASE IN THE MODULE.
//
// It declares WithNetSerializer = true and is NOT registered in
// IrisNetSerializerRegistrations.cpp, which is correct ONLY because nothing
// replicates it today: T8 retired `m_replicatedInputSyncedBuffer`, and the one
// surviving instance (USimmableUpdateComponent::m_clientToServerInputSyncedBuffer)
// carries no UPROPERTY macro and appears in no DOREPLIFETIME and no RPC signature,
// so no descriptor is ever built for it. That is also why it never warned.
//
// It is therefore ONE `UPROPERTY` — or one RPC parameter — AWAY FROM BEING A
// SILENT CASUALTY of the exact defect T29 fixed. Whoever adds that must add the
// registration in Source/OGSimulationUnreal/IrisNetSerializerRegistrations.cpp in
// the same edit, and must expect the wire framing around the payload to change
// (see §5.2 of the T29 impl notes for what that costs).
template<>
struct TStructOpsTypeTraits<FSimulationInputSyncBuffer> : public TStructOpsTypeTraitsBase2<FSimulationInputSyncBuffer>
{
	enum { WithNetSerializer = true };
};

USTRUCT()
struct OGSIMULATIONUNREAL_API FSmallSimulationStateSyncBuffer
{
public:
	GENERATED_BODY()

	FSmallSimulationStateSyncBuffer()
		: buffer({ 0 })
	{
		buffer.SetNum(48);
	}

	template <typename T>
	void writeToBuffer(uint32 ByteIndex, const T& Value)
	{
		if (ByteIndex + sizeof(T) > buffer.Num())
			std::invalid_argument("writing out of bounds");

		// Copy the value into the buffer as raw bytes
		const uint8* ValueAsBytes = reinterpret_cast<const uint8*>(&Value);
		for (int32 i = 0; i < sizeof(T); ++i)
			buffer[ByteIndex + i] = ValueAsBytes[i];
	}

	template <typename T>
	T readFromBuffer(uint32 ByteIndex) const
	{
		if (ByteIndex + sizeof(T) > buffer.Num())
			std::invalid_argument("writing out of bounds");

		// Read the value from the buffer as raw bytes
		T Value;
		uint8* ValueAsBytes = reinterpret_cast<uint8*>(&Value);
		for (int32 i = 0; i < sizeof(T); ++i)
		{
			ValueAsBytes[i] = buffer[ByteIndex + i];
		}
		return Value;
	}

	// Satisfies SyncedTimingBufferConcept — used by SimulationManagerOwnerConcept.
	void write(const ServerTickClock& clock)
	{
		ServerTickClock::writeToSyncedBuffer(clock, *this, 0);
	}

private:
	UPROPERTY()
	TArray<uint8> buffer;
};

