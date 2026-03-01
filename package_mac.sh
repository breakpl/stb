#!/bin/bash
# package_mac.sh – builds SprintToolBox and produces a self-contained .dmg
# Requirements: brew install dylibbundler

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
DIST_DIR="$SCRIPT_DIR/dist/mac"
APP_NAME="SprintToolBox"
APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
VERSION="1.0.0"

# ── 1. Build ──────────────────────────────────────────────────────────────────
echo "==> Building $APP_NAME (Release)..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# ── 2. Copy SprintToolBox.ini-example into the bundle Resources ───────────────
RESOURCES="$APP_BUNDLE/Contents/Resources"
mkdir -p "$RESOURCES"
cp "$SCRIPT_DIR/SprintToolBox.ini-example" "$RESOURCES/SprintToolBox.ini-example"
# If a real SprintToolBox.ini exists alongside the script, ship it too
if [ -f "$SCRIPT_DIR/SprintToolBox.ini" ]; then
    cp "$SCRIPT_DIR/SprintToolBox.ini" "$RESOURCES/SprintToolBox.ini"
fi

# ── 3. Bundle dylibs ──────────────────────────────────────────────────────────
echo "==> Bundling dylibs..."
if ! command -v dylibbundler &>/dev/null; then
    echo "ERROR: dylibbundler not found."
    echo "       Install with: brew install dylibbundler"
    exit 1
fi

FRAMEWORKS="$APP_BUNDLE/Contents/Frameworks"
mkdir -p "$FRAMEWORKS"

dylibbundler -od -b \
    -x "$APP_BUNDLE/Contents/MacOS/$APP_NAME" \
    -d "$FRAMEWORKS" \
    -p @executable_path/../Frameworks

# ── 4. Create DMG ─────────────────────────────────────────────────────────────
echo "==> Creating DMG..."
mkdir -p "$DIST_DIR"
DMG="$DIST_DIR/${APP_NAME}-${VERSION}-mac.dmg"

hdiutil create \
    -volname "$APP_NAME" \
    -srcfolder "$APP_BUNDLE" \
    -ov -format UDZO \
    "$DMG"

echo ""
echo "==> Done: $DMG"
