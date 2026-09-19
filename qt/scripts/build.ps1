$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'env.ps1')

$BuildDir = Join-Path $Root 'build'
$Config = if ($args.Count -gt 0) { $args[0] } else { 'Release' }

if (-not (Test-Path -LiteralPath (Join-Path $BuildDir 'build.ninja'))) {
    cmake -S $Root -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=$Config
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

cmake --build $BuildDir --config $Config
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $PSScriptRoot 'checks.ps1')
