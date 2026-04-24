#!/bin/bash
# package_win.sh – run this INSIDE MSYS2 UCRT64 shell
# Builds SprintToolBox and creates a self-contained NSIS installer (.exe).
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

VERSION=$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || echo "1.0.0")
DATE=$(date +%Y%m%d)
OUT_EXE="${APP_NAME}-${VERSION}-${DATE}-windows-x86_64.exe"

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
cp "$SCRIPT_DIR/SprintToolBox.ini" "$STAGE_DIR/SprintToolBox.ini"

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

# ── 4. Build NSIS installer ───────────────────────────────────────────────────
echo "==> Building installer..."
mkdir -p "$DIST_DIR"
cd "$SCRIPT_DIR"

# Convert STAGE_DIR to Windows path for NSIS
STAGE_WIN=$(cygpath -w "$STAGE_DIR")

makensis \
    -DAPP_VERSION="$VERSION" \
    -DBUILD_DATE="$DATE" \
    -DSTAGE_DIR="$STAGE_WIN" \
    -DOUT_FILE="$DIST_DIR/$OUT_EXE" \
    installer.nsi

echo ""
echo "==> Done: $DIST_DIR/$OUT_EXE"
