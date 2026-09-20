# Qt rewrite (C++ / QML)

Windows-first native port of Token Monitor. Electron remains the shipping product; this tree is a parallel implementation.

## Toolchain

- Qt 6.8.3 MSVC 2022 x64 (`D:\App\Qt\6.8.3\msvc2022_64`)
- VS 2022 Build Tools, CMake, Ninja (`D:\App\...`)

`scripts/env.ps1` resolves Qt and MSVC for a developer machine *and* for CI (where `install-qt-action` and `msvc-dev-cmd` put `qmake`/`windeployqt`/`cl` on PATH): `QTDIR` / `QT_ROOT_DIR` / `CMAKE_PREFIX_PATH`, then `qmake` on PATH, then the local kit path above, and it sources `vcvars.ps1` only when `cl.exe` is not already available.

```powershell
cd qt
.\scripts\build.ps1               # configure + compile + the three checks
.\scripts\download-tokscale.ps1   # once, into %APPDATA%\Token Monitor\tokscale.exe
.\scripts\windeploy.ps1           # deploy the Qt runtime next to the exes (for dev runs)
.\build\TokenMonitorQt.exe        # windowed subsystem: no console on double-click
```

Other binaries: `TokenMonitorHub.exe`, `TokenMonitorAgent.exe` (console CLIs). Widget `--scan-once` prints one usage snapshot as JSON — the exe is windowed, so it reattaches to the parent console when launched from a terminal.

## Packaging

```powershell
.\qt\scripts\package.ps1                  # installer + portable ZIP into qt\out\
.\qt\scripts\package.ps1 -NoInstaller     # portable ZIP only (no NSIS needed)
.\qt\scripts\package.ps1 -VerifyInstall   # + install/uninstall verification (see below)
```

`package.ps1` is the single packaging entry point (CI calls it with `-SkipBuild -SkipChecks`):

1. `build.ps1` (configure, compile, `checks.ps1`).
2. `windeploy.ps1` when `qt\build` has no deployed runtime yet.
3. **Stage** `qt\out\package`: the three exes, `build\*.dll`, the plugin directories (`platforms`, `styles`, `tls`, `imageformats`, `iconengines`, `networkinformation`, `sqldrivers`, `generic`, `qml`), the **app-local MSVC runtime** (`Microsoft.VC143.CRT`, from `VCToolsRedistDir`), `tokscale.exe`, `LICENSE` and `installer\README.txt`. No `vc_redist.x64.exe` (25 MB, needs elevation), no CMake/ninja leftovers, no test binaries.
4. **Assert** the files a missing piece would silently break: `msvcp140.dll`, `vcruntime140.dll`, `platforms\qwindows.dll`, `tls\qschannelbackend.dll`, `iconengines\qsvgicon.dll`, `sqldrivers\qsqlite.dll`, `tokscale.exe`, `qml\QtQuick\Controls\Basic`. A missing `qsqlite.dll` costs Cursor's token fallback and Qoder CN usage; a missing CRT means the exe does not start at all.
5. **Smoke test** (`scripts\smoke-test.ps1`) with `PATH` stripped to `System32`, so a deployment that only works because the development machine has Qt on PATH fails here: `TokenMonitorQt.exe --screenshot` must produce a real PNG (retrying with `QT_QUICK_BACKEND=software` for GPU-less runners), `TokenMonitorQt.exe --self-test-top-edge` must print `top-edge-ok` (the dock/reveal cycle that crashed 0.57), and both CLIs must answer `--help`.
6. Portable ZIP (`Compress-Archive`, contents at the ZIP root), then the NSIS installer.

`tokscale.exe` is a hard dependency (the widget has no self-download path), so packaging takes `-TokscalePath`, else the first copy whose SHA256 matches `scripts\vendor\tokscale.json` — `%APPDATA%\Token Monitor\tokscale.exe`, or the npm CLI package under `node_modules\@tokscale\cli-win32-x64-msvc\bin` so a checkout can package offline — else the pinned release asset, and it **fails** rather than shipping a package that cannot scan.

`makensis.exe` is located by path — `-MakensisPath`, `PATH`, `%ProgramFiles(x86)%\NSIS`, the chocolatey shim, then electron-builder's cached NSIS (which is why a local installer needs no extra download). CI installs NSIS with `choco install nsis`.

Outputs (`qt\out\`, rebuilt from scratch on every run; the stage is kept for inspection):

- `Token-Monitor-Qt-Setup-<version>.exe` — installer
- `Token-Monitor-Qt-<version>-win-x64-portable.zip` — extract and run
- both are **unsigned**: SmartScreen shows an unknown publisher warning. Signing stays with the Electron release pipeline (SignPath).

CI: `.github/workflows/qt-build.yml` (forks only, mirroring the `windows-exe.yml` guard) builds, packages, verifies and uploads `token-monitor-qt-installer` + `token-monitor-qt-portable`.

## Runtime dependencies

`windeploy.ps1` puts everything the exes need next to them:

- Qt6Core / Gui / Widgets / Qml / Quick / QuickControls2 / QuickLayouts / QuickEffects / QuickShapes / Svg / Network / Sql / Concurrent (+ QmlModels/QmlMeta/QmlWorkerScript)
- `platforms/qwindows.dll`, `styles/`, `imageformats/` (incl. `qsvgicon`), `iconengines/`, `tls/` backends, `networkinformation/`, `sqldrivers/qsqlite.dll`
- ANGLE/D3D pieces shipped by windeployqt (`D3Dcompiler_47.dll`, `libEGL`, `libGLESv2`, `opengl32sw.dll`)

Packaging adds two things windeployqt does not: the MSVC runtime DLLs (app-local, no redistributable install and no UAC — traded against not being serviced by Windows Update) and `tokscale.exe`. `Paths::tokscaleBinary()` prefers `%APPDATA%\Token Monitor\tokscale.exe` and falls back to the copy beside the exe, so a machine that already ran `download-tokscale.ps1` keeps using that one.

## Installer behaviour

- Per-user, `%LOCALAPPDATA%\Programs\Token Monitor Qt`, `RequestExecutionLevel user` (no UAC), directory page available — same mode as the Electron build's NSIS installer.
- Components: the app (required) and a desktop shortcut (optional). There is deliberately **no** "start with Windows" component: 0.57 had one that NSIS selects by default, which started the widget at login while `settings.json` still said `startAtLogin: false`. Autostart now belongs to the widget's own Settings → start with Windows, and the widget syncs that setting from the Windows state at startup (the same thing Electron does with `syncLoginItemSettingFromOs`), so an entry written by anything else is shown as enabled and can be turned off in one place.
- The widget exe carries its own icon (`resources/app.rc.in` → `resources/app.ico`) and a version resource generated from `tmon.h`, so crash reports and Task Manager identify the build (0.57 reported `0.0.0.0`).
- `README.txt` in the install directory explains the binaries, the data locations and the uninstall contract.

## Uninstalling (leaves nothing behind)

`Uninstall.exe`, or Settings → Apps → Token Monitor (Qt).

The uninstaller stops the widget/hub/agent, then removes the install directory, both shortcuts, the "Apps & features" entry, and every file this build writes: `%APPDATA%\Token Monitor\limits-snapshot.json`, `data\`, `qt-*.json`, `qt-*.log`, `tokscale.exe`; `%LOCALAPPDATA%\Token Monitor\cache`; `%LOCALAPPDATA%\Javis\Token Monitor`. Empty husks are pruned. The autostart entry is removed **only when it points into the install directory being removed** — an entry pointing elsewhere belongs to another copy (portable build, second install) and is left alone.

`%APPDATA%\Token Monitor` is shared with the Electron build (`settings.json`, `credentials.json`, `history.json`, … belong to both), so the whole directory is only removed when it holds no Electron trace, or when the user answers Yes, or on a silent uninstall. `Uninstall.exe /S` removes everything; `Uninstall.exe /S /KEEPDATA` keeps the files shared with the Electron build. Interactive runs ask, and the details pane reports what is left if anything survived.

## Verifying an install

```powershell
.\qt\scripts\verify-install.ps1 -InstallerPath .\qt\out\Token-Monitor-Qt-Setup-0.57.0.exe
```

Refuses to run while a Token Monitor process is alive, backs up `%APPDATA%\Token Monitor`, `%LOCALAPPDATA%\Token Monitor` and `%LOCALAPPDATA%\Javis` (plus every HKCU Run value, the uninstall key and the shortcuts) with a SHA256 manifest, then:

- **A** extracts the portable ZIP (asserting the ZIP root has no extra folder) and smoke tests it with an isolated `TOKEN_MONITOR_USER_DATA`;
- **B** silently installs, checks the installed files/shortcuts/uninstall entry, asserts the installer left the HKCU autostart value **untouched**, smoke tests (including the top-edge self-test), seeds an autostart entry pointing into the install directory, silently uninstalls, and asserts the whole footprint is gone;
- **C** seeds an autostart entry pointing *outside* the install directory, installs and uninstalls with `/S /KEEPDATA`, and asserts the Qt-owned files are gone, the shared ones (and the Electron-only Chromium markers) survive, and the outside entry was not disturbed.

Other programs' Run values are checked in the two windows where only this script runs: a sentinel value (pointing at a real executable, so a startup cleaner has no reason to remove it) must survive each install/uninstall step, and every seeding step compares the set of foreign names across itself, because `New-Item -Force` recreates a registry key and takes its values with it — the bug that once emptied a real Run key. Nothing but `TokenMonitor` and that sentinel is ever written.

After each pass the backup is restored and re-hashed: a mismatch is an error, so the destructive part cannot leave the machine changed. A value that the backup recorded and that is missing afterwards is reported with its command line (it is not put back — the Electron build rewrites its own login item). `package.ps1 -VerifyInstall` runs this automatically; CI runs it on every build.

## Data

Reads/writes the same `%APPDATA%\Token Monitor\` tree as Electron (`settings.json`, `credentials.json`). Hub protocol matches `docs/API.md`; health `runtime` is `native` (no JS `hubBuild` hash).

## Tests

```
.\scripts\checks.ps1
```
(`build.ps1` and `package.ps1` call it; it runs `TokenMonitorDeltaCheck.exe`, `TokenMonitorHubCheck.exe`, `TokenMonitorBurnCheck.exe` from `build\`.)
