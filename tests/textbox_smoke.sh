#!/usr/bin/env bash
# Phase 4 GUI checklist — headless event harness (no display required).
# Run from repo root: ./tests/textbox_smoke.sh  or  make textbox-smoke
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BIN="./textbox_test_harness"

if [[ ! -x "$BIN" ]]; then
    if [[ ! -f "$ROOT/third_party/sdl3-install/lib/libSDL3.so" ]] \
	&& [[ ! -f "$ROOT/third_party/sdl3-install/lib/libSDL3.so.0" ]]; then
	echo "SDL3 not built — run ./scripts/build-sdl3.sh first" >&2
	exit 1
    fi
    make textbox_test_harness
fi

exec "$BIN"
