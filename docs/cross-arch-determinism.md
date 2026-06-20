<!-- SPDX-License-Identifier: MPL-2.0 -->
# Cross-Arch Determinism (KU-1) — Harness & Toolchain

This document describes the **KU1CrossArch** mode of the Stage 0 determinism
harness and the Android NDK toolchain that lets the pure-C++ cores be compiled for
ARM64. Together they let a future researcher produce **paired hash logs** from two
toolchains (MSVC-Win64 and Clang-ARM64-NDK) and diff them tick-by-tick.

> **Scope (FINAL doc §6).** This deliverable is the harness *mode* + the toolchain
> *file* only. Actual cross-arch **measurement** — building on real ARM64 hardware,
> running both binaries, and characterising the divergence — is **parallel-track
> research** (the KU-1 spike) and is **out of scope** for this initiative. Nothing
> here gate-fails on cross-arch divergence; see "Option 4" below.

---

## 1. Components

| Component | Path | Role |
|---|---|---|
| `KU1CrossArch` tests | `Source/OGSimulationTests/extern/og-simulation-tests/Source/OGSimulationTests/Determinism/DeterminismHarness_KU1.cpp` | Writes / compares binary hash logs. Tag `[Determinism][KU1CrossArch]` — **opt-in**, NOT in `[@og]`. |
| Harness façade | `.../Determinism/DeterminismHarness.h` | `runDeterminismLoop` + the 4-method-API `static_assert` (shared with Production/DevTest). |
| Android toolchain | `tools/cmake/android-ndk-clang.toolchain.cmake` | CMake cross-compile to `arm64-v8a` via the NDK Clang. |

The KU1 grid is the **same** seeded 600-tick input stream as DevTest's
`RandomInputGrid600Ticks` (seed `0x0FB1AC1E`, trivial POD `TestState`/`TestInput`,
accumulate integrate functor), so the two modes are directly comparable.

---

## 2. The two KU1 tests

### `Determinism.KU1.HashLogToFile`
Runs the canonical KU1 grid and writes a binary hash log:

- **Path:** env `OG_KU1_HASH_LOG`, or default `./ku1_hash_log_<arch>_<compiler>.bin`.
- **`<arch>_<compiler>`** and the in-file `build_id` are derived from compile-time
  macros (`__clang__` / `_MSC_VER` / `__aarch64__` / `_M_X64` …), so an MSVC-Win64
  run writes `ku1_hash_log_x64_msvc.bin` and a Clang-ARM64-NDK run writes
  `ku1_hash_log_arm64_clang.bin` — **distinct files, no manual disambiguation**.

### `Determinism.KU1.HashLogReplayMatch`
Compares two hash logs and reports the first divergent tick:

- **Paths:** env `OG_KU1_HASH_LOG_A` and `OG_KU1_HASH_LOG_B`.
- **Self-test (both env vars unset):** writes a fresh log and compares it against
  itself → **zero divergences** (order-independent; does not depend on
  `HashLogToFile` having run first). This is what the local MSVC verification run
  exercises.
- **Cross-arch (distinct paths):** reports the first divergent tick as a **warning
  (magnitude), and does NOT fail** — see Option 4.

---

## 3. Binary hash-log format

Little-endian (both target archs are LE):

```
offset  field          type      notes
0       magic          char[4]   "KU1H"
4       formatVersion  u32       = 1
8       buildId        u64       compiler-tag<<48 | version
16      tickCount      u32       = 600
20      records[]      {u32,u32} (tick, checksum) × tickCount
```

`HashLogReplayMatch` compares the per-tick **checksum** fields; the `buildId`
header differs between toolchains by design and is reported as context, not as a
divergence.

---

## 4. Cross-arch verification flow

```
# 1. Build the standalone core + tests for Win64 (MSVC) and run HashLogToFile:
OG_KU1_HASH_LOG=./logs/win64.bin  OGSimulationTests.exe "[Determinism][KU1CrossArch]"

# 2. Cross-compile the core for arm64-v8a and run HashLogToFile on-device / emulator:
cmake -S Plugins/OGBrawler -B build_arm64 \
      -DCMAKE_TOOLCHAIN_FILE=tools/cmake/android-ndk-clang.toolchain.cmake
cmake --build build_arm64
OG_KU1_HASH_LOG=./logs/arm64.bin  ./og_simulation_tests "[Determinism][KU1CrossArch]"

# 3. Diff the two logs (run HashLogReplayMatch with both env vars set):
OG_KU1_HASH_LOG_A=./logs/win64.bin OG_KU1_HASH_LOG_B=./logs/arm64.bin \
      OGSimulationTests.exe "[Determinism][KU1CrossArch]"
```

Step 3 prints the first divergent tick (if any) as a warning.

---

## 5. Option 4 vs Option 3 — why divergence does not fail

Under the **current strategy (Option 4)**, the simulation is not committed to
bit-identical cross-architecture determinism. Floating-point and codegen
differences between MSVC-x64 and Clang-ARM64 *can* legitimately produce divergent
checksums. The harness therefore **measures and reports** divergence magnitude
rather than treating it as a failure.

Under a **future Option 3 commitment** (full cross-arch determinism — e.g. via a
soft-float / fixed-point sim core), the same `HashLogReplayMatch` test would be
flipped to **gate-fail** on any divergence. The comparison logic is already in
place; only the assertion policy would change.

---

## 6. Android NDK toolchain

`tools/cmake/android-ndk-clang.toolchain.cmake` configures a standard CMake Android
cross-compile:

- `CMAKE_SYSTEM_NAME = Android`, `CMAKE_SYSTEM_VERSION = 24` (API 24 / Android 7.0).
- `CMAKE_ANDROID_ARCH_ABI = arm64-v8a`, `CMAKE_ANDROID_STL_TYPE = c++_static`.
- `CMAKE_ANDROID_NDK` resolved from env `ANDROID_NDK_ROOT` (fallback `ANDROID_NDK`);
  fatal error with guidance if unset.

```
cmake -S Plugins/OGBrawler -B build_arm64 \
      -DCMAKE_TOOLCHAIN_FILE=tools/cmake/android-ndk-clang.toolchain.cmake
```

A clean **configure** is the acceptance bar for Task 4 (no compile required). The
configure requires both the NDK (via `ANDROID_NDK_ROOT`) and the standalone CMake
runner for `Plugins/OGBrawler` to be present; when either is absent the check is
recorded as a skippable dry-run (see `impl/impl_notes_phase0_task4.md`).
