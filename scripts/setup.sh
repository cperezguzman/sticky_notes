#!/usr/bin/env bash
# One-shot setup after cloning: check tools, build CLI (+ optional GUI), optional desktop launcher.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

INSTALL_DESKTOP=false
BUILD_GUI=true

usage() {
    cat <<'EOF'
Usage: ./scripts/setup.sh [options]

Build the project from a fresh clone. Safe to run again (skips work already done).

Options:
  --desktop   Install ~/.local/share/applications/sticky-notes.desktop (Linux app menu)
  --cli-only  Build only the terminal app (skip SDL3 + GUI; faster)
  -h, --help  Show this help

Examples:
  ./scripts/setup.sh
  ./scripts/setup.sh --desktop
  ./scripts/setup.sh --cli-only
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
	--desktop) INSTALL_DESKTOP=true ;;
	--cli-only) BUILD_GUI=false ;;
	-h | --help)
	    usage
	    exit 0
	    ;;
	*)
	    echo "Unknown option: $1" >&2
	    usage >&2
	    exit 1
	    ;;
    esac
    shift
done

print_missing_deps() {
    cat >&2 <<'EOF'

Install build tools, then rerun ./scripts/setup.sh

  Debian / Ubuntu:
    sudo apt update
    sudo apt install build-essential cmake curl

  Fedora:
    sudo dnf install gcc-c++ make cmake curl

  Arch:
    sudo pacman -S base-devel cmake curl
EOF
}

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
	echo "Missing required command: $1" >&2
	print_missing_deps
	exit 1
    fi
}

echo "==> Checking tools"
require_cmd g++
require_cmd make
if [[ "$BUILD_GUI" == true ]]; then
    require_cmd cmake
    require_cmd curl
fi

echo "==> Building CLI (sticky_notes)"
make sticky_notes

if [[ "$BUILD_GUI" == true ]]; then
    if [[ ! -f "$ROOT/third_party/sdl3-install/lib/libSDL3.so" ]] \
	&& [[ ! -f "$ROOT/third_party/sdl3-install/lib/libSDL3.so.0" ]]; then
	echo "==> Building SDL3 (one-time; may take a few minutes)"
	"$ROOT/scripts/build-sdl3.sh"
    else
	echo "==> SDL3 already present"
    fi
    echo "==> Building GUI (textbox_sandbox)"
    make textbox
    ln -sf "$ROOT/textbox_sandbox" "$ROOT/sticky-notes"
fi

if [[ "$INSTALL_DESKTOP" == true ]]; then
    echo "==> Installing desktop launcher"
    "$ROOT/scripts/install-desktop.sh"
fi

echo ""
echo "Setup complete."
echo ""
echo "  Terminal:  ./sticky_notes"
if [[ "$BUILD_GUI" == true ]]; then
    echo "  GUI:       ./scripts/run-sticky-notes-gui.sh"
    if [[ "$INSTALL_DESKTOP" == true ]]; then
	echo "  App menu:  search for \"Sticky Notes\""
    else
	echo "  App menu:  run ./scripts/setup.sh --desktop  (or make desktop)"
    fi
fi
echo ""
echo "Run commands from: $ROOT"
