#!/bin/bash
# package_mac.sh – builds SprintToolBox (arm64) and produces a self-contained .dmg
# Requires: Xcode CLT (otool, install_name_tool, codesign, hdiutil)
# Usage: ./package_mac.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build-arm64"
DIST_DIR="$SCRIPT_DIR/dist/mac"
APP_NAME="SprintToolBox"
APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
FRAMEWORKS="$APP_BUNDLE/Contents/Frameworks"
BINARY="$APP_BUNDLE/Contents/MacOS/$APP_NAME"
HOMEBREW_PREFIX="/opt/homebrew"

VERSION=$(git describe --tags --abbrev=0 2>/dev/null || echo "v1.0.0")
VERSION="${VERSION#v}"
DATE=$(date +%Y%m%d)
DMG_NAME="${APP_NAME}-${VERSION}-${DATE}-macos-arm64.dmg"

# ── 1. Build ──────────────────────────────────────────────────────────────────
echo "==> Building $APP_NAME (Release, arm64)..."
echo "    Using Homebrew: $HOMEBREW_PREFIX"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
rm -f "$BINARY"
CMAKE_PREFIX_PATH="$HOMEBREW_PREFIX" \
    cmake .. -DCMAKE_BUILD_TYPE=Release \
             -DCMAKE_OSX_ARCHITECTURES=arm64 \
             -DCMAKE_PREFIX_PATH="$HOMEBREW_PREFIX" \
             -Wno-dev
cmake --build . --config Release

# ── 2. Copy SprintToolBox.ini into bundle Resources ───────────────────────────
local_RESOURCES="$APP_BUNDLE/Contents/Resources"
mkdir -p "$local_RESOURCES"
cp "$SCRIPT_DIR/SprintToolBox.ini" "$local_RESOURCES/SprintToolBox.ini"

# ── 3. Bundle dylibs ──────────────────────────────────────────────────────────
echo "==> Bundling dylibs..."
mkdir -p "$FRAMEWORKS"

bundle_dylib() {
    local SRC="$1"
    local NAME
    NAME="$(basename "$SRC")"
    local DEST="$FRAMEWORKS/$NAME"

    [ -f "$DEST" ] && return 0

    echo "  Bundling: $NAME"

    local REAL_SRC
    REAL_SRC="$(realpath "$SRC" 2>/dev/null || echo "$SRC")"
    cp "$REAL_SRC" "$DEST"
    chmod 755 "$DEST"

    install_name_tool -id "@executable_path/../Frameworks/$NAME" "$DEST"

    while IFS= read -r RAW_LINE; do
        local DEP
        DEP="$(echo "$RAW_LINE" | awk '{print $1}')"
        [ -z "$DEP" ] && continue

        local DEP_NAME
        DEP_NAME="$(basename "$DEP")"
        [ "$DEP_NAME" = "$NAME" ] && continue

        if [[ "$DEP" == /usr/local/* ]] || [[ "$DEP" == /opt/homebrew/* ]]; then
            bundle_dylib "$DEP"
            install_name_tool -change "$DEP" "@executable_path/../Frameworks/$DEP_NAME" "$DEST"

        elif [[ "$DEP" == @rpath/* ]] || [[ "$DEP" == @loader_path/* ]]; then
            local RESOLVED=""
            if [ -e "$HOMEBREW_PREFIX/lib/$DEP_NAME" ]; then
                RESOLVED="$HOMEBREW_PREFIX/lib/$DEP_NAME"
            else
                RESOLVED="$(find "$HOMEBREW_PREFIX/opt" -name "$DEP_NAME" \
                            -maxdepth 5 2>/dev/null | head -1)"
            fi
            if [ -z "$RESOLVED" ]; then
                local SRC_DIR
                SRC_DIR="$(dirname "$REAL_SRC")"
                [ -e "$SRC_DIR/$DEP_NAME" ] && RESOLVED="$SRC_DIR/$DEP_NAME"
            fi
            if [ -n "$RESOLVED" ] && [ -e "$RESOLVED" ]; then
                bundle_dylib "$RESOLVED"
                install_name_tool -change "$DEP" "@executable_path/../Frameworks/$DEP_NAME" "$DEST"
            fi
        fi
    done < <(otool -L "$REAL_SRC" | tail -n +2)
}

fix_binary() {
    local BIN="$1"
    echo "==> Fixing references in: $(basename "$BIN")"
    while IFS= read -r RAW_LINE; do
        local DEP
        DEP="$(echo "$RAW_LINE" | awk '{print $1}')"
        [ -z "$DEP" ] && continue

        local DEP_NAME
        DEP_NAME="$(basename "$DEP")"

        if [[ "$DEP" == /usr/local/* ]] || [[ "$DEP" == /opt/homebrew/* ]]; then
            bundle_dylib "$DEP"
            install_name_tool -change "$DEP" "@executable_path/../Frameworks/$DEP_NAME" "$BIN"

        elif [[ "$DEP" == "@executable_path/../Frameworks/"* ]] || [[ "$DEP" == @rpath/* ]] || [[ "$DEP" == @loader_path/* ]]; then
            if [ ! -f "$FRAMEWORKS/$DEP_NAME" ]; then
                local FOUND="$HOMEBREW_PREFIX/lib/$DEP_NAME"
                [ ! -e "$FOUND" ] && FOUND="$(find "$HOMEBREW_PREFIX/opt" -name "$DEP_NAME" \
                                              -maxdepth 5 2>/dev/null | head -1)"
                [ -e "$FOUND" ] && bundle_dylib "$FOUND" || echo "  WARNING: cannot find source for $DEP_NAME" >&2
            fi
            if [[ "$DEP" != "@executable_path/../Frameworks/"* ]]; then
                install_name_tool -change "$DEP" "@executable_path/../Frameworks/$DEP_NAME" "$BIN"
            fi
        fi
    done < <(otool -L "$BIN" | tail -n +2)
}

fix_binary "$BINARY"

echo "==> Fixing references in bundled dylibs..."
for DYLIB in "$FRAMEWORKS"/*.dylib; do
    [ -f "$DYLIB" ] || continue
    local_DYLIB_NAME="$(basename "$DYLIB")"
    while IFS= read -r RAW_LINE; do
        local DEP
        DEP="$(echo "$RAW_LINE" | awk '{print $1}')"
        [ -z "$DEP" ] && continue
        local DEP_NAME
        DEP_NAME="$(basename "$DEP")"
        [ "$DEP_NAME" = "$local_DYLIB_NAME" ] && continue

        if [[ "$DEP" == @rpath/* ]] || [[ "$DEP" == @loader_path/* ]]; then
            if [ ! -f "$FRAMEWORKS/$DEP_NAME" ]; then
                local FOUND="$HOMEBREW_PREFIX/lib/$DEP_NAME"
                [ ! -e "$FOUND" ] && FOUND="$(find "$HOMEBREW_PREFIX/opt" -name "$DEP_NAME" \
                                              -maxdepth 5 2>/dev/null | head -1)"
                [ -e "$FOUND" ] && bundle_dylib "$FOUND"
            fi
            install_name_tool -change "$DEP" "@executable_path/../Frameworks/$DEP_NAME" "$DYLIB"
        fi
    done < <(otool -L "$DYLIB" | tail -n +2)
done

# ── 4. Verify bundle ──────────────────────────────────────────────────────────
echo "==> Verifying bundle..."
MISSING=0
check_refs() {
    local MACHO="$1"
    while IFS= read -r RAW_LINE; do
        local DEP
        DEP="$(echo "$RAW_LINE" | awk '{print $1}')"
        if [[ "$DEP" == "@executable_path/../Frameworks/"* ]] || [[ "$DEP" == @rpath/* ]] || [[ "$DEP" == @loader_path/* ]]; then
            local NEED
            NEED="$(basename "$DEP")"
            if [ ! -f "$FRAMEWORKS/$NEED" ]; then
                echo "  MISSING ($MACHO → $NEED)" >&2
                MISSING=$((MISSING + 1))
            fi
        fi
    done < <(otool -L "$MACHO" | tail -n +2)
}
check_refs "$BINARY"
for DYLIB in "$FRAMEWORKS"/*.dylib; do check_refs "$DYLIB"; done
if [ "$MISSING" -gt 0 ]; then
    echo "ERROR: $MISSING reference(s) unsatisfied – bundle is broken" >&2
    exit 1
fi
echo "  OK – all references satisfied."

# ── 5. Code sign ─────────────────────────────────────────────────────────────
echo "==> Code signing..."
for DYLIB in "$FRAMEWORKS"/*.dylib; do
    [ -f "$DYLIB" ] || continue
    codesign --force --sign - --timestamp=none "$DYLIB"
done
codesign --force --sign - --timestamp=none \
         --entitlements "$SCRIPT_DIR/entitlements.plist" "$BINARY"
codesign --force --deep --sign - --timestamp=none \
         --entitlements "$SCRIPT_DIR/entitlements.plist" "$APP_BUNDLE"
echo "  OK – app signed."

# ── 6. Create DMG ─────────────────────────────────────────────────────────────
echo "==> Creating DMG..."
mkdir -p "$DIST_DIR"
DMG="$DIST_DIR/$DMG_NAME"

STAGE_DIR="$BUILD_DIR/dmg-stage"
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"
cp -R "$APP_BUNDLE" "$STAGE_DIR/"
cp "$SCRIPT_DIR/install_mac.sh" "$STAGE_DIR/install.command"
chmod +x "$STAGE_DIR/install.command"

hdiutil create \
    -volname "$APP_NAME" \
    -srcfolder "$STAGE_DIR" \
    -ov -format UDZO \
    "$DMG"

rm -rf "$STAGE_DIR"

echo ""
echo "==> Done: $DMG"
