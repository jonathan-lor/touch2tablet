# touch2tablet

User mode Linux daemon + [OpenTabletDriver](https://github.com/OpenTabletDriver/OpenTabletDriver)-style GUI for using a multitouch panel as an absolutely positioned tablet.

Your finger is the pen.

![touch2tablet GUI screenshot](assets/gui_screenshot.png)

## Supported Hardware

[docs/supported-panels.md](docs/supported-panels.md)

## Quick Start

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev libevdev-dev
scripts/install-user.sh    # udev rule, build, daemon + GUI + user unit into ~/.local
```

Plug in a supported panel and then the user service will grab it and present "touch2tablet Pen Tablet".
Edit mappings in the touch2tablet GUI app.

The daemon runs as a systemd user service:

```bash
systemctl --user status touch2tablet
journalctl --user -u touch2tablet -f            # follow the daemon log
systemctl --user reload touch2tablet            # re-read settings.json
systemctl --user disable --now touch2tablet     # back to plain touchscreen behavior
```

## Settings and Presets

Settings are in `~/.config/touch2tablet/settings.json` (`--settings`), and get written by Save in the
GUI. Missing or invalid fields keep the current value. The glass size comes
from the panel's entry in `panels.json`.

```
display       {width, height, width_mm, height_mm}   monitor the virtual tablet spans
display_area  {width, height, x, y}                  px, center-based
tablet        {width, height}                        glass active area, mm; the daemon sets it from the panel
tablet_area   {width, height, x, y, rotation}        mm, center-based, degrees clockwise
clip, limit, lock_aspect                              bools
```

Presets are settings files in `~/.config/touch2tablet/presets/` (next to the settings file). File >
Save as preset… to write the current settings there, and list them with File > Presets.
A hand-written preset may be partial, e.g. only `tablet_area`.

## How it works

The virtual tablet's coordinate space is the monitor in pixels (ABS range = configured display
size), so the touch2tablet daemon owns the whole mapping like OpenTabletDriver does:

```
raw touch -> mm on the glass (raw range read from the device, scaled to tablet.width/height)
          -> rotate about the tablet-area center by -rotation
          -> normalize to tablet_area (width/height/x/y in mm, center-based)
             clip=true  : clamp to the area edge        limit=true : ignore touches starting outside
          -> display_area (width/height/x/y in px, center-based) -> clamp to the display -> uinput
```

### Gestures

Tip-only and the first finger is what gets recognized as the pen. Extra contacts change nothing.
If that finger lifts while others remain, the stroke ends and the leftovers are ignored until everything lifts.
Tip and proximity are released when the last finger lifts.

## Development

[CONTRIBUTING.md](CONTRIBUTING.md).

The daemon's control socket protocol: [docs/protocol.md](docs/protocol.md).

## License

MIT
