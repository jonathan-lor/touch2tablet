# touch2tablet

Cross platform, user mode daemon and [OpenTabletDriver](https://github.com/OpenTabletDriver/OpenTabletDriver)-style GUI that turns a multitouch panel into an absolute-positioned tablet. Runs on Linux and [Windows](docs/windows.md).

![touch2tablet GUI screenshot](assets/gui_screenshot.png)

*GUI*

![playing osu with touch2tablet](assets/osu_demo.gif)

*Using touch2tablet to play osu! on a 165 × 100 mm GT911 touchscreen controller*

## Supported Hardware

See [Supported Panels](docs/supported-panels.md) for compatible devices and how to add one.

## Quick Start (Linux)

You need an x86_64 distribution from 2022 or later with udev, a systemd user session and libevdev.
Download the latest release and run its installer (needs sudo for udev rule):

```bash
curl -L https://github.com/jonathan-lor/touch2tablet/releases/latest/download/touch2tablet-linux-x86_64.tar.gz | tar xz
./touch2tablet/install.sh
```

Run the same commands again to update. 

Plug in a supported panel and the service should grab it and present "touch2tablet Absolute
Mouse" ("touch2tablet Pen Tablet" in [pen mode](#output-modes)). Open **touch2tablet** from the
application menu or run `touch2tablet` to edit the mapping.

touch2tablet daemon runs as a systemd user service:

```bash
systemctl --user status touch2tablet
journalctl --user -u touch2tablet -f            # follow the daemon log
systemctl --user reload touch2tablet            # re-read settings.json
systemctl --user disable --now touch2tablet     # back to plain touchscreen behavior
```

## Settings and Presets

Settings live in `~/.config/touch2tablet/settings.json` on Linux and `%LOCALAPPDATA%\touch2tablet\settings.json` on Windows.

## How It Works

Basically touch2tablet grabs the panel's multitouch input and turns the first finger into a single
absolute pointer, so applications see an absolute mouse (or a pen tablet in pen mode) instead of
a touchscreen. The daemon:

1. converts the panel's raw coordinates into millimeters on the glass,
2. applies the tablet area (position, size and rotation),
3. scales the result into the display area,
4. and moves the virtual mouse or pen there.

### Touch Behavior

Only the first finger controls the pointer! Additional contacts won't affect the stroke. The
finger acts as a held left button with no pressure, hover or right click.
When it lifts, the stroke ends and any fingers still on the panel are ignored
until all of them have lifted.

### Output Modes

`output_mode` (**Output mode** in the GUI) sets what applications see:

- `mouse` (default): an absolute mouse. **This is what you should use for osu!**
- `pen` (Linux only): a pen tablet for applications that read pen input.

In mouse mode the compositor spreads the device over the whole desktop, so with several monitors
set `display` to the whole desktop and place `display_area` on the monitor you want.

## Development

See [CONTRIBUTING.md](CONTRIBUTING.md). The control socket protocol is in
[docs/protocol.md](docs/protocol.md).

## Uninstall

Stop the service and remove the installed user files and system-wide udev rule:

```bash
systemctl --user disable --now touch2tablet.service
rm -f "$HOME/.config/systemd/user/touch2tablet.service"
rm -f "$HOME/.local/bin/touch2tabletd" "$HOME/.local/bin/touch2tablet"
rm -rf "$HOME/.local/lib/touch2tablet"
rm -f "$HOME/.local/share/applications/touch2tablet.desktop"
systemctl --user daemon-reload
sudo rm -f /etc/udev/rules.d/70-touch2tablet.rules
sudo udevadm control --reload
sudo udevadm trigger
```

This leaves settings and presets in `~/.config/touch2tablet/`. Delete that directory too if you don't want them anymore.

## License

[MIT](LICENSE)
