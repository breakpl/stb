#!/bin/bash
# install_mac.sh – Installs SprintToolBox to /Applications
# Clears the macOS quarantine flag that causes "damaged or incomplete" errors
# on machines other than the build machine (ad-hoc signed app).

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

# Remove quarantine flag so Gatekeeper doesn't block ad-hoc signed app
xattr -cr "$DEST"

echo ""
echo "✅ $APP_NAME installed to /Applications."
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
