// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "OGSimulationUnreal.h"
#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include <stdexcept>

#include "OGSimulation/PCTimeManagement/ServerTickClock.h"
#include "OGSimulation/SimulationComposite.h"

#include "SyncedSimulationStateBuffer.generated.h"

USTRUCT()
struct OGSIMULATIONUNREAL_API FSimulationStateSyncBuffer
{
public:
	GENERATED_BODY()

	// Wire format budget. Must hold the largest composite the project replicates
	// through this buffer (simulatableBrawler::State / PlayerInput).
	//
	// 2026-06-10: previously bumped to 1024 to fit brawlerProjectileSimulation's
	// State+IC additions; that triggered ~7x MaxClientRate overrun on the
	// 100Hz Reliable input RPC (see OGBrawlerNetworkModelResearch /
	// research/spike_input_rpc_saturation.md). Reverted to 256 after un-wiring
	// the projectile sub-sim from the brawler composite. Do NOT bump this
	// without coordinating with the OGBrawlerNetworkModelResearch wire-format
	// decisions (split buffer types vs custom NetSerialize vs unreliable
	// transport).
	static constexpr int32 kBufferBytes = 256;

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

	// Wire format: tick (uint32) at offset 0, then per-field serialized composite.
	// Generic on SimulationComposite<Ts...> so the same method handles every
	// simulatable's State/InputType without pulling simulation-specific writer
	// functions into the transport.
	template <typename... Ts>
	void write(const SimulationComposite<Ts...>& value, uint32_t tick)
	{
		uint32 offset = 0;
		writeToBuffer(offset, tick);
		offset += sizeof(uint32);
		writeCompositeToSyncedBuffer(value, *this, offset);
	}

	// Symmetric counterpart to write(composite, tick). Deduces Ts... from the
	// out-parameter and returns the tick that was read from byte 0.
	template <typename... Ts>
	uint32_t readInto(SimulationComposite<Ts...>& outValue) const
	{
		uint32 offset = 0;
		const uint32 tick = readFromBuffer<uint32>(offset);
		offset += sizeof(uint32);
		readCompositeFromSyncedBuffer(outValue, *this, offset);
		return tick;
	}

private:
	UPROPERTY()
	TArray<uint8> buffer;
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

