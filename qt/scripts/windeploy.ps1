$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'env.ps1')

$BuildDir = Join-Path $Root 'build'
$Exe = Join-Path $BuildDir 'TokenMonitorQt.exe'
$QmlDir = Join-Path $Root 'qml'
if (-not (Test-Path -LiteralPath $Exe)) { throw "Build TokenMonitorQt.exe first: $Exe" }

windeployqt --release --qmldir $QmlDir --no-translations $Exe
windeployqt --release --no-translations (Join-Path $BuildDir 'TokenMonitorHub.exe')
windeployqt --release --no-translations (Join-Path $BuildDir 'TokenMonitorAgent.exe')
