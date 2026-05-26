#pragma once

// SPDX-License-Identifier: MPL-2.0

#include <atomic>
#include <cstdint>

// ChaosTickMapper — maps between Chaos physics frame numbers and simulation ticks.
//
// Chaos frames and simulation ticks are independent counters that may start at
// different values and can diverge during resimulation.  This class stores the
// constant offset (chaosTick - simulationTick) atomically so that
// TriggerRewindIfNeeded_Internal can safely read it from the physics thread.
//
// std::memory_order_relaxed is intentional: one-frame staleness is harmless
// because the offset only ever changes by 0 or 1 per frame.
class ChaosTickMapper
{
public:
    // Store the latest chaosTick - simulationTick offset.
    // Call once per non-resimulating physics step.
    void update(int32_t chaosTick, int32_t simulationTick)
    {
        m_offset.store(chaosTick - simulationTick, std::memory_order_relaxed);
    }

    // Convert a simulation tick to the corresponding Chaos frame number.
    int32_t toChaosTick(int32_t simulationTick) const
    {
        return simulationTick + m_offset.load(std::memory_order_relaxed);
    }

    // Convert a Chaos frame number to the corresponding simulation tick.
    int32_t toSimulationTick(int32_t chaosTick) const
    {
        return chaosTick - m_offset.load(std::memory_order_relaxed);
    }

private:
    std::atomic<int32_t> m_offset{0};
};
