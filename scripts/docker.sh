#!/usr/bin/env bash
# One-command Docker workflow: build image (first time) and run CLI or GUI.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

MODE="${1:-gui}"

usage() {
    cat <<'EOF'
Usage: ./scripts/docker.sh [cli|gui|build]

  gui    Build image if needed, then run the SDL desk GUI (default)
  cli    Build image if needed, then run the terminal editor
  build  Build the image only

Notes:
  - Requires Docker (docker compose).
  - GUI mode needs a Linux host with DISPLAY and X11 socket (/tmp/.X11-unix).
  - Your notes persist in ./notes on the host.
EOF
}

compose() {
    if docker compose version >/dev/null 2>&1; then
	docker compose "$@"
    elif command -v docker-compose >/dev/null 2>&1; then
	docker-compose "$@"
    else
	echo "Docker Compose not found. Install Docker Desktop or docker-compose-plugin." >&2
	exit 1
    fi
}

require_docker() {
    if ! command -v docker >/dev/null 2>&1; then
	echo "Docker is not installed." >&2
	echo "  Debian/Ubuntu: https://docs.docker.com/engine/install/ubuntu/" >&2
	exit 1
    fi
    if ! docker info >/dev/null 2>&1; then
	echo "Docker daemon is not running (or you lack permission). Try: sudo usermod -aG docker \$USER" >&2
	exit 1
    fi
}

prepare_gui() {
    if [[ -z "${DISPLAY:-}" ]]; then
	echo "DISPLAY is not set — GUI mode needs an X11/Wayland session with XWayland." >&2
	exit 1
    fi
    if [[ ! -S /tmp/.X11-unix/X0 ]] && [[ ! -d /tmp/.X11-unix ]]; then
	echo "No X11 socket at /tmp/.X11-unix — GUI forwarding may fail." >&2
    fi
    if command -v xhost >/dev/null 2>&1; then
	xhost +local:docker >/dev/null 2>&1 || true
    fi
}

mkdir -p "$ROOT/notes"

case "$MODE" in
    -h | --help)
	usage
	exit 0
	;;
    build)
	require_docker
	echo "==> Building Docker image (first time may take several minutes)"
	compose build
	echo "Done. Run: ./scripts/docker.sh gui   or   ./scripts/docker.sh cli"
	;;
    cli)
	require_docker
	echo "==> Building Docker image if needed"
	compose build
	echo "==> Starting terminal editor (notes in ./notes)"
	compose run --rm sticky-notes-cli
	;;
    gui)
	require_docker
	prepare_gui
	echo "==> Building Docker image if needed"
	compose build
	echo "==> Starting GUI (notes in ./notes)"
	compose run --rm sticky-notes-gui
	;;
    *)
	echo "Unknown mode: $MODE" >&2
	usage >&2
	exit 1
	;;
esac
