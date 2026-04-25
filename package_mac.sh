#!/bin/bash
# package_mac.sh – builds SprintToolBox (arm64) and produces a signed .pkg installer
# The installer offers an osascript dialog to enable autostart at login.
# Requires: Xcode CLT (otool, install_name_tool, codesign, pkgbuild, productbuild)
# Usage: ./package_mac.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build-arm64"
DIST_DIR="$SCRIPT_DIR/dist/mac"
APP_NAME="SprintToolBox"
BUNDLE_ID="com.sprinttoolbox"
APP_BUNDLE="$BUILD_DIR/$APP_NAME.app"
FRAMEWORKS="$APP_BUNDLE/Contents/Frameworks"
BINARY="$APP_BUNDLE/Contents/MacOS/$APP_NAME"
HOMEBREW_PREFIX="/opt/homebrew"

VERSION=$(git describe --tags --abbrev=0 2>/dev/null || echo "v1.0.0")
VERSION="${VERSION#v}"
DATE=$(date +%Y%m%d)
PKG_NAME="${APP_NAME}-${VERSION}-${DATE}-macos-arm64.pkg"

# ── 1. Build ──────────────────────────────────────────────────────────────────
echo "==> Building $APP_NAME (Release, arm64)..."
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
RESOURCES="$APP_BUNDLE/Contents/Resources"
mkdir -p "$RESOURCES"
cp "$SCRIPT_DIR/SprintToolBox.ini" "$RESOURCES/SprintToolBox.ini"

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
        local DEP DEP_NAME
        DEP="$(echo "$RAW_LINE" | awk '{print $1}')"
        [ -z "$DEP" ] && continue
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
                RESOLVED="$(find "$HOMEBREW_PREFIX/opt" -name "$DEP_NAME" -maxdepth 5 2>/dev/null | head -1)"
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
        local DEP DEP_NAME
        DEP="$(echo "$RAW_LINE" | awk '{print $1}')"
        [ -z "$DEP" ] && continue
        DEP_NAME="$(basename "$DEP")"
        if [[ "$DEP" == /usr/local/* ]] || [[ "$DEP" == /opt/homebrew/* ]]; then
            bundle_dylib "$DEP"
            install_name_tool -change "$DEP" "@executable_path/../Frameworks/$DEP_NAME" "$BIN"
        elif [[ "$DEP" == "@executable_path/../Frameworks/"* ]] || [[ "$DEP" == @rpath/* ]] || [[ "$DEP" == @loader_path/* ]]; then
            if [ ! -f "$FRAMEWORKS/$DEP_NAME" ]; then
                local FOUND="$HOMEBREW_PREFIX/lib/$DEP_NAME"
                [ ! -e "$FOUND" ] && FOUND="$(find "$HOMEBREW_PREFIX/opt" -name "$DEP_NAME" -maxdepth 5 2>/dev/null | head -1)"
                if [ -e "$FOUND" ]; then bundle_dylib "$FOUND"; else echo "  WARNING: cannot find source for $DEP_NAME" >&2; fi
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
    DYLIB_NAME="$(basename "$DYLIB")"
    while IFS= read -r RAW_LINE; do
        DEP="$(echo "$RAW_LINE" | awk '{print $1}')"
        [ -z "$DEP" ] && continue
        DEP_NAME="$(basename "$DEP")"
        [ "$DEP_NAME" = "$DYLIB_NAME" ] && continue
        if [[ "$DEP" == @rpath/* ]] || [[ "$DEP" == @loader_path/* ]]; then
            if [ ! -f "$FRAMEWORKS/$DEP_NAME" ]; then
                FOUND="$HOMEBREW_PREFIX/lib/$DEP_NAME"
                [ ! -e "$FOUND" ] && FOUND="$(find "$HOMEBREW_PREFIX/opt" -name "$DEP_NAME" -maxdepth 5 2>/dev/null | head -1)"
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
        local DEP NEED
        DEP="$(echo "$RAW_LINE" | awk '{print $1}')"
        if [[ "$DEP" == "@executable_path/../Frameworks/"* ]] || [[ "$DEP" == @rpath/* ]] || [[ "$DEP" == @loader_path/* ]]; then
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

# ── 6. Create .pkg installer ──────────────────────────────────────────────────
echo "==> Building .pkg installer..."
PKG_WORK="$BUILD_DIR/pkg-work"
rm -rf "$PKG_WORK"
mkdir -p "$PKG_WORK"

# Payload: app → /Applications
PKG_ROOT="$PKG_WORK/root"
mkdir -p "$PKG_ROOT/Applications"
cp -R "$APP_BUNDLE" "$PKG_ROOT/Applications/"

# Postinstall script: dialog → LaunchAgent
SCRIPTS_DIR="$PKG_WORK/scripts"
mkdir -p "$SCRIPTS_DIR"

# Write the postinstall script without heredoc nesting.
# It asks the user (via osascript) whether to enable autostart and,
# if yes, installs a LaunchAgent plist into their ~/Library/LaunchAgents/.
PLIST_CONTENT='<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.sprinttoolbox</string>
    <key>ProgramArguments</key>
    <array>
        <string>/Applications/SprintToolBox.app/Contents/MacOS/SprintToolBox</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <false/>
    <key>ProcessType</key>
    <string>Interactive</string>
</dict>
</plist>'

# Embed the plist as a base64 blob so the postinstall script has no heredoc dependency.
PLIST_B64=$(printf '%s' "$PLIST_CONTENT" | base64)

cat > "$SCRIPTS_DIR/postinstall" << POSTINSTALL_EOF
#!/bin/bash
set -e

# Find the logged-in console user (the one who launched the installer).
CONSOLE_USER=\$(stat -f "%Su" /dev/console 2>/dev/null || true)
if [ -z "\$CONSOLE_USER" ] || [ "\$CONSOLE_USER" = "root" ]; then exit 0; fi

USER_UID=\$(id -u "\$CONSOLE_USER" 2>/dev/null || true)
USER_HOME=\$(dscl . -read "/Users/\$CONSOLE_USER" NFSHomeDirectory 2>/dev/null | awk '{print \$2}')
if [ -z "\$USER_HOME" ]; then exit 0; fi

# Show a native dialog asking about autostart.
RESPONSE=\$(launchctl asuser "\$USER_UID" sudo -u "\$CONSOLE_USER" \
    osascript -e 'display dialog "Start SprintToolBox automatically when you log in?" buttons {"No", "Yes"} default button "Yes" with icon note with title "SprintToolBox"' \
    2>/dev/null || echo "button returned:No")

if echo "\$RESPONSE" | grep -q "Yes"; then
    LAUNCH_AGENTS="\$USER_HOME/Library/LaunchAgents"
    PLIST_PATH="\$LAUNCH_AGENTS/com.sprinttoolbox.plist"
    mkdir -p "\$LAUNCH_AGENTS"
    printf '%s' "${PLIST_B64}" | base64 --decode > "\$PLIST_PATH"
    chown "\$CONSOLE_USER" "\$PLIST_PATH"
    launchctl asuser "\$USER_UID" sudo -u "\$CONSOLE_USER" \
        launchctl load "\$PLIST_PATH" 2>/dev/null || true
    echo "Autostart enabled."
fi
POSTINSTALL_EOF

chmod 755 "$SCRIPTS_DIR/postinstall"

# Build the component package
pkgbuild \
    --root "$PKG_ROOT" \
    --identifier "$BUNDLE_ID" \
    --version "$VERSION" \
    --install-location "/" \
    --scripts "$SCRIPTS_DIR" \
    "$PKG_WORK/component.pkg"

# Wrap in a distribution package (adds welcome/licence pages and proper title)
cat > "$PKG_WORK/distribution.xml" << DIST_EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>SprintToolBox ${VERSION}</title>
    <options hostArchitectures="arm64" customize="never" require-scripts="false"/>
    <choices-outline>
        <line choice="default"/>
    </choices-outline>
    <choice id="default" title="SprintToolBox">
        <pkg-ref id="${BUNDLE_ID}"/>
    </choice>
    <pkg-ref id="${BUNDLE_ID}" version="${VERSION}" onConclusion="none">component.pkg</pkg-ref>
</installer-gui-script>
DIST_EOF

mkdir -p "$DIST_DIR"
productbuild \
    --distribution "$PKG_WORK/distribution.xml" \
    --package-path "$PKG_WORK" \
    "$DIST_DIR/$PKG_NAME"

echo ""
echo "==> Done: $DIST_DIR/$PKG_NAME"
