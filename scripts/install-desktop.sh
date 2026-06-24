#!/usr/bin/env bash
# Install a user-level desktop launcher (no sudo).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DESKTOP_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
ICON_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/256x256/apps"
ICON_SRC="$ROOT/assets/sticky-notes.png"
LAUNCHER="$ROOT/scripts/run-sticky-notes-gui.sh"

if [[ ! -f "$ICON_SRC" ]]; then
    echo "Missing icon: $ICON_SRC" >&2
    exit 1
fi

chmod +x "$LAUNCHER"

mkdir -p "$DESKTOP_DIR" "$ICON_DIR"
cp "$ICON_SRC" "$ICON_DIR/sticky-notes.png"

cat > "$DESKTOP_DIR/sticky-notes.desktop" <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Sticky Notes
GenericName=Notes
Comment=Local sticky notes desk and pop-out editor
Exec=$LAUNCHER
Path=$ROOT
Icon=$ICON_DIR/sticky-notes.png
Terminal=false
StartupNotify=true
Categories=Office;Utility;
Keywords=notes;sticky;memo;notepad;
EOF

chmod 644 "$DESKTOP_DIR/sticky-notes.desktop"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true
fi

echo "Installed launcher: $DESKTOP_DIR/sticky-notes.desktop"
echo "Icon: $ICON_DIR/sticky-notes.png"
echo ""
echo "Open your app menu and search for \"Sticky Notes\"."
echo "First launch may take a moment while the GUI binary is built."
