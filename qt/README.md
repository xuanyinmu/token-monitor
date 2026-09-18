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
.\build\TokenMonitor.exe
```

Other binaries: `TokenMonitorHub.exe`, `TokenMonitorAgent.exe`. Widget `--scan-once` prints one usage snapshot as JSON.

## Data

Reads/writes the same `%APPDATA%\Token Monitor\` tree as Electron (`settings.json`, `credentials.json`). Hub protocol matches `docs/API.md`; health `runtime` is `native` (no JS `hubBuild` hash).

## Tests

```
.\build\TokenMonitorDeltaCheck.exe
.\build\TokenMonitorHubCheck.exe
.\build\TokenMonitorBurnCheck.exe
```
