# Contributing

## Building on Linux

The project requires Linux, CMake 3.22 or newer, Ninja, a C++20 compiler, Qt
6.5 or newer (Core, Network, Widgets, and Test), `pkg-config`, and libevdev. On
Debian-based distributions, install the build dependencies with:

```bash
sudo apt install build-essential cmake ninja-build pkg-config qt6-base-dev libevdev-dev
```

Make sure your distribution's `qt6-base-dev` provides Qt 6.5 or newer. Then
configure, build, and run the tests:

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
```

The daemon can run without hardware for GUI and protocol work:

```bash
./build/daemon/touch2tabletd --no-device --socket /tmp/t2t.sock --settings /tmp/t2t.json &
./build/gui/touch2tablet --socket /tmp/t2t.sock
```

The control socket protocol is documented in [docs/protocol.md](docs/protocol.md).

## Building on Windows

The native Windows backend, GUI and tests build with MSVC and Qt. Unsigned
development builds use `--no-device`; hardware capture requires signed UIAccess
installation as described in the [Windows guide](docs/windows.md).

Install Visual Studio 2022 Build Tools with the **Desktop development with C++**
workload, including the Windows SDK and C++ CMake tools (which include Ninja).
Install Qt 6.5 or newer for the matching compiler. The verified development
setup uses MSVC 19.44, CMake 3.31.6, Ninja 1.12.1, and Qt 6.8.3 for MSVC 2022 x64.

With Python installed, Qt's packages can be downloaded using
[aqtinstall](https://aqtinstall.readthedocs.io/en/stable/getting_started.html):

```bat
python -m pip install aqtinstall
python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --outputdir "%LOCALAPPDATA%\Qt" --archives qtbase qttools
```

Open **x64 Native Tools Command Prompt for VS 2022**, change to the repository,
and run:

```bat
set "QTDIR=%LOCALAPPDATA%\Qt\6.8.3\msvc2022_64"
set "PATH=%QTDIR%\bin;%PATH%"
cmake -S . -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="%QTDIR%"
cmake --build build/windows --parallel 4
ctest --test-dir build/windows --output-on-failure
```

Keep Qt's `bin` directory on PATH when running the executables. In that prompt,
start the daemon:

```bat
build\windows\touch2tabletd.exe --no-device --socket touch2tablet-dev --settings build/windows/settings.json
```

In a second prompt with Qt's `bin` on PATH, run:

```bat
build\windows\touch2tablet.exe --socket touch2tablet-dev
```

The explicit socket name uses a Windows named pipe. The tests exercise the
same transport; only the Unix-specific stale socket file test is skipped.

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
