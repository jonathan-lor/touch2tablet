# Contributing

## Building

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev libevdev-dev
cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build   # build + tests
```

The daemon can run without hardware for GUI and protocol work:

```bash
./build/daemon/touch2tabletd --no-device --socket /tmp/t2t.sock --settings /tmp/t2t.json &
./build/gui/touch2tablet --socket /tmp/t2t.sock
```

The control socket protocol is documented in [docs/protocol.md](docs/protocol.md).

## Layout

| Path | What |
|---|---|
| `core/` | platform-neutral library (QtCore only): `Settings` (+JSON), `Mapper`, `GestureMachine`, `SettingsStore` |
| `panels.json` | the supported panels, built into the daemon (`daemon/src/Panels.*`), which renders the udev rule and the table in `docs/supported-panels.md` from it |
| `daemon/` | `Daemon` orchestration, `ControlServer`, `ITouchSource`/`IPenSink`, `backend/linux/` (evdev, uinput), user unit |
| `gui/` | Qt Widgets GUI: `ControlClient`, `AreaCanvas`, `MainWindow`, desktop file |
| `tests/` | Qt Test suites, shared `fakes.h` and `pen_signature.h` |
| `scripts/install-user.sh` | build + udev rule (seat ACL for the listed panels and uinput) + install everything user-level |
| `scripts/update-docs.sh` | rewrite the table in `docs/supported-panels.md` after editing `panels.json` |
