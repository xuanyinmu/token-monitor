# Qt rewrite (C++ / QML)

Windows-first native port of Token Monitor. Electron remains the shipping product; this tree is a parallel implementation.

## Toolchain

- Qt 6.8.3 MSVC 2022 x64 (`D:\App\Qt\6.8.3\msvc2022_64`)
- VS 2022 Build Tools, CMake, Ninja (`D:\App\...`)

```powershell
cd qt
. .\scripts\env.ps1
.\scripts\build.ps1
.\scripts\download-tokscale.ps1   # once, into %APPDATA%\Token Monitor\tokscale.exe
.\scripts\windeploy.ps1           # deploy Qt runtime next to the exes
.\build\TokenMonitorQt.exe        # windowed subsystem: no console on double-click
```

Other binaries: `TokenMonitorHub.exe`, `TokenMonitorAgent.exe` (console CLIs). Widget `--scan-once` prints one usage snapshot as JSON — the exe is windowed, so it reattaches to the parent console when launched from a terminal.

CI: `.github/workflows/qt-build.yml` builds the three exes on Windows, runs the checks below, and uploads a `windeployqt`-deployed, ready-to-run artifact (forks only, mirroring the `windows-exe.yml` guard).

## Runtime dependencies

`scripts/windeploy.ps1` (windeployqt) places everything the exes need next to them:

- Qt6Core / Gui / Widgets / Qml / Quick / QuickControls2 / QuickLayouts / QuickEffects / QuickShapes / Svg / Network / Sql / Concurrent (+ QmlModels/QmlMeta/QmlWorkerScript)
- `platforms/qwindows.dll`, `styles/`, `imageformats/` (incl. `qsvgicon`), `iconengines/`, `tls/` backends, `networkinformation/`
- ANGLE/D3D pieces shipped by windeployqt (`D3Dcompiler_47.dll`, `libEGL`, `libGLESv2`, `opengl32sw.dll`)

One non-Qt dependency at runtime: `tokscale.exe` (the vendored usage scanner) under `%APPDATA%\Token Monitor\` — install once with `scripts/download-tokscale.ps1`.

## Data

Reads/writes the same `%APPDATA%\Token Monitor\` tree as Electron (`settings.json`, `credentials.json`). Hub protocol matches `docs/API.md`; health `runtime` is `native` (no JS `hubBuild` hash).

## Tests

```
.\build\TokenMonitorDeltaCheck.exe
.\build\TokenMonitorHubCheck.exe
.\build\TokenMonitorBurnCheck.exe
```
