#!/usr/bin/env bash
# Idempotent installer for hyprnotice.
# - Symlinks build/hyprnotice and build/hyprnotice-ctl into /usr/local/bin
# - Drops the systemd user unit
# - Disables the predecessor mako D-Bus service (if present)
set -euo pipefail

SRC="$(cd "$(dirname "$0")" && pwd)"
BUILD="$SRC/build"
USER_UNIT_DIR="$HOME/.config/systemd/user"

if [ ! -x "$BUILD/hyprnotice" ] || [ ! -x "$BUILD/hyprnotice-ctl" ]; then
    echo "Build first: cmake -B build && cmake --build build -j\$(nproc)" >&2
    exit 1
fi

echo "Source: $SRC"

# 1. system-level symlinks
sudo ln -sfn "$BUILD/hyprnotice"     /usr/local/bin/hyprnotice
sudo ln -sfn "$BUILD/hyprnotice-ctl" /usr/local/bin/hyprnotice-ctl
echo "  -> /usr/local/bin/hyprnotice"
echo "  -> /usr/local/bin/hyprnotice-ctl"

# 2. user systemd unit
mkdir -p "$USER_UNIT_DIR"
# Render the .service.in by replacing @CMAKE_INSTALL_PREFIX@ with /usr/local
sed 's|@CMAKE_INSTALL_PREFIX@|/usr/local|g' "$SRC/systemd/hyprnotice.service.in" \
    > "$USER_UNIT_DIR/hyprnotice.service"
echo "  -> $USER_UNIT_DIR/hyprnotice.service"

# 3. disable predecessor mako D-Bus activation (if it's still enabled)
MAKO_SVC="/usr/share/dbus-1/services/fr.emersion.mako.service"
if [ -f "$MAKO_SVC" ] && [ ! -f "$MAKO_SVC.disabled" ]; then
    echo "Disabling predecessor mako D-Bus activation"
    sudo mv "$MAKO_SVC" "$MAKO_SVC.disabled"
fi

# 4. enable + start
systemctl --user daemon-reload
systemctl --user enable --now hyprnotice.service
systemctl --user is-active hyprnotice.service && echo "hyprnotice running"

echo
echo "Installed. Tail logs: journalctl --user -u hyprnotice.service -f"
