$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'env.ps1')

$BuildDir = Join-Path $Root 'build'
$Config = if ($args.Count -gt 0) { $args[0] } else { 'Release' }

if (-not (Test-Path -LiteralPath (Join-Path $BuildDir 'build.ninja'))) {
    cmake -S $Root -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=$Config
}

cmake --build $BuildDir --config $Config
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$check = Join-Path $BuildDir 'TokenMonitorDeltaCheck.exe'
if (Test-Path -LiteralPath $check) {
    & $check
    if ($LASTEXITCODE -ne 0) { throw 'TokenMonitorDeltaCheck failed' }
}
$hubCheck = Join-Path $BuildDir 'TokenMonitorHubCheck.exe'
if (Test-Path -LiteralPath $hubCheck) {
    & $hubCheck
    if ($LASTEXITCODE -ne 0) { throw 'TokenMonitorHubCheck failed' }
}
$burnCheck = Join-Path $BuildDir 'TokenMonitorBurnCheck.exe'
if (Test-Path -LiteralPath $burnCheck) {
    & $burnCheck
    if ($LASTEXITCODE -ne 0) { throw 'TokenMonitorBurnCheck failed' }
}
