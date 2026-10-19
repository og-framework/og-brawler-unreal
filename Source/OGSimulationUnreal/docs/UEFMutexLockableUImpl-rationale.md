<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UEFMutexLockableUImpl.h` — rationale

The wrapper gives Unreal's `UE::FMutex` the two lower-case members of og-simulation's `BasicLockable` concept
(`TickDriverThreadContext.h`), so an Unreal host can pass it as the world lock of a tick-driver world host. The source
holds the licence, a docs pointer, the class and one `static_assert`; this file carries the why. It has no
prohibition (`UEFMutexLockableUImpl-guards.md` is empty).

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## 1. Why a wrapper

og-simulation's `WorldLockScope<LockT, AccessScopeT>` locks with `std::lock_guard<LockT>`, and every world host that
takes a lock type (og-simulation-jolt's `JoltTickWorldHost` is the first) requires `BasicLockable<LockT>`: `lock()` and
`unlock()`, as the standard library's lockables have. `UE::FMutex` has TryLock, Lock and Unlock (and
IsLocked) and nothing lower-case, so it does not model the concept, and og-simulation's own self-check pins that a
capitalised-only lock is refused. The wrapper forwards `lock` to Lock and `unlock` to Unlock and adds nothing.

## 2. Why `UE::FMutex` and not `std::mutex`

The equivalence point: before og-tickdriver task 10 the brawler's Jolt host took its world mutex (J2) as a
`UE::FMutex` through UE::TUniqueLock, which calls Lock when built and Unlock when destroyed. The world host's
`WorldLockScope` makes the same two calls through this wrapper, so the lock, its wait behaviour and its cost are
unchanged. `UE::FMutex` is not recursive; the scope checks the role's held bit before it locks, so a second lock on
one thread is an `OG_CHECK`, not a deadlock (`TickDriverThreadContext-rationale.md` §5).

## 3. The compile-time check

The header's `static_assert` holds the wrapper to `BasicLockable` where it is declared, so a rename of either member
fails here and not first in a world host's `requires` clause. The wrapper is default-constructible and not copyable,
because `UE::FMutex` is both; a world host holds it by value as its own member.

## 4. Placement and callers

It is host code: it names an Unreal type and lives in the MPL-2.0 OGSimulationUnreal module. It names no game type
and no physics backend. Its caller today is the brawler's Jolt host, which instantiates
`JoltTickWorldHost<SimulatableBrawler, UEFMutexLockableUImpl, SimulationEngineUImpl>` (`TickDriverBackendUImpl.h`). It is
header-only, so it exports nothing.

## 5. Provenance

Written by og-tickdriver task 10 (design decision K8: the J2 lock is the world host's template parameter and must
model `BasicLockable`; the Unreal side passes this six-line wrapper over `UE::FMutex`).
