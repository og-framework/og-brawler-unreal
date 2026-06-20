# SPDX-License-Identifier: MPL-2.0
#
# Android NDK (Clang/ARM64) cross-compile toolchain for the pure-C++ cores.
# ---------------------------------------------------------------------------
# Lets a researcher compile the standalone og-simulation / og-brawler cores (and
# the Catch2 KU1CrossArch determinism harness) for arm64-v8a Android with the NDK
# Clang, so the cross-arch hash logs can be produced without re-deriving the build
# setup. This is the (ii) deliverable of Backlog Task 4; actual cross-arch
# MEASUREMENT on ARM64 hardware is parallel-track research (FINAL doc §6) and is
# OUT OF SCOPE here.
#
# Usage:
#   cmake -S Plugins/OGBrawler -B build_arm64 \
#         -DCMAKE_TOOLCHAIN_FILE=tools/cmake/android-ndk-clang.toolchain.cmake
#
# Requires the Android NDK (r23+ recommended) and the ANDROID_NDK_ROOT environment
# variable pointing at its root (e.g. .../Android/Sdk/ndk/26.1.10909125). CMake's
# built-in Android support (CMAKE_SYSTEM_NAME=Android) drives the NDK Clang
# directly — no external android.toolchain.cmake is required.
#
# See docs/cross-arch-determinism.md for the full cross-arch verification flow.
# ---------------------------------------------------------------------------

set(CMAKE_SYSTEM_NAME      Android)
set(CMAKE_SYSTEM_VERSION   24)             # Android 7.0 (API 24) minimum
set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)      # 64-bit ARM
set(CMAKE_ANDROID_STL_TYPE c++_static)     # static libc++ — no STL .so dependency

# Resolve the NDK root from the environment. ANDROID_NDK_ROOT is the canonical var;
# fall back to ANDROID_NDK for convenience.
if(DEFINED ENV{ANDROID_NDK_ROOT})
    set(CMAKE_ANDROID_NDK "$ENV{ANDROID_NDK_ROOT}")
elseif(DEFINED ENV{ANDROID_NDK})
    set(CMAKE_ANDROID_NDK "$ENV{ANDROID_NDK}")
endif()

if(NOT CMAKE_ANDROID_NDK)
    message(FATAL_ERROR
        "Android NDK not found. Set the ANDROID_NDK_ROOT environment variable to the "
        "NDK root (e.g. .../Android/Sdk/ndk/<version>) before configuring with this "
        "toolchain. See docs/cross-arch-determinism.md.")
endif()

# Use the NDK's bundled Clang/LLVM toolchain explicitly.
set(CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION clang)
