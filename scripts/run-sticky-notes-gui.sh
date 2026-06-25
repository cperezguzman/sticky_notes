#!/usr/bin/env bash
# Launch the SDL GUI from the repo root so notes/ paths resolve correctly.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BIN="$ROOT/textbox_sandbox"
GUI_NAME="$ROOT/sticky-notes"

if [[ ! -x "$BIN" ]]; then
    if [[ ! -f "$ROOT/third_party/sdl3-install/lib/libSDL3.so" ]] \
	&& [[ ! -f "$ROOT/third_party/sdl3-install/lib/libSDL3.so.0" ]]; then
	echo "Building SDL3 (one-time)..." >&2
	"$ROOT/scripts/build-sdl3.sh"
    fi
    echo "Building GUI..." >&2
    make -C "$ROOT" textbox
fi

ln -sf "$BIN" "$GUI_NAME"

exec env SDL_APP_ID=sticky-notes SDL_APP_NAME="Sticky Notes" "$GUI_NAME"
