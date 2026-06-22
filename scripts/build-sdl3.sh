#!/usr/bin/env bash
# Build SDL3 into third_party/sdl3-install (one-time). Requires cmake and a C compiler.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="3.2.8"
SRC_DIR="$ROOT/third_party/SDL3-${VERSION}"
PREFIX="$ROOT/third_party/sdl3-install"
TARBALL="/tmp/SDL3-${VERSION}.tar.gz"

if [[ -f "$PREFIX/lib/libSDL3.so" || -f "$PREFIX/lib/libSDL3.a" ]]; then
    echo "SDL3 already installed at $PREFIX"
    exit 0
fi

mkdir -p "$ROOT/third_party"
if [[ ! -d "$SRC_DIR" ]]; then
    curl -fsSL -o "$TARBALL" "https://github.com/libsdl-org/SDL/releases/download/release-${VERSION}/SDL3-${VERSION}.tar.gz"
    tar -xzf "$TARBALL" -C "$ROOT/third_party"
fi

cmake -S "$SRC_DIR" -B "$SRC_DIR/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DSDL_SHARED=ON \
    -DSDL_STATIC=ON

cmake --build "$SRC_DIR/build" -j"$(nproc)"
cmake --install "$SRC_DIR/build"
echo "SDL3 installed to $PREFIX"
