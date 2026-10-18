#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/UEFMutexLockableUImpl-rationale.md · docs/UEFMutexLockableUImpl-guards.md

#include "Async/Mutex.h"
#include "OGSimulation/TickDriverThreadContext.h"

class UEFMutexLockableUImpl
{
public:
	void lock() { m_mutex.Lock(); }
	void unlock() { m_mutex.Unlock(); }

private:
	UE::FMutex m_mutex;
};

static_assert(BasicLockable<UEFMutexLockableUImpl>,
	"UEFMutexLockableUImpl must model og-simulation's BasicLockable (lock, unlock), the world lock type a JoltTickWorldHost "
	"takes. Was the brawler Jolt host's UE::TUniqueLock over its UE::FMutex world mutex (J2).");
