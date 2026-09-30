#!/bin/sh
# Install this touch2tablet tree (a release, or build/stage from install-user.sh) for the current
# user: the files go to ~/.local/lib/touch2tablet, plus a user service and launcher. Requires sudo
# for the udev rule.
set -eu
src=$(cd "$(dirname "$0")" && pwd)
dest="$HOME/.local/lib/touch2tablet"
if systemctl --user is-active --quiet touch2tablet.service; then
  systemctl --user stop touch2tablet.service
fi
"$src/bin/touch2tabletd" --print-udev-rules > "$src/70-touch2tablet.rules"   # from panels.json
if ! cmp -s "$src/70-touch2tablet.rules" /etc/udev/rules.d/70-touch2tablet.rules; then
  echo "installing udev rule (sudo, only when the supported panels change)"
  sudo install -m644 "$src/70-touch2tablet.rules" /etc/udev/rules.d/
  sudo udevadm control --reload && sudo udevadm trigger
fi
rm -rf "$dest"
mkdir -p "$dest" "$HOME/.local/bin" "$HOME/.local/share/applications" "$HOME/.config/touch2tablet"
cp -a "$src/." "$dest/"
ln -sf "$dest/bin/touch2tabletd" "$HOME/.local/bin/touch2tabletd"
ln -sf "$dest/bin/touch2tablet" "$HOME/.local/bin/touch2tablet"
sed "s|^Exec=touch2tablet|Exec=$HOME/.local/bin/touch2tablet|" "$src/share/applications/touch2tablet.desktop" \
  > "$HOME/.local/share/applications/touch2tablet.desktop"
update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
install -Dm644 "$src/lib/systemd/user/touch2tablet.service" "$HOME/.config/systemd/user/touch2tablet.service"
systemctl --user daemon-reload
systemctl --user enable --now touch2tablet.service
sleep 1
systemctl --user --no-pager --lines=5 status touch2tablet.service || true
