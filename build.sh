#!/usr/bin/env bash
# Configure + build the 32-bit mod DLL and injector with mingw-w64.
set -euo pipefail
cd "$(dirname "$0")"

# Vendored deps (MinHook, ImGui) are git submodules; make sure they are checked
# out before configuring. No-op once initialized.
if [ -f "$(git rev-parse --show-toplevel 2>/dev/null)/.gitmodules" ]; then
  git submodule update --init --recursive
fi

BUILD_DIR=build
# Honor $CUBE_MOD_BUILD_TYPE (the CMakeLists fallback reads the same var); Release by default.
BUILD_TYPE="${CUBE_MOD_BUILD_TYPE:-Release}"
cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$(pwd)/cmake/toolchain-i686-mingw.cmake" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" >/dev/null

# Portable CPU count (nproc is coreutils/Linux; fall back for macOS/BSD).
JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
cmake --build "$BUILD_DIR" -j"$JOBS"

echo
echo "Artifacts:"
ls -la "$BUILD_DIR"/cube_mod.dll "$BUILD_DIR"/inject.exe 2>/dev/null || true
