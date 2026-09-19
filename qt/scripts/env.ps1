$ErrorActionPreference = 'Stop'

# Qt + MSVC resolution shared by build.ps1 and package.ps1.
#
# It has to work in two very different places: a developer machine with the kit
# installed at a known path, and a CI runner where install-qt-action and
# ilammy/msvc-dev-cmd already put qmake/windeployqt/cl on PATH (and export
# QT_ROOT_DIR). Order is therefore explicit env first, then PATH, then the
# local kit - so one script drives every entry point instead of CI re-deriving
# the build command by hand.

$LocalQtRoot = 'D:\App\Qt\6.8.3\msvc2022_64'
$CMakeRoot = 'D:\App\CMake'
$NinjaRoot = 'D:\App\Ninja'

function Test-QtKitRoot {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return $false }
    return Test-Path -LiteralPath (Join-Path $Path 'bin\qmake.exe')
}

function Find-QtKitRoot {
    foreach ($candidate in @($env:QTDIR, $env:QT_ROOT_DIR)) {
        if (Test-QtKitRoot $candidate) { return (Resolve-Path -LiteralPath $candidate).Path }
    }
    foreach ($entry in @($env:CMAKE_PREFIX_PATH -split ';')) {
        if (Test-QtKitRoot $entry) { return (Resolve-Path -LiteralPath $entry).Path }
    }
    $qmake = Get-Command qmake -ErrorAction SilentlyContinue
    if ($qmake) {
        # <root>\bin\qmake.exe -> <root>
        $root = Split-Path -Parent (Split-Path -Parent $qmake.Source)
        if (Test-QtKitRoot $root) { return (Resolve-Path -LiteralPath $root).Path }
    }
    if (Test-QtKitRoot $LocalQtRoot) { return $LocalQtRoot }
    return $null
}

$QtRoot = Find-QtKitRoot
if (-not $QtRoot) {
    throw "No Qt kit found. Set QTDIR (or QT_ROOT_DIR / CMAKE_PREFIX_PATH) to a kit that contains bin\qmake.exe, put qmake on PATH, or install the local kit at $LocalQtRoot."
}

# A CI runner already sourced msvc-dev-cmd, so cl.exe resolves and VsDevCmd
# would only cost seconds; a bare local shell has no cl.exe at all.
$cl = Get-Command cl -ErrorAction SilentlyContinue
if (-not $cl) {
    . (Join-Path $PSScriptRoot 'vcvars.ps1')
    $cl = Get-Command cl -ErrorAction SilentlyContinue
}
if (-not $cl) { throw 'cl.exe not found after VsDevCmd; install the MSVC C++ build tools.' }

$prepend = @(
    (Join-Path $QtRoot 'bin'),
    (Join-Path $CMakeRoot 'bin'),
    $NinjaRoot
) | Where-Object { Test-Path -LiteralPath $_ }

$env:CMAKE_PREFIX_PATH = $QtRoot
$env:QTDIR = $QtRoot
$env:PATH = ($prepend + $env:PATH) -join ';'

Write-Host "Qt:    $QtRoot"
foreach ($tool in @('cmake', 'ninja', 'cl', 'windeployqt')) {
    $found = Get-Command $tool -ErrorAction SilentlyContinue
    if ($found) { Write-Host ("{0,-12}{1}" -f ($tool + ':'), $found.Source) }
    else { Write-Host ("{0,-12}{1}" -f ($tool + ':'), '(not on PATH)') }
}
