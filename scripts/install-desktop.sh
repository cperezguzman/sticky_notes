#!/usr/bin/env bash
# Install a user-level desktop launcher (no sudo).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DESKTOP_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
ICON_THEME_ROOT="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor"
ICON_SRC="$ROOT/assets/sticky-notes.png"
LAUNCHER="$ROOT/scripts/run-sticky-notes-gui.sh"
APP_ID="sticky-notes"
DESKTOP_FILE="$DESKTOP_DIR/${APP_ID}.desktop"
ICON_THEME_NAME="sticky-notes"

if [[ ! -f "$ICON_SRC" ]]; then
    echo "Missing icon: $ICON_SRC" >&2
    exit 1
fi

chmod +x "$LAUNCHER"

# Symlink so SDL / GNOME see the process as "sticky-notes", not "textbox_sandbox".
ln -sf "$ROOT/textbox_sandbox" "$ROOT/sticky-notes"

mkdir -p "$DESKTOP_DIR"
for size in 32 48 64 128 256; do
    install -Dm644 "$ICON_SRC" "$ICON_THEME_ROOT/${size}x${size}/apps/${ICON_THEME_NAME}.png"
done

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f "$ICON_THEME_ROOT" >/dev/null 2>&1 || true
fi

ICON_PATH="$ICON_THEME_ROOT/256x256/apps/${ICON_THEME_NAME}.png"

cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=Sticky Notes
GenericName=Notes
Comment=Local sticky notes desk and pop-out editor
Exec=$LAUNCHER
Path=$ROOT
Icon=$ICON_PATH
Terminal=false
StartupNotify=true
StartupWMClass=$APP_ID
Categories=Office;Utility;
Keywords=notes;sticky;memo;notepad;
EOF

chmod 644 "$DESKTOP_FILE"

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true
fi

if command -v desktop-file-validate >/dev/null 2>&1; then
    desktop-file-validate "$DESKTOP_FILE" || true
fi

echo "Installed launcher: $DESKTOP_FILE"
echo "Icon: $ICON_PATH (hicolor theme + gtk-update-icon-cache)"
echo "App id / WM class: $APP_ID"
echo ""
echo "Open your app menu and search for \"Sticky Notes\"."
echo "Quit any running copy, then launch again so the dock picks up the notepad icon."
