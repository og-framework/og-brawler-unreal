// SPDX-License-Identifier: MPL-2.0

#include "IrisNetSerializerProof.h"

#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogOGIrisNet);

namespace ogIrisNetSerializerProof
{
	namespace
	{
		// --- WIRE-COST ACCOUNTING ------------------------------------------------
		// Both helpers are transcriptions of UE 5.6.1 Iris source, cited inline, so
		// the numbers in the log line are the engine's own arithmetic applied to
		// LIVE values rather than a spreadsheet estimate. They are only ever used to
		// print a diagnostic — nothing on the wire depends on them.

		int32 bitsNeeded(uint32 value)
		{
			// Mirrors UE::Net::GetBitsNeeded.
			return value == 0u ? 0 : static_cast<int32>(FMath::FloorLog2(value)) + 1;
		}

		// What Iris spends on a `UPROPERTY TArray<uint8>` when it falls through to
		// the member-wise descriptor — i.e. the PRE-T29 cost of these two structs.
		//
		// FArrayPropertyNetSerializer::Serialize (Iris/Serialization,
		// ArrayPropertyNetSerializer.cpp:81-136):
		//   1 bit  "array is empty" flag
		//   Config->ElementCountBitCount bits for the element count. That is
		//     GetBitsNeeded(MaxElementCount) with MaxElementCount = 65535
		//     (ReplicationStateDescriptorBuilder.cpp:1467-1468) => 16 bits.
		//   then EVERY element, unconditionally, at 8 bits each.
		//
		// "Unconditionally" is load-bearing and was worth verifying rather than
		// inheriting: the per-element changemask branch is taken only when
		// `Args.ChangeMaskInfo.BitCount > 1`. A TArray gets 1 + 63 changemask bits
		// ONLY when it is a direct member of the replication state
		// (ReplicationStateDescriptorBuilder.cpp:2133-2139). Ours is nested inside a
		// struct, and FStructNetSerializer::Serialize forwards the STRUCT's
		// changemask info to its members verbatim (StructNetSerializer.cpp) — and a
		// plain struct member gets exactly 1 bit. So BitCount == 1, the changemask
		// pointer is null, and the whole array is written every single time. The
		// backlog's fallback argument (that the "simple modulo scheme" would dirty
		// every bit class anyway) is therefore not even reached.
		int32 memberWiseBits(int32 arrayElementCount)
		{
			if (arrayElementCount <= 0)
				return 1;

			return 1 + 16 + 8 * arrayElementCount;
		}

		// What the registered LastResort serializer spends carrying the blob our
		// NetSerialize produced — i.e. the POST-T29 cost.
		//
		// FLastResortPropertyNetSerializer::Serialize
		// (LastResortPropertyNetSerializer.cpp:79-111):
		//   FIrisPackageMapExportsUtil::Serialize -> 2 WriteBool with no exports
		//     (object references, names). NetTokens are appended out-of-band.
		//   WritePackedUint32(BitCount) -> 2 bits of byte-count selector plus
		//     8 * ceil(bitsNeeded(BitCount | 1) / 8) bits (NetBitStreamUtil.cpp).
		//   then the blob itself.
		int32 lastResortBits(int32 netSerializePayloadBytes)
		{
			const int32 blobBits = 8 * netSerializePayloadBytes;
			const int32 packedValueBytes =
				FMath::DivideAndRoundUp(bitsNeeded(static_cast<uint32>(blobBits) | 1u), 8);

			return 2 /*export bools*/ + 2 /*packed selector*/ + 8 * packedValueBytes + blobBits;
		}
	}

	void reportNetSerialize(
		const TCHAR* structName,
		bool bLoading,
		bool bLivePayload,
		int32 netSerializePayloadBytes,
		int32 arrayElementCount)
	{
		const TCHAR* const dir = bLoading ? TEXT("Load") : TEXT("Save");

		if (!bLivePayload)
		{
			// THE DEFAULT-STATE SIGNATURE — a real result, but a LESSER one, and it
			// must not be readable as the acceptance line. See the LIVE-PAYLOAD GATE
			// block in the header for what this does and does not prove.
			//
			// Log, not Display, and not Verbose. Display is reserved for the
			// acceptance line so the two are never confused at a glance. Verbose was
			// considered and rejected: LogOGIrisNet's compiled-in default verbosity is
			// Log, so a Verbose line would be INVISIBLE in a default PIE log and would
			// need an explicit `-LogCmds="LogOGIrisNet Verbose"` — which loses the
			// registration-resolved signal for exactly the user who just ran the
			// scenario and wants to know why the acceptance line is missing. Log still
			// lands in the .log file and is still greppable; the `state=default` label
			// and the text below carry the distinction.
			UE_LOG(LogOGIrisNet, Log,
				TEXT("[IrisSerializerProof] struct=%s dir=%s state=default NetSerialize was RESOLVED AND CALLED, ")
				TEXT("but on an EMPTY buffer (usedBytes=0) — payloadBytes=%d is framing only, arrayElements=%d. ")
				TEXT("This is the Iris DEFAULT-STATE signature (descriptor build quantizes the CDO through us); ")
				TEXT("it proves the registration resolved to OUR serializer, NOT that anything replicated. ")
				TEXT("Wire accounting is suppressed here because it would be meaningless on an empty payload. ")
				TEXT("The acceptance line is `state=live dir=Load`."),
				structName,
				dir,
				netSerializePayloadBytes,
				arrayElementCount);
			return;
		}

		const int32 beforeBits = memberWiseBits(arrayElementCount);
		const int32 afterBits  = lastResortBits(netSerializePayloadBytes);

		// Display, not Log: this is one-shot acceptance evidence and must survive a
		// default PIE log with no verbosity overrides. (LogOGNet is demoted to
		// Warning in Config/DefaultEngine.ini, which is exactly the trap this
		// avoids — a proof nobody can see proves nothing.)
		UE_LOG(LogOGIrisNet, Display,
			TEXT("[IrisSerializerProof] struct=%s dir=%s state=live NetSerialize IS BEING CALLED ON A REAL PAYLOAD. ")
			TEXT("payloadBytes=%d arrayElements=%d | wire bits: memberWise(pre-T29)=%d lastResort(post-T29)=%d delta=%+d bits (%+.1f B)"),
			structName,
			dir,
			netSerializePayloadBytes,
			arrayElementCount,
			beforeBits,
			afterBits,
			afterBits - beforeBits,
			static_cast<double>(afterBits - beforeBits) / 8.0);
	}

#if !UE_BUILD_SHIPPING
	namespace
	{
		int32 GForcedWireVersionOffset = 0;

		FAutoConsoleVariableRef CVarForcedWireVersionOffset(
			TEXT("og.net.ForceCorrectionWireVersionOffset"),
			GForcedWireVersionOffset,
			TEXT("DEBUG ONLY. Added to the wire-format version byte that ")
			TEXT("FSimulationStateSyncBuffer::NetSerialize WRITES, without changing what this ")
			TEXT("build EXPECTS, to make every connected client take ")
			TEXT("USimmableUpdateComponent::OnRep_CorrectionState's build-mismatch refusal path ")
			TEXT("(error + on-screen toast). 0 (default) = byte-identical to the shipped encoder. ")
			TEXT("This exists because every PIE process runs the SAME BINARY and so holds the same ")
			TEXT("copy of the compile-time kWireFormatVersion; editing that constant can never ")
			TEXT("produce a mismatch to test against. ")
			TEXT("SET IT IN THE PROCESS THAT SAVES: only the server saves this buffer ")
			TEXT("(DOREPLIFETIME, server->client), and this cvar is process-global, not ")
			TEXT("session-global — typing it into a CLIENT console under the project's default ")
			TEXT("multi-process PIE layout does nothing and looks identical to a dead fence. ")
			TEXT("NEVER SET IT FROM AN .ini OR FROM -ExecCmds AT STARTUP: Iris quantizes the CDO ")
			TEXT("through this same save path while building the class descriptor, so an offset ")
			TEXT("armed that early bakes a wrong version byte into DefaultStateBuffer, changes ")
			TEXT("DescriptorIdentifier.DefaultStateHash on the server only, and yields an Iris ")
			TEXT("PROTOCOL MISMATCH instead of the fence firing."),
			ECVF_Cheat);

		std::atomic<bool> GForcedOffsetWarned{false};
	}

	uint8 applyForcedWireVersionOffset(uint8 localVersion)
	{
		const int32 offset = GForcedWireVersionOffset;
		if (offset == 0)
			return localVersion;

		if (!GForcedOffsetWarned.exchange(true, std::memory_order_relaxed))
		{
			UE_LOG(LogOGIrisNet, Warning,
				TEXT("[IrisSerializerProof] og.net.ForceCorrectionWireVersionOffset=%d is ACTIVE — ")
				TEXT("correction state is being sent with a DELIBERATELY WRONG wire-format version ")
				TEXT("byte (%u instead of %u). Every receiving client will refuse corrections. ")
				TEXT("Set it back to 0 to restore normal play."),
				offset,
				static_cast<uint32>(static_cast<uint8>(localVersion + offset)),
				static_cast<uint32>(localVersion));
		}

		return static_cast<uint8>(localVersion + offset);
	}
#endif
}
