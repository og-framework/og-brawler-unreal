// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

#include <atomic>

// ---------------------------------------------------------------------------
// [og-netcode-v2-input-relay / T29] RUNTIME PROOF THAT NetSerialize IS CALLED
// ON A REAL PAYLOAD.
//
// WHY THIS EXISTS AT ALL. The defect T29 fixes was SILENT: Iris built a
// member-wise descriptor for FSimulationStateSyncBuffer and FRelayedInputRing
// and never called their NetSerialize, so the wire-format version fence and the
// malformed-length guard inside those functions were dead code that still read
// as live in source. The fix (IrisNetSerializerRegistrations.cpp) is a
// registration macro. A registration macro that COMPILES proves nothing, and a
// vanished `LogIris: Warning: Generating descriptor for struct X that has custom
// serialization.` proves only that Iris found *a* serializer — not that it found
// OURS. So the acceptance evidence has to come from inside the function itself.
//
// ---------------------------------------------------------------------------
// ⭐ THE LIVE-PAYLOAD GATE, AND WHY IT IS NOT OPTIONAL (T29 rework).
//
// The first version of this instrumentation latched on the FIRST CALL OF EACH
// DIRECTION, and that reproduced the very failure mode the task exists to close:
// an instrument reporting something weaker than it claimed.
//
// Iris calls our NetSerialize long before any replication happens. When it builds
// a class descriptor it immediately constructs the default state —
// AllocateAndInitializeDefaultInternalStateBuffer (ReplicationStateDescriptorBuilder.cpp,
// unconditional for class descriptors; DefaultStateSourceData comes from the CDO)
// loops every member and calls Serializer->Quantize. And
// FLastResortPropertyNetSerializer::Quantize has NO IsInitializingDefaultState()
// guard — only its Serialize/Deserialize do — so it calls
// Property->NetSerializeItem on the CDO's value, into a scratch FNetBitWriter,
// AT DESCRIPTOR-BUILD TIME, ON EVERY PROCESS THAT BUILDS THE DESCRIPTOR.
//
// A real PIE run (2026-08-07) confirmed it exactly: all four latches in each
// client process fired inside a 3 ms burst on ONE frame, 636 ms after "Welcomed
// by server" and ~140 ms BEFORE the first correction was published — and every
// line read payloadBytes=3 (usedBytes=0) for the correction buffer and
// payloadBytes=2 (usedBytes=0) for the ring. 2,676 further correction/relay
// updates followed and the latch never fired again. So the ONLY thing the run
// demonstrated was that our serializer had been RESOLVED — never that a real
// payload had gone through it.
//
// Hence the gate: the acceptance latch arms only on a call that carries
// usedBytes > 0 (see carriesLivePayload). Default-state / empty-buffer calls take
// a SEPARATE one-shot latch, are labelled `state=default`, and are logged at Log
// rather than Display so they cannot be mistaken for the acceptance line.
//
// WHAT EACH LINE PROVES, precisely — do not overclaim either one:
//   state=default dir=Save  Iris resolved OUR serializer and called it while
//                           quantizing the CDO. Real, and it is the ONLY signal
//                           the pre-gate build ever produced. NOT replication.
//   state=default dir=Load  a Dequantize outside descriptor construction (there
//                           is NO Dequantize in ReplicationStateDescriptorBuilder.cpp
//                           at all) — our load path ran, but on an empty buffer.
//                           It does NOT distinguish "applied a received payload"
//                           from "applied the default state to a fresh object".
//   state=live    dir=Save  our encoder produced a real payload for the wire.
//   state=live    dir=Load  ⭐ THE ACCEPTANCE LINE. A non-empty payload was
//                           decoded through our NetSerialize. This one cannot be
//                           produced by default-state construction.
//
// ONE-SHOT, NOT THROTTLED. These serialize once per character per replication
// poll (60 Hz x N characters). A per-call log would be a multi-megabyte log file
// in under two minutes; each flag is set once per (struct, direction, live/default)
// per process and never cleared.
//
// COST WHEN QUIET: one relaxed atomic load per NetSerialize call after the first.
// ---------------------------------------------------------------------------

OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGIrisNet, Log, All);

namespace ogIrisNetSerializerProof
{
	// ⭐ THE PREDICATE THE ACCEPTANCE EVIDENCE RESTS ON. `usedPayloadBytes` is the
	// struct's own watermark/length field — the count it just wrote to, or just
	// read from, the wire — NOT including the length prefix or version byte. A
	// default-constructed CDO answers 0 for both structs (the correction buffer's
	// usedBytes starts at 0; the ring's wireBytes is empty), which is exactly the
	// case that must NOT arm the acceptance latch.
	constexpr bool carriesLivePayload(int32 usedPayloadBytes)
	{
		return usedPayloadBytes > 0;
	}

	// REGRESSION GUARD for the distinction the 2026-08-07 PIE run exposed. This
	// pins "the report predicate rejects an empty/default buffer and accepts a
	// populated one" — the property whose absence made a default-state artefact
	// readable as live-decode proof.
	//
	// Why a static_assert and not a Catch2 case: both Low-Level-Test targets
	// (OGSimulationTests, OGBrawlerTests) are built with bCompileAgainstEngine =
	// false / bCompileAgainstCoreUObject = false and depend only on Core +
	// OGSimulation — they cannot see this header, or any UE module, at all. A
	// compile-time assertion on a constexpr predicate is the same coverage
	// evaluated on EVERY build of EVERY target, including the two game targets the
	// Catch2 suites never touch.
	static_assert(!carriesLivePayload(0),
		"An empty/default-constructed buffer must never arm the live-payload latch — "
		"that is the Iris default-state artefact T29's rework exists to exclude.");
	static_assert(!carriesLivePayload(-1),
		"A negative byte count is not a live payload.");
	static_assert(carriesLivePayload(1),
		"A single carried byte is a live payload.");
	static_assert(carriesLivePayload(308),
		"The shipped correction-state watermark (tick 4 + ref 4 + composite 300) is a live payload.");
	static_assert(carriesLivePayload(83),
		"The shipped depth-1 relay ring (header 2 + one 81 B entry) is a live payload.");

	// One-shot state for a single NetSerialize call site: four independent latches,
	// so an empty/default-state call can never consume the acceptance one-shot.
	// Declare as a function-local static at the call site — C++11 magic-static init
	// is thread-safe, and an inline member function gets exactly one copy across
	// TUs, so this is genuinely once per process rather than once per TU. Iris does
	// not guarantee the game thread; the latching itself is atomic.
	struct FNetSerializeReportLatch
	{
		std::atomic<bool> liveSave{false};
		std::atomic<bool> liveLoad{false};
		std::atomic<bool> defaultSave{false};
		std::atomic<bool> defaultLoad{false};
	};

	// Emits the one-shot `[IrisSerializerProof]` line for one
	// (struct, direction, live/default) triple. `netSerializePayloadBytes` is the
	// TOTAL byte count THIS struct's NetSerialize moved (its own framing included);
	// `arrayElementCount` is the live element count of the UPROPERTY TArray<uint8>
	// Iris would otherwise have replicated member-wise.
	//
	// The before/after wire accounting is derived and logged ONLY for a live
	// payload. On a default-state call those figures are meaningless — the run
	// above printed memberWise=1 for an empty ring and delta=-381.6 B for an empty
	// correction buffer — so they are suppressed rather than printed and disowned.
	OGSIMULATIONUNREAL_API void reportNetSerialize(
		const TCHAR* structName,
		bool bLoading,
		bool bLivePayload,
		int32 netSerializePayloadBytes,
		int32 arrayElementCount);

	// Fires `reportNetSerialize` at most once per (call site, direction,
	// live/default). `usedPayloadBytes` is the gate input; see carriesLivePayload.
	inline void reportNetSerializeOnce(
		FNetSerializeReportLatch& latch,
		const TCHAR* structName,
		bool bLoading,
		int32 usedPayloadBytes,
		int32 netSerializePayloadBytes,
		int32 arrayElementCount)
	{
		const bool bLivePayload = carriesLivePayload(usedPayloadBytes);

		std::atomic<bool>& fired =
			bLivePayload ? (bLoading ? latch.liveLoad    : latch.liveSave)
			             : (bLoading ? latch.defaultLoad : latch.defaultSave);

		if (fired.load(std::memory_order_relaxed))
			return;
		if (fired.exchange(true, std::memory_order_relaxed))
			return;

		reportNetSerialize(structName, bLoading, bLivePayload, netSerializePayloadBytes, arrayElementCount);
	}

	// ------------------------------------------------------------------------
	// THE VERSION-FENCE FORCING LEVER (debug only; default OFF = byte-identical).
	//
	// `FSimulationStateSyncBuffer::kWireFormatVersion` is a compile-time constant,
	// and every PIE process — the dedicated server and both clients — runs THE SAME
	// BINARY, so all of them hold the same copy of that constant. Editing it can
	// therefore never produce a mismatch to test against, no matter how the PIE
	// session is laid out across processes. (This used to say "one process". That
	// premise is FALSE for this project — Saved/Config/WindowsEditor/
	// EditorPerProjectUserSettings.ini sets RunUnderOneProcess=False with a separate
	// dedicated server. ONE BINARY is the property that actually matters, and it
	// holds either way.)
	//
	// The console variable `og.net.ForceCorrectionWireVersionOffset` perturbs only
	// the byte the SAVE path writes, which is exactly the byte a mismatched build
	// would differ in, and leaves the local expectation alone. A receiving client
	// then takes the real USimmableUpdateComponent::OnRep_CorrectionState refusal
	// path.
	//
	// ⚠ IT MUST BE SET AT RUNTIME, IN THE PROCESS THAT SAVES, AND NEVER FROM AN .ini
	// OR -ExecCmds AT STARTUP. Two independent reasons:
	//   * SCOPE — the cvar is process-global, not session-global. Only the server
	//     saves this buffer (DOREPLIFETIME, server->client, one instance), so under
	//     the project's default multi-process PIE layout a client console cannot
	//     reach the saver and the fence will silently appear dead.
	//   * ARMED-AT-STARTUP BREAKS THE CONNECTION INSTEAD OF FIRING THE FENCE — Iris
	//     quantizes the CDO through this same save path when it builds the class
	//     descriptor (AllocateAndInitializeDefaultInternalStateBuffer). An offset
	//     already armed at that moment bakes the wrong version byte into
	//     Descriptor->DefaultStateBuffer, which changes
	//     DescriptorIdentifier.DefaultStateHash on the server only — producing an
	//     Iris PROTOCOL MISMATCH rather than the OnRep refusal path you were trying
	//     to observe.
	//
	// At the default of 0 this returns its argument unchanged, so the shipped wire
	// bytes are identical to the pre-T29 encoder's.
	// ------------------------------------------------------------------------
#if !UE_BUILD_SHIPPING
	OGSIMULATIONUNREAL_API uint8 applyForcedWireVersionOffset(uint8 localVersion);
#else
	FORCEINLINE uint8 applyForcedWireVersionOffset(uint8 localVersion) { return localVersion; }
#endif
}
