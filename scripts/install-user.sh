#!/bin/sh
# Build and install touch2tabletd as a user service plus the touch2tablet GUI.
# Requires sudo for the udev rule.
set -eu
cd "$(dirname "$0")/.."
cmake -S . -B build -G Ninja >/dev/null
cmake --build build
build/daemon/touch2tabletd --print-udev-rules > build/70-touch2tablet.rules   # from panels.json
if ! cmp -s build/70-touch2tablet.rules /etc/udev/rules.d/70-touch2tablet.rules; then
  echo "installing udev rule (sudo, only when the supported panels change)"
  sudo install -m644 build/70-touch2tablet.rules /etc/udev/rules.d/
  sudo udevadm control --reload && sudo udevadm trigger
fi
install -Dm755 build/daemon/touch2tabletd "$HOME/.local/bin/touch2tabletd"
install -Dm755 build/gui/touch2tablet "$HOME/.local/bin/touch2tablet"
sed "s|^Exec=touch2tablet|Exec=$HOME/.local/bin/touch2tablet|" gui/touch2tablet.desktop \
  > "$HOME/.local/share/applications/touch2tablet.desktop" 2>/dev/null || true
update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
install -Dm644 daemon/systemd/user/touch2tablet.service "$HOME/.config/systemd/user/touch2tablet.service"
mkdir -p "$HOME/.config/touch2tablet"
systemctl --user daemon-reload
systemctl --user enable --now touch2tablet.service
systemctl --user restart touch2tablet.service
sleep 1
systemctl --user --no-pager --lines=5 status touch2tablet.service || true
