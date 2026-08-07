// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "OGSimulationUnreal.h"
#include "CoreMinimal.h"

#include "OGSimulation/RelayedInputRingCodec.h"

#include "IrisNetSerializerProof.h"

#include "RelayedInputRing.generated.h"

// ---------------------------------------------------------------------------
// [og-netcode-v2-input-relay / T1] FRelayedInputRing
//
// The OUTBOUND relay payload: the server's replicated view of one character's
// most recent inputs, delivered to the OTHER clients so a peer can simulate that
// character with its real input instead of extrapolating
// (InputRelayDesign.md §4; RelayDelaySpectrumDesign.md §5).
//
// Each entry is (captureTick, dA, input), where `dA` is the SCHEDULE STAMP — the
// effective input delay the server held for that wire when the input arrived.
// The tick the input is scheduled to be APPLIED at is `captureTick + dA` and is
// DERIVED at the read, never stored: use relayedInputRing::applicationTick().
//
// SHAPE: a REPLACE-LATEST, depth-configurable RING. A newer capture tick
// supersedes the OLDEST resident entry; a capture tick that is already resident
// is rewritten in place. It NEVER appends without bound.
//
// THIS IS DELIBERATELY NOT FInputRedundancyBundle (fable finding B2). That type
// is the INBOUND (client -> server) RPC payload and is append-only + immutable
// per capture tick: it OG_CHECK-fails on a duplicate capture tick and hard-caps
// at kMaxSlots = 8. Correct for a payload rebuilt from scratch every send — fatal
// for a PERSISTENT replicated property written again on every newer capture tick,
// which would overflow and assert within 8 ticks. Do not "unify" the two.
//
// STRUCTURE MIRRORS FInputRedundancyBundle's split, for the same reasons: this
// USTRUCT owns storage (a UPROPERTY TArray<uint8>) + replication (custom
// NetSerialize), and DELEGATES all ring logic to the engine-agnostic core codec
// (OGSimulation/RelayedInputRingCodec.h). That split is what lets the pure-C++
// Low-Level-Tests exercise the real production logic — and, because a USTRUCT
// cannot be a template, it is also what lets ONE replicated property carry an
// arbitrary InputType.
//
// DIRTY DETECTION: every byte of ring state (version, entry count, entries) lives
// inside the single UPROPERTY, so UE's default property comparison sees any
// change to the ring and marks it dirty. (FSimulationInputSyncBuffer keeps its
// usedBytes watermark OUTSIDE the UPROPERTY and gets away with it because the
// payload bytes always change too; this type deliberately has no such shadow
// state.)
//
// REPLICATION CONDITION: registered with a plain DOREPLIFETIME and NO COND_ —
// unlike the (retired) COND_OwnerOnly tier, the whole point of the relay is that
// NON-owning clients receive it.
// ---------------------------------------------------------------------------
USTRUCT()
struct OGSIMULATIONUNREAL_API FRelayedInputRing
{
public:
	GENERATED_BODY()

	// Wire-format constants live in the engine-agnostic codec. These aliases
	// preserve an FRelayedInputRing::kWireFormatVersion / ::kMaxDepth surface that
	// mirrors FInputRedundancyBundle's.
	static constexpr uint8 kWireFormatVersion = relayedInputRing::kWireFormatVersion;
	static constexpr uint8 kMaxDepth          = relayedInputRing::kMaxDepth;

	// [T29] Both bounds moved into the codec beside the layout they bound, so the
	// malformed-length rule is reachable from the pure-C++ Low-Level Tests (a
	// USTRUCT is invisible to that target). These aliases keep the existing
	// FRelayedInputRing::kMaxInputBytes / ::kMaxWireBytes surface and the VALUES
	// ARE IDENTICAL — see the MALFORMED-LENGTH BOUND block in
	// OGSimulation/RelayedInputRingCodec.h for the anchoring rationale.
	static constexpr int32 kMaxInputBytes = static_cast<int32>(relayedInputRing::kMaxInputBytes);
	static constexpr int32 kMaxWireBytes  = static_cast<int32>(relayedInputRing::kMaxWireBytes);

	// [T29] The move above must not have moved the VALUE. 1066 is what the
	// expression that used to sit here evaluated to
	// (kHeaderBytes 2 + kMaxDepth 8 * (4 + 1 + kMaxInputBytes 128)), and it is what
	// the codec's expression evaluates to now. Pinning the literal is what makes a
	// silent drift in either operand a compile error rather than a quietly
	// loosened or tightened inbound bound.
	static_assert(kMaxWireBytes == 1066, "FRelayedInputRing malformed-length bound drifted");

	// --- Byte-buffer adapter surface required by the codec's BUFFER CONCEPT ----
	// (identical to FInputRedundancyBundle's, deliberately — see the codec header)
	int32 bundleByteNum() const { return wireBytes.Num(); }
	void  bundleAddZeroedBytes(int32 Count) { wireBytes.AddZeroed(Count); }

	template <typename T>
	void writeToBuffer(uint32 ByteIndex, const T& Value)
	{
		checkf(ByteIndex + sizeof(T) <= static_cast<uint32>(wireBytes.Num()),
			TEXT("FRelayedInputRing write OOB: offset=%u size=%llu capacity=%d"),
			ByteIndex, static_cast<uint64>(sizeof(T)), wireBytes.Num());

		const uint8* ValueAsBytes = reinterpret_cast<const uint8*>(&Value);
		for (int32 i = 0; i < sizeof(T); ++i)
			wireBytes[ByteIndex + i] = ValueAsBytes[i];
	}

	template <typename T>
	T readFromBuffer(uint32 ByteIndex) const
	{
		checkf(ByteIndex + sizeof(T) <= static_cast<uint32>(wireBytes.Num()),
			TEXT("FRelayedInputRing read OOB: offset=%u size=%llu capacity=%d"),
			ByteIndex, static_cast<uint64>(sizeof(T)), wireBytes.Num());

		T Value;
		uint8* ValueAsBytes = reinterpret_cast<uint8*>(&Value);
		for (int32 i = 0; i < sizeof(T); ++i)
			ValueAsBytes[i] = wireBytes[ByteIndex + i];
		return Value;
	}

	// --- Ring surface (thin delegation to the core codec) ---------------------

	// Server-side write. `depth` comes from TimeConfig::relayRedundancyDepthTicks
	// and is clamped to [1, kMaxDepth] by the codec. Returns false only when the
	// ring is at depth and `captureTick` is older than every resident entry — a
	// benign stale-write drop, not an error (see the codec's writeLatest doc).
	template <typename InputType>
	bool writeLatest(uint32 captureTick, uint8 dA, const InputType& input, int32 depth)
	{
		return relayedInputRing::writeLatest<InputType>(*this, captureTick, dA, input, depth);
	}

	// Capture-tick lookup. On a hit fills `outDA` + `outInput` and returns true.
	template <typename InputType>
	bool findEntry(uint32 captureTick, uint8& outDA, InputType& outInput) const
	{
		return relayedInputRing::findEntry<InputType>(*this, captureTick, outDA, outInput);
	}

	// Iterates every resident entry as callback(uint32 captureTick, uint8 dA,
	// const InputType&). Entries come in RING-POSITION order, which is NOT age
	// order — key by captureTick.
	template <typename InputType, typename Callback>
	void forEachEntry(Callback&& callback) const
	{
		relayedInputRing::forEachEntry<InputType>(*this, Forward<Callback>(callback));
	}

	// Number of resident entries (0 on a never-written ring).
	uint8 num() const { return relayedInputRing::entryCount(*this); }

	// Version byte as it arrived on the wire; 0 on a never-written ring.
	uint8 getWireFormatVersionOnWire() const { return relayedInputRing::getWireFormatVersion(*this); }

	// Length-prefixed network serialization. The ring's own version byte and entry
	// count are INSIDE the payload (the codec owns that header), so the only thing
	// the transport adds is the byte count — an empty ring costs 2 bytes.
	//
	// [T29] LIKE FSimulationStateSyncBuffer::NetSerialize, THIS WAS NOT BEING
	// CALLED. Iris replicated `wireBytes` member-wise and the oversize rejection
	// below never ran — T20 measured that and left it. The registration in
	// IrisNetSerializerRegistrations.cpp puts the function back in the loop; the
	// one-shot report below is the evidence.
	bool NetSerialize(FArchive& Ar, class UPackageMap* /*Map*/, bool& bOutSuccess)
	{
		uint16 used = static_cast<uint16>(wireBytes.Num());
		Ar << used;  // saving writes the count; loading overwrites it from the wire

		if (Ar.IsLoading())
		{
			// A length beyond what any well-formed sender can produce means a
			// corrupt or hostile bunch. Trimming it would desynchronize the rest of
			// the archive, so fail the bunch instead of guessing.
			//
			// [T29] The predicate is the codec's (Catch2-covered:
			// RelayRing.WireLengthBound*); the escalation stays here because it is
			// transport policy. Under Iris this archive is the LastResort
			// serializer's scratch FNetBitReader over an already-received blob, not
			// the bunch itself — see the RECEIVE-SIDE ESCALATION note in
			// IrisNetSerializerRegistrations.cpp.
			if (!relayedInputRing::isAcceptableWireLength(static_cast<std::uint32_t>(used)))
			{
				Ar.SetError();
				bOutSuccess = false;
				return true;
			}
			wireBytes.SetNumZeroed(used);
		}

		if (used > 0)
			Ar.Serialize(wireBytes.GetData(), used);

		// [T29] One-shot runtime proof, per direction AND per live/default payload.
		// Payload = uint16 length + `used` payload bytes; the member-wise alternative
		// would have carried wireBytes.Num() elements.
		//
		// `used` is the GATE. A never-written ring — including the CDO Iris quantizes
		// while building the class descriptor — has an EMPTY wireBytes (the codec's
		// initHeaderIfEmpty is deliberately lazy so an unwritten ring costs zero wire
		// bytes, not an empty header), so used == 0 and the acceptance latch is not
		// consumed. A header-only ring (used == 2, entryCount == 0) is not reachable:
		// writeLatest calls initHeaderIfEmpty and grows the first entry in the same
		// call, so any ring that has a header has at least one entry. See the
		// LIVE-PAYLOAD GATE block in IrisNetSerializerProof.h.
		{
			static ogIrisNetSerializerProof::FNetSerializeReportLatch reportLatch;
			ogIrisNetSerializerProof::reportNetSerializeOnce(
				reportLatch,
				TEXT("RelayedInputRing"), Ar.IsLoading(),
				static_cast<int32>(used),
				static_cast<int32>(sizeof(uint16)) + static_cast<int32>(used),
				wireBytes.Num());
		}

		bOutSuccess = true;
		return true;
	}

private:
	// The entire ring: [version u8][entryCount u8][entries...]. Grown on demand up
	// to the configured depth; never shrinks during a session.
	UPROPERTY()
	TArray<uint8> wireBytes;
};

// ⛔ [T29] UNDER IRIS, `WithNetSerializer` ALONE DOES NOTHING. Iris does not read
// this traits block to find a custom serializer — it resolves them from a NAME
// REGISTRY (FPropertyNetSerializerInfoRegistry). With no entry it builds a
// member-wise descriptor and NEVER CALLS NetSerialize, silently: no error, no
// compile failure, and the function still reads as live in source. That is exactly
// how this struct's malformed-length guard sat dead from T1 until T29.
//
// So: if this struct is used as a REPLICATED PROPERTY **OR AS AN RPC PARAMETER**
// (Iris builds a descriptor for a UFunction's parameters through the same
// resolution — RPC params are NOT exempt), it MUST have a matching registration in
// Source/OGSimulationUnreal/IrisNetSerializerRegistrations.cpp, added IN THE SAME
// EDIT. This one is registered.
template<>
struct TStructOpsTypeTraits<FRelayedInputRing> : public TStructOpsTypeTraitsBase2<FRelayedInputRing>
{
	enum { WithNetSerializer = true };
};
