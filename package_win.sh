#!/bin/bash
# package_win.sh – run this INSIDE MSYS2 UCRT64 shell
# Builds SprintToolBox and collects all required DLLs into a zip package.
# Usage: from MSYS2 UCRT64 terminal:  bash /path/to/stb/package_win.sh

export MSYSTEM=UCRT64
[ -f /etc/profile ] && source /etc/profile

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
DIST_DIR="$SCRIPT_DIR/dist/win"
STAGE_DIR="$DIST_DIR/SprintToolBox"
APP_NAME="SprintToolBox"
EXE="$BUILD_DIR/$APP_NAME.exe"
VERSION="1.0.0"

# ── 1. Build ──────────────────────────────────────────────────────────────────
echo "==> Building $APP_NAME (Release)..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# ── 2. Stage directory ────────────────────────────────────────────────────────
echo "==> Staging files..."
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"

cp "$EXE" "$STAGE_DIR/"
cp "$SCRIPT_DIR/SprintToolBox.ini-example" "$STAGE_DIR/SprintToolBox.ini-example"
if [ -f "$SCRIPT_DIR/SprintToolBox.ini" ]; then
    cp "$SCRIPT_DIR/SprintToolBox.ini" "$STAGE_DIR/SprintToolBox.ini"
fi

# ── 3. Collect DLLs (exclude Windows system DLLs) ────────────────────────────
echo "==> Collecting DLLs..."
ldd "$EXE" \
    | grep '=> /' \
    | grep -iv '/c/windows' \
    | awk '{print $3}' \
    | while read -r dll; do
        echo "  + $(basename "$dll")"
        cp "$dll" "$STAGE_DIR/"
    done

# ── 4. Zip ────────────────────────────────────────────────────────────────────
echo "==> Zipping..."
cd "$DIST_DIR"
ZIP="${APP_NAME}-${VERSION}-win64.zip"
rm -f "$ZIP"
zip -r "$ZIP" "SprintToolBox/"

echo ""
echo "==> Done: $DIST_DIR/$ZIP"
echo "    Contents:"
unzip -l "$DIST_DIR/$ZIP" | tail -n +4 | head -n -2
