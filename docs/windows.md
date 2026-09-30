# Windows

The Windows daemon captures the supported touchscreen and emits absolute mouse movement and
left-button events. Mapping, first-finger handling, settings, presets and the GUI are shared with
Linux. No kernel driver is installed.

## Use

Open **touch2tablet** from the Start menu. The GUI starts the daemon, which captures the panel.
Touching holds the left mouse button and lifting releases it.

- **Apply** changes the live mapping; **Save** persists it.
- Closing the window leaves touch2tablet in the notification area. Its menu offers
  **Use as touchscreen**, **Enable tablet mode**, **Start with Windows** and
  **Exit and restore touchscreen**.
- To pick a monitor, move the window onto it, choose **Settings > Detect from the monitor this
  window is on**, then Apply or Save. `display.output` then holds a device name such as
  `\\.\DISPLAY1`; detect again if Windows renames monitors after a hardware change.
- If the monitor disappears, capture is released and native touch works until it returns.
  Unplugging the panel releases held input.

Settings are in `%LOCALAPPDATA%\touch2tablet\settings.json`, with presets in the `presets` folder
next to it. The GUI's **Console** tab shows capture and output errors. "Not attached" can also
mean the selected monitor is unavailable or capture permission is missing.

## Build and install

Requires Windows 10 1809+ and the toolchain from [CONTRIBUTING](../CONTRIBUTING.md). Build a
release with UIAccess and stage it with its Qt libraries:

```powershell
cmake -S . -B build/windows-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$env:LOCALAPPDATA/Qt/6.8.3/msvc2022_64" -DT2T_WINDOWS_UIACCESS=ON
cmake --build build/windows-release --parallel
ctest --test-dir build/windows-release --output-on-failure
New-Item -ItemType Directory -Force build/windows-release/stage
Copy-Item build/windows-release/touch2tablet.exe,build/windows-release/touch2tabletd.exe build/windows-release/stage
windeployqt --release --no-translations build/windows-release/stage/touch2tablet.exe build/windows-release/stage/touch2tabletd.exe
```

Capturing all touch input requires a UIAccess manifest, a trusted signature and installation in
a protected directory (see Microsoft's
[RegisterPointerInputTarget requirements](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerpointerinputtarget)).
Sign the staged daemon, then run `scripts/install-windows.ps1 -Stage <absolute staging path>` as
administrator. It installs to Program Files and adds a Start menu shortcut. For local
development, `-CertificateThumbprint` signs with a code-signing certificate you already trust in
`LocalMachine\My`; the script never creates certificates or changes trust. Exit via the tray
before updating.

`touch2tabletd --identify` inspects devices, `--log <file>` writes a log, and
`scripts/windows-control.ps1` sends [control protocol](protocol.md) requests.

To uninstall, turn off **Start with Windows**, choose **Exit and restore touchscreen**, then run
`scripts/uninstall-windows.ps1` as administrator. Settings, presets and certificates are kept.

## Limitations

- Only one touchscreen may be connected. Windows redirects all touch input at once, so capture
  is refused while another touchscreen is present.
- The output is always an absolute mouse (`output_mode` is ignored). There is no pen, WinTab or
  pressure output.
- Hardware testing used the supported SingWon panel on a laptop with an external monitor,
  including release osu!lazer (no modified osu-framework needed), reconnects, sleep/resume and
  startup after reboot. Forced-crash recovery and unusual display or DPI setups are untested.

Output follows OpenTabletDriver's absolute-mouse approach. See its
[Windows compatibility notes](https://opentabletdriver.net/Wiki/FAQ/WindowsAppSpecific).
