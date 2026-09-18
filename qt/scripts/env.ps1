$ErrorActionPreference = 'Stop'

$QtRoot = 'D:\App\Qt\6.8.3\msvc2022_64'
$CMakeRoot = 'D:\App\CMake'
$NinjaRoot = 'D:\App\Ninja'

if (-not (Test-Path -LiteralPath (Join-Path $QtRoot 'bin\qmake.exe'))) {
    throw "Qt 6.8.3 MSVC kit not found at $QtRoot"
}

. (Join-Path $PSScriptRoot 'vcvars.ps1')

$prepend = @(
    (Join-Path $QtRoot 'bin'),
    (Join-Path $CMakeRoot 'bin'),
    $NinjaRoot
) | Where-Object { Test-Path -LiteralPath $_ }

$env:CMAKE_PREFIX_PATH = $QtRoot
$env:QTDIR = $QtRoot
$env:PATH = ($prepend + $env:PATH) -join ';'

Write-Host "Qt:    $QtRoot"
Write-Host "CMake: $(Get-Command cmake | Select-Object -ExpandProperty Source)"
Write-Host "Ninja: $(Get-Command ninja | Select-Object -ExpandProperty Source)"
Write-Host "cl:    $(Get-Command cl | Select-Object -ExpandProperty Source)"
