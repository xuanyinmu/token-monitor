$ErrorActionPreference = 'Stop'

# The three behavioural checks that guard the ported core logic. Kept in one
# script so build.ps1 and package.ps1 cannot drift on which checks run or how
# their exit codes are treated.

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root 'build'
if (-not (Test-Path -LiteralPath $BuildDir)) { throw "Build directory not found: $BuildDir" }

$names = @('TokenMonitorDeltaCheck.exe', 'TokenMonitorHubCheck.exe', 'TokenMonitorBurnCheck.exe')
foreach ($name in $names) {
    $exe = Join-Path $BuildDir $name
    if (-not (Test-Path -LiteralPath $exe)) { throw "Check binary missing: $exe" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "$name failed with exit code $LASTEXITCODE" }
}

Write-Host "Checks passed: $($names -join ', ')"
