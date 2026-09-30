# Contributing

## Linux

You need CMake 3.22+, Ninja, a C++20 compiler, Qt 6.5+ (Core, Network, Widgets and Test),
`pkg-config` and libevdev. On Debian-based distributions (check that `qt6-base-dev` is 6.5+):

```bash
sudo apt install build-essential cmake ninja-build pkg-config qt6-base-dev libevdev-dev
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

The daemon runs without hardware for GUI and protocol work:

```bash
./build/daemon/touch2tabletd --no-device --socket /tmp/t2t.sock --settings /tmp/t2t.json &
./build/gui/touch2tablet --socket /tmp/t2t.sock
```

The control socket protocol is in [docs/protocol.md](docs/protocol.md).

## Windows

Install Visual Studio 2022 Build Tools with **Desktop development with C++** (it includes the
Windows SDK, CMake and Ninja) and Qt 6.5+ for MSVC. Known-good versions: MSVC 19.44, CMake
3.31.6, Ninja 1.12.1 and Qt 6.8.3. With Python installed,
[aqtinstall](https://aqtinstall.readthedocs.io/en/stable/getting_started.html) can fetch Qt:

```bat
python -m pip install aqtinstall
python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --outputdir "%LOCALAPPDATA%\Qt" --archives qtbase qttools
```

In an **x64 Native Tools Command Prompt for VS 2022**, from the repository:

```bat
set "QTDIR=%LOCALAPPDATA%\Qt\6.8.3\msvc2022_64"
set "PATH=%QTDIR%\bin;%PATH%"
cmake -S . -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="%QTDIR%"
cmake --build build/windows --parallel 4
ctest --test-dir build/windows --output-on-failure
```

Development builds are unsigned, so they can't capture the panel (that needs the signed install
in the [Windows guide](docs/windows.md)). For GUI and protocol work, run each of these in its own
prompt with Qt's `bin` on PATH:

```bat
build\windows\touch2tabletd.exe --no-device --socket touch2tablet-dev --settings build/windows/settings.json
build\windows\touch2tablet.exe --socket touch2tablet-dev
```

## Layout

| Path | What |
|---|---|
| `core/` | platform-neutral library (QtCore only): `Settings`, `Mapper`, `GestureMachine`, `SettingsStore` |
| `daemon/` | `Daemon`, `ControlServer`, the `ITouchSource`/`IPenSink` backends in `backend/linux/` and `backend/windows/`, the systemd user unit |
| `gui/` | Qt Widgets GUI: `ControlClient`, `AreaCanvas`, `MainWindow` |
| `tests/` | Qt Test suites and their shared fakes |
| `panels.json` | the supported panels; built into the daemon, which renders the udev rule and the table in `docs/supported-panels.md` from it |
| `scripts/` | Linux install and docs update; Windows install, uninstall and control |
