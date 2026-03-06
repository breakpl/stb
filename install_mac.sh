#!/bin/bash
# install_mac.sh – Installs SprintToolBox to /Applications
# Handles Gatekeeper: clears quarantine, re-signs locally, and registers
# the app with macOS so it won't be blocked.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_NAME="SprintToolBox"
APP_BUNDLE="$SCRIPT_DIR/$APP_NAME.app"
DEST="/Applications/$APP_NAME.app"

if [ ! -d "$APP_BUNDLE" ]; then
    echo "ERROR: $APP_NAME.app not found next to this script."
    exit 1
fi

echo "Installing $APP_NAME to /Applications..."
echo ""

# Kill existing instance if running
pkill -9 -x "$APP_NAME" 2>/dev/null || true
sleep 1

# Remove old installation
rm -rf "$DEST"

# Copy to /Applications
cp -R "$APP_BUNDLE" "$DEST"

# ── Gatekeeper bypass ─────────────────────────────────────────────────────────
# 1. Remove ALL extended attributes (quarantine, sandbox, etc.)
xattr -cr "$DEST" 2>/dev/null || true

# 2. Re-sign locally – ad-hoc signatures are per-machine but valid
echo "Signing app for this Mac..."
if [ -d "$DEST/Contents/Frameworks" ]; then
    for DYLIB in "$DEST/Contents/Frameworks"/*.dylib; do
        [ -f "$DYLIB" ] || continue
        codesign --force --sign - "$DYLIB" 2>/dev/null || true
    done
fi
codesign --force --sign - "$DEST/Contents/MacOS/$APP_NAME" 2>/dev/null || true
codesign --force --deep --sign - "$DEST" 2>/dev/null || true

# 3. Tell Gatekeeper to allow this specific app
sudo spctl --add --label "$APP_NAME" "$DEST" 2>/dev/null || true

echo ""
echo "Done! $APP_NAME installed to /Applications."
echo "The app runs as a menu bar icon (no Dock icon)."
echo ""

# If Gatekeeper still blocks the app:
echo "If the app doesn't open, go to:"
echo "  System Settings → Privacy & Security → scroll down → click 'Open Anyway'"
echo ""

# Offer to launch
read -p "Launch now? [Y/n] " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Nn]$ ]]; then
    open "$DEST"
    echo "$APP_NAME is running in your menu bar."
fi
