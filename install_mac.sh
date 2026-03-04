#!/bin/bash
# install_mac.sh – Installs SprintToolBox to /Applications
# Clears quarantine and re-signs the app locally so Gatekeeper accepts it.

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

# Kill existing instance if running
pkill -9 -x "$APP_NAME" 2>/dev/null || true
sleep 1

# Copy to /Applications
cp -R "$APP_BUNDLE" /Applications/

# Remove quarantine extended attributes
xattr -cr "$DEST"

# Re-sign the app locally so the ad-hoc signature is valid on THIS machine.
# Sign frameworks first, then the main binary, then the whole bundle.
echo "Signing app..."
if [ -d "$DEST/Contents/Frameworks" ]; then
    for DYLIB in "$DEST/Contents/Frameworks"/*.dylib; do
        [ -f "$DYLIB" ] || continue
        codesign --force --sign - "$DYLIB" 2>/dev/null
    done
fi
codesign --force --sign - "$DEST/Contents/MacOS/$APP_NAME" 2>/dev/null
codesign --force --deep --sign - "$DEST" 2>/dev/null

echo ""
echo "$APP_NAME installed to /Applications."
echo ""
echo "You can now open it from Spotlight or /Applications."
echo "The app runs as a menu bar icon (no Dock icon)."

# Offer to launch
read -p "Launch now? [Y/n] " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Nn]$ ]]; then
    open "$DEST"
    echo "$APP_NAME is running in your menu bar."
fi
