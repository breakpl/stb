#!/bin/bash
# package_linux.sh – builds SprintToolBox and produces a .deb package (Ubuntu/Debian)
# Requirements: build-essential cmake libwxgtk3.2-dev libcurl4-openssl-dev dpkg-dev

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
DIST_DIR="$SCRIPT_DIR/dist/linux"
APP_NAME="SprintToolBox"
APP_NAME_LOWER="sprinttoolbox"
EXE="$BUILD_DIR/$APP_NAME"
# Auto-detect version from git tag or use default
VERSION=$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || echo "1.0.4")
ARCH="$(dpkg --print-architecture 2>/dev/null || echo amd64)"

# ── 1. Build ──────────────────────────────────────────────────────────────────
echo "==> Building $APP_NAME (Release)..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# ── 2. Prepare .deb tree ──────────────────────────────────────────────────────
echo "==> Preparing .deb structure..."
DEB_ROOT="$DIST_DIR/deb"
rm -rf "$DEB_ROOT"

DEB_BIN="$DEB_ROOT/usr/bin"
DEB_SHARE_APPS="$DEB_ROOT/usr/share/applications"
DEB_SHARE_DOC="$DEB_ROOT/usr/share/doc/$APP_NAME_LOWER"
DEB_CTRL="$DEB_ROOT/DEBIAN"

mkdir -p "$DEB_BIN" "$DEB_SHARE_APPS" "$DEB_SHARE_DOC" "$DEB_CTRL"

# Binary
cp "$EXE" "$DEB_BIN/$APP_NAME_LOWER"
chmod 755 "$DEB_BIN/$APP_NAME_LOWER"

# Config → /usr/share/doc/
cp "$SCRIPT_DIR/SprintToolBox.ini" "$DEB_SHARE_DOC/SprintToolBox.ini"

# .desktop file (autostart-capable)
cat > "$DEB_SHARE_APPS/$APP_NAME_LOWER.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=SprintToolBox
Comment=Sprint tracking tray utility
Exec=/usr/bin/$APP_NAME_LOWER
Icon=utilities-system-monitor
Categories=Utility;
StartupNotify=false
X-GNOME-Autostart-enabled=true
EOF

# copyright stub
cat > "$DEB_SHARE_DOC/copyright" <<EOF
SprintToolBox $VERSION
See source repository for full license information.
EOF

# ── 3. DEBIAN/control ─────────────────────────────────────────────────────────
INSTALLED_KB=$(du -sk "$DEB_ROOT/usr" | cut -f1)

# Use flexible dependencies with alternatives (3.2 or 3.0)
cat > "$DEB_CTRL/control" <<EOF
Package: $APP_NAME_LOWER
Version: $VERSION
Section: utils
Priority: optional
Architecture: $ARCH
Installed-Size: $INSTALLED_KB
Depends: libwxgtk3.2-1 | libwxgtk3.2-0v5 | libwxgtk3.0-gtk3-0v5, libcurl4
Maintainer: SprintToolBox
Description: Sprint tracking tray utility
 A tray icon application for tracking JIRA sprint progress,
 with Hex/Dec and time format converters.
EOF

# ── 4. DEBIAN/postinst / postrm ───────────────────────────────────────────────
cat > "$DEB_CTRL/postinst" <<'EOF'
#!/bin/sh
set -e

# Update desktop database
update-desktop-database /usr/share/applications 2>/dev/null || true

# Prompt for autostart setup (runs as the installing user)
if [ -n "$SUDO_USER" ]; then
    USER_HOME=$(getent passwd "$SUDO_USER" | cut -d: -f6)
else
    USER_HOME="$HOME"
fi

AUTOSTART_DIR="$USER_HOME/.config/autostart"

# Ask user if they want autostart
cat <<ENDMSG

────────────────────────────────────────────────────────
SprintToolBox installed successfully!

To enable autostart at login, run:
  mkdir -p ~/.config/autostart
  cp /usr/share/applications/sprinttoolbox.desktop ~/.config/autostart/

Or manually enable in your desktop environment's startup settings.
────────────────────────────────────────────────────────
ENDMSG

# If we can write to user's autostart directory, offer to set it up
if [ -w "$USER_HOME" ] && [ "$USER_HOME" != "/root" ]; then
    printf "Enable autostart at login? [y/N] "
    read -r REPLY < /dev/tty || REPLY="n"
    case "$REPLY" in
        [Yy]*)
            mkdir -p "$AUTOSTART_DIR"
            cp /usr/share/applications/sprinttoolbox.desktop "$AUTOSTART_DIR/"
            chown -R "$SUDO_USER:$SUDO_USER" "$AUTOSTART_DIR" 2>/dev/null || true
            echo "✓ Autostart enabled"
            ;;
    esac
fi
EOF
chmod 755 "$DEB_CTRL/postinst"

cat > "$DEB_CTRL/postrm" <<'EOF'
#!/bin/sh
set -e
update-desktop-database /usr/share/applications 2>/dev/null || true

# Clean up autostart file if it exists
if [ "$1" = "purge" ] || [ "$1" = "remove" ]; then
    if [ -n "$SUDO_USER" ]; then
        USER_HOME=$(getent passwd "$SUDO_USER" | cut -d: -f6)
    else
        USER_HOME="$HOME"
    fi
    rm -f "$USER_HOME/.config/autostart/sprinttoolbox.desktop" 2>/dev/null || true
fi
EOF
chmod 755 "$DEB_CTRL/postrm"

# ── 5. Build .deb ─────────────────────────────────────────────────────────────
echo "==> Building .deb..."
mkdir -p "$DIST_DIR"
DEB_FILE="$DIST_DIR/${APP_NAME_LOWER}_${VERSION}_${ARCH}.deb"
dpkg-deb --build "$DEB_ROOT" "$DEB_FILE"

echo ""
echo "==> Done: $DEB_FILE"
echo "    Install with: sudo dpkg -i $DEB_FILE"
echo "    Fix missing deps: sudo apt-get install -f"
