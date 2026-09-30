# Supported Panels

Any slotted multitouch panel (`ABS_MT_SLOT` + `ABS_MT_POSITION_X/Y`, e.g. driven by `hid-multitouch`
or `goodix`) should work, but the daemon only grabs the panels listed here. The table is generated
from [`panels.json`](../panels.json); edit that instead.

<!-- panels:begin -->
| Panel | Bus | ID | evdev name | Glass | Notes |
|---|---|---|---|---|---|
| SingWon 7" USB touch panel | usb | `222a:0001` | `UsbHID SingWon-CTP-V1.18B` | 165 x 100 mm | sold as "7 Inch 165mmx100mm GT911 6Pin IIC Universal USB Drive Capacitive Digitizer Touch Screen Panel Glass" on Amazon; `hid-multitouch` out of the box |
<!-- panels:end -->

## Adding a Panel

A panel is matched on its evdev bus:vendor:product ID and its evdev name.

1. Plug the panel in and run `build/daemon/touch2tabletd --identify` (after a normal build, see
   [CONTRIBUTING.md](../CONTRIBUTING.md)). It prints a ready-made entry for every multitouch
   device; with `sudo` it can also read the size from devices that report one.
2. Paste the entry into `panels.json`. Keep `bus`, `vendor`, `product` and `evdev_name` as
   printed, and fill in:
   - `name`: what the GUI and logs call it, ideally what a buyer would recognize (brand, size,
     connection). Don't name a controller chip the device doesn't report.
   - `width_mm` / `height_mm`: the active glass area. Keep the size `--identify` printed if there
     is one, else use the datasheet or listing, else measure. The aspect ratio matters most.
   - `notes` (optional): anything else, e.g. how it is sold or which driver binds it.
3. Run `scripts/update-docs.sh` to rewrite the table above, then `scripts/install-user.sh` to
   rebuild, reinstall the udev rule and restart the daemon.
4. Commit `panels.json` and `docs/supported-panels.md`.

`ctest` rejects a malformed entry, a duplicate panel, a misspelled key or a stale table.
