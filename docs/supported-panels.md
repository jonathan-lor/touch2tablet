# Supported panels

Any slotted multitouch panel (`ABS_MT_SLOT` + `ABS_MT_POSITION_X/Y`, e.g. `hid-multitouch` or
`goodix`) should work, but the daemon will only grabs the ones explicitly listed here.

DO NOT EDIT THIS TABLE MANUALLY! It's generated from [`panels.json`](../panels.json).

<!-- panels:begin -->
| Panel | Bus | ID | evdev name | Glass | Notes |
|---|---|---|---|---|---|
| SingWon 7" USB touch panel | usb | `222a:0001` | `UsbHID SingWon-CTP-V1.18B` | 165 x 100 mm | sold as "7 Inch 165mmx100mm GT911 6Pin IIC Universal USB Drive Capacitive Digitizer Touch Screen Panel Glass" on Amazon; `hid-multitouch` out of the box |
<!-- panels:end -->

## Adding a panel

The list is [`panels.json`](../panels.json). It gets built into the daemon, and the udev rule and
the table above are rendered from it. A panel is matched on its evdev bus:vendor:product ID and its evdev name.

1. Plug the panel in and run `build/daemon/touch2tabletd --identify` (after a normal build, see
   [CONTRIBUTING.md](../CONTRIBUTING.md)). It prints a ready-made entry for every multitouch device; with `sudo` it can
   also read the size from devices that report one.
2. Paste the entry into `panels.json`. `bus`, `vendor`, `product` and `evdev_name` identify the
   panel and come from the device: leave them as printed. You supply:
   - `name`: what the GUI and logs call it. Describe what a buyer would recognize (brand, size,
     connection); do not name a controller chip the device does not report.
   - `width_mm` / `height_mm`: the active glass area. Keep the size `--identify` printed if there
     is one, else use the datasheet or listing, else measure. The aspect ratio is what matters.
   - `notes` (optional): anything else, e.g. how it is sold or which driver binds it.
3. Run `scripts/update-docs.sh` (rewrites the table above), then `scripts/install-user.sh`
   (rebuilds, reinstalls the udev rule and restarts the daemon).
4. Commit `panels.json` and `docs/supported-panels.md`.

`ctest` will reject a malformed entry, a duplicate panel, a misspelled key, or a stale table.
