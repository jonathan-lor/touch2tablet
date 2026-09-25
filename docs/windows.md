# Windows

The Windows daemon captures the supported touchscreen and emits absolute mouse
movement and left-button events. Mapping, first-finger handling, settings,
presets and the Qt GUI are shared with Linux. No kernel driver is installed.

## Use

Open **touch2tablet** from the Start menu. The GUI starts the daemon and captures
the panel automatically. Contact holds the left mouse button; lifting releases
it. Additional fingers cannot take over an existing stroke.

- **Apply** changes the live mapping; **Save** persists it.
- Closing settings leaves the app in the notification area beside the clock.
  Its tray menu offers **Use as touchscreen**, **Enable tablet mode**, optional
  **Start with Windows**, and **Exit and restore touchscreen**.
- Select a display by moving settings onto it and choosing **Settings > Detect
  from the monitor this window is on**, then Apply/Save. Empty `display.output`
  follows the primary display; otherwise it contains a Windows device name such
  as `\\.\DISPLAY1`, not a friendly label. Detect again if Windows reassigns
  device names after a display hardware change.
- Output scales to the selected monitor's current pixel bounds. If it disappears,
  the next output attempt releases capture. Native touch remains available while
  the daemon retries until the selected output can be reopened.
- Panel unplug/replug is detected automatically and held input is released.

Settings are in `%LOCALAPPDATA%\touch2tablet\settings.json`, with presets in the
adjacent `presets` folder. Unsaved changes do not survive a daemon restart.
The GUI Console reports capture, output and recovery errors. A panel can be
“not attached” because of an unavailable selected monitor or capture permission
error, as well as a disconnected device.

## Build and install

Requires Windows 10 1809+, MSVC, the Windows SDK, CMake, Ninja and Qt 6.5+
(Core, Network, Widgets and Test). See [CONTRIBUTING](../CONTRIBUTING.md).
From an x64 Visual Studio developer shell with Qt on PATH:

```powershell
cmake -S . -B build/windows-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$env:LOCALAPPDATA/Qt/6.8.3/msvc2022_64" -DT2T_WINDOWS_UIACCESS=ON
cmake --build build/windows-release --parallel
ctest --test-dir build/windows-release --output-on-failure
New-Item -ItemType Directory -Force build/windows-release/stage
Copy-Item build/windows-release/touch2tablet.exe,build/windows-release/touch2tabletd.exe build/windows-release/stage
windeployqt --release --no-translations build/windows-release/stage/touch2tablet.exe build/windows-release/stage/touch2tabletd.exe
```

Global touch redirection requires a UIAccess manifest, a trusted signature and
installation in a protected directory. See Microsoft's
[RegisterPointerInputTarget requirements](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerpointerinputtarget).
The GUI launches the daemon through ShellExecute in the interactive user session.

Sign the staged daemon with your code-signing certificate, then run
`scripts/install-windows.ps1 -Stage <absolute staging path>` as administrator.
It installs to Program Files and creates a Start menu shortcut. Exit via the
tray before updating. For local development, `-CertificateThumbprint` can sign
with an already trusted code-signing certificate in `LocalMachine\My`. The
installer does not create certificates or change trust. Development certificates
are not a production distribution arrangement.

Unsigned builds can leave `T2T_WINDOWS_UIACCESS=OFF` and use `--no-device` for
GUI/protocol work. The daemon offers `--identify` for device inspection and
`--log <file>` for logging. `scripts/windows-control.ps1` sends requests over the
user's named pipe; see the [control protocol](protocol.md).

To uninstall, disable **Start with Windows**, choose **Exit and restore
touchscreen**, then run `scripts/uninstall-windows.ps1` as administrator.
Settings, presets and signing certificates are preserved.

## Scope and validation

Hardware validation covered the supported SingWon panel on a laptop with an
external monitor: desktop and release osu!lazer aiming, taps/drags, first-finger
handling, rotation, clamp/ignore options, presets, persistence, tray controls,
panel and monitor reconnect, fullscreen/Alt-Tab, lock/unlock, sleep/resume and
automatic startup after reboot. Automated tests cover mapping, contact decoding,
mouse output, failure recovery, settings and the control protocol.

Remaining limits:

- Exactly one touchscreen is required. Windows redirects the entire touch
  pointer type; capture is refused when another touchscreen is present.
- Coalesced movement history is not replayed; latency equivalence with Linux
  has not been measured.
- Windows emits an absolute mouse; Linux emits a virtual pen. Release lazer
  worked without a modified osu-framework. Other applications may distinguish
  the output types; there is no WinTab or pressure support.
- Forced-crash recovery and every display/DPI configuration have not been
  validated on hardware. Windows tests do not replace Linux runtime testing.

The output follows OpenTabletDriver's absolute-mouse approach; see its
[Windows compatibility notes](https://opentabletdriver.net/Wiki/FAQ/WindowsAppSpecific).
