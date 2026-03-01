#!/bin/bash
# package_mac.sh – builds SprintToolBox and produces a self-contained .dmg
# No external tools needed – uses only otool and install_name_tool (Xcode CLT)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
DIST_DIR="$SCRIPT_DIR/dist/mac"
APP_NAME="SprintToolBox"
APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
FRAMEWORKS="$APP_BUNDLE/Contents/Frameworks"
VERSION="1.0.0"

# ── 1. Build ──────────────────────────────────────────────────────────────────
echo "==> Building $APP_NAME (Release)..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
# Remove the binary so cmake is forced to relink (gives us clean Homebrew paths)
rm -f "$APP_BUNDLE/Contents/MacOS/$APP_NAME"
cmake .. -DCMAKE_BUILD_TYPE=Release -Wno-dev
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
mkdir -p "$FRAMEWORKS"

# bundle_dylib <src>
#   Copies <src> into $FRAMEWORKS, sets its install name, and recursively
#   bundles + rewrites all of its Homebrew (/usr/local) dependencies.
bundle_dylib() {
    local SRC="$1"
    local NAME
    NAME="$(basename "$SRC")"
    local DEST="$FRAMEWORKS/$NAME"

    # Already processed in this run – nothing to do
    [ -f "$DEST" ] && return 0

    echo "  Bundling: $NAME"

    # Copy the real file (resolve symlinks so we don't copy a dangling symlink)
    local REAL_SRC
    REAL_SRC="$(realpath "$SRC" 2>/dev/null || echo "$SRC")"
    cp "$REAL_SRC" "$DEST"
    chmod 755 "$DEST"

    # Set this library's own install name to the bundle-relative path
    install_name_tool -id "@executable_path/../Frameworks/$NAME" "$DEST"

    # Walk each dependency listed in the ORIGINAL file
    while IFS= read -r RAW_LINE; do
        local DEP
        DEP="$(echo "$RAW_LINE" | awk '{print $1}')"
        [ -z "$DEP" ] && continue

        local DEP_NAME
        DEP_NAME="$(basename "$DEP")"

        # Skip the self-reference entry (same basename = LC_ID_DYLIB line)
        [ "$DEP_NAME" = "$NAME" ] && continue

        # Only process Homebrew libraries (absolute /usr/local paths)
        if [[ "$DEP" == /usr/local/* ]]; then
            # Recurse with the ORIGINAL path (not realpath) so the bundled
            # filename matches the reference name (e.g. libzstd.1.dylib, not
            # libzstd.1.5.7.dylib which is the realpath-resolved version).
            bundle_dylib "$DEP"
            # Rewrite the reference inside the copy we are currently fixing
            install_name_tool -change "$DEP" "@executable_path/../Frameworks/$DEP_NAME" "$DEST"
        fi
    done < <(otool -L "$REAL_SRC" | tail -n +2)
}

# fix_binary <binary>
#   For each Homebrew dependency in the binary:
#     - if path is /usr/local/*  → bundle it and rewrite the LC_LOAD_DYLIB entry
#     - if path is @executable_path/../Frameworks/* → already correct; just
#       ensure the target dylib has been copied into Frameworks
fix_binary() {
    local BINARY="$1"
    echo "==> Fixing references in: $(basename "$BINARY")"
    while IFS= read -r RAW_LINE; do
        local DEP
        DEP="$(echo "$RAW_LINE" | awk '{print $1}')"
        [ -z "$DEP" ] && continue

        local DEP_NAME
        DEP_NAME="$(basename "$DEP")"

        if [[ "$DEP" == /usr/local/* ]]; then
            # Absolute Homebrew path – bundle + rewrite
            bundle_dylib "$DEP"
            install_name_tool -change "$DEP" "@executable_path/../Frameworks/$DEP_NAME" "$BINARY"

        elif [[ "$DEP" == "@executable_path/../Frameworks/"* ]]; then
            # Reference already correct – just ensure the file is present
            if [ ! -f "$FRAMEWORKS/$DEP_NAME" ]; then
                # Try the canonical Homebrew symlink dir first
                local FOUND="/usr/local/lib/$DEP_NAME"
                if [ ! -e "$FOUND" ]; then
                    FOUND="$(find /usr/local/opt -name "$DEP_NAME" \
                             -maxdepth 5 2>/dev/null | head -1)"
                fi
                if [ -e "$FOUND" ]; then
                    bundle_dylib "$FOUND"
                else
                    echo "  WARNING: cannot find source for $DEP_NAME" >&2
                fi
            fi
        fi
    done < <(otool -L "$BINARY" | tail -n +2)
}

BINARY="$APP_BUNDLE/Contents/MacOS/$APP_NAME"
fix_binary "$BINARY"

# ── 4. Verify all @executable_path refs are satisfied (binary + all dylibs) ───
echo "==> Verifying bundle..."
MISSING=0
check_refs() {
    local MACHO="$1"
    while IFS= read -r RAW_LINE; do
        local DEP
        DEP="$(echo "$RAW_LINE" | awk '{print $1}')"
        if [[ "$DEP" == "@executable_path/../Frameworks/"* ]]; then
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
for DYLIB in "$FRAMEWORKS"/*.dylib; do
    check_refs "$DYLIB"
done
if [ "$MISSING" -gt 0 ]; then
    echo "ERROR: $MISSING reference(s) unsatisfied – bundle is broken" >&2
    exit 1
fi
echo "  OK – all references satisfied."

# ── 5. Create DMG ─────────────────────────────────────────────────────────────
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
