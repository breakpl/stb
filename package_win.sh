#!/bin/bash
# package_win.sh – run this INSIDE MSYS2 UCRT64 shell
# Builds SprintToolBox and creates an NSIS installer (.exe).
# Usage: bash package_win.sh

export MSYSTEM="${MSYSTEM:-UCRT64}"
[ -f /etc/profile ] && source /etc/profile

set -e

ARCH="${ARCH:-x86_64}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
DIST_DIR="$SCRIPT_DIR/dist/win"
STAGE_DIR="$DIST_DIR/SprintToolBox"
APP_NAME="SprintToolBox"
EXE="$BUILD_DIR/$APP_NAME.exe"
CMAKE_VERSION=$(grep -m1 'project.*VERSION' "$SCRIPT_DIR/CMakeLists.txt" | sed -E 's/.*VERSION[[:space:]]+([0-9][0-9.]*).*/\1/')
VERSION=$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//')
VERSION="${VERSION:-$CMAKE_VERSION}"
DATE=$(date +%Y%m%d)

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
cp "$SCRIPT_DIR/SprintToolBox.ini" "$STAGE_DIR/"

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
echo "==> Building NSIS installer..."
mkdir -p "$DIST_DIR"
STAGE_WIN=$(cygpath -w "$STAGE_DIR")
DIST_WIN=$(cygpath -w "$DIST_DIR")

# Locate makensis – prefer the one in PATH, fall back to /ucrt64/bin
if command -v makensis &>/dev/null; then
    MAKENSIS="makensis"
elif [ -f "/ucrt64/bin/makensis" ]; then
    MAKENSIS="/ucrt64/bin/makensis"
else
    echo "ERROR: makensis not found" >&2
    exit 1
fi

"$MAKENSIS" \
    -DAPP_VERSION="$VERSION" \
    -DBUILD_DATE="$DATE" \
    -DARCH="$ARCH" \
    -DSTAGE_DIR="$STAGE_WIN" \
    -DOUT_DIR="$DIST_WIN" \
    "$SCRIPT_DIR/installer.nsi"

OUT_EXE="${APP_NAME}-${VERSION}-${DATE}-windows-${ARCH}.exe"
echo ""
echo "==> Done: $DIST_DIR/$OUT_EXE"
