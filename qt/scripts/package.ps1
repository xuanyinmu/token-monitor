<#
.SYNOPSIS
  Builds the Windows deliverables for the Qt port: a portable ZIP and an NSIS
  installer, both from one staged deployment of qt\build.

.DESCRIPTION
  One code path for the local machine and for CI. Steps:

    1. build (build.ps1: configure + compile + the three check binaries)
    2. windeploy.ps1 if qt\build has no deployed Qt runtime yet
    3. mirror the deployed runtime into qt\out\package, adding the pieces
       windeployqt does not ship: the app-local MSVC runtime and tokscale.exe
    4. assert the required files (a missing qsqlite.dll or CRT is a silent
       runtime failure, so it fails the package instead)
    5. smoke test the staged tree with PATH stripped to System32, so it cannot
       pass by borrowing the development machine's Qt
    6. portable ZIP, then the NSIS installer (unless -NoInstaller)

  Outputs land in qt\out\. Nothing outside qt\out\ is written.

  The stage is rebuilt from scratch on every run, so a stale deployment cannot
  be shipped. It is deliberately kept afterwards: it is the portable tree and
  the fastest way to inspect what went into the installer.

.PARAMETER Configuration
  Build configuration for build.ps1 (default Release).

.PARAMETER SkipBuild
  Use the existing qt\build. CI calls this after its own build step.

.PARAMETER SkipChecks
  Do not run the three check binaries. Only meaningful together with -SkipBuild
  (build.ps1 runs them itself); CI already ran them as its own step.

.PARAMETER MakensisPath
  Path to makensis.exe. Otherwise PATH, the usual NSIS/chocolatey locations and
  electron-builder's cached NSIS are searched, in that order.

.PARAMETER TokscalePath
  tokscale.exe to bundle. Otherwise a copy already verified against
  scripts\vendor\tokscale.json is reused, or the pinned release is downloaded.

.PARAMETER NoInstaller
  Produce only the portable ZIP (no NSIS compiler needed).

.PARAMETER VerifyInstall
  After packaging, run scripts\verify-install.ps1: silent install into a
  temporary directory, smoke test, silent uninstall, residue assertions, then
  restore of the pre-test user data. That script backs up and hashes the data
  directories before touching anything.
#>
[CmdletBinding()]
param(
    [string]$Configuration = 'Release',
    [switch]$SkipBuild,
    [switch]$SkipChecks,
    [string]$MakensisPath,
    [string]$TokscalePath,
    [switch]$NoInstaller,
    [switch]$VerifyInstall
)

$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $PSScriptRoot          # qt\
$RepoRoot = Split-Path -Parent $Root              # repository root
. (Join-Path $PSScriptRoot 'env.ps1')

$BuildDir = Join-Path $Root 'build'
$OutRoot = Join-Path $Root 'out'
$Stage = Join-Path $OutRoot 'package'
$InstallerDir = Join-Path $Root 'installer'
$ManifestPath = Join-Path $RepoRoot 'scripts\vendor\tokscale.json'

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

function Get-AppVersion {
    $header = Get-Content -LiteralPath (Join-Path $Root 'src\core\tmon.h') -Raw
    if ($header -notmatch 'kAppVersion\s*=\s*"([^"]+)"') {
        throw "Could not read kAppVersion from $Root\src\core\tmon.h"
    }
    return $Matches[1]
}

# Version resource form: 0.57.0 -> 0.57.0.0 (NSIS needs four numbers).
function Get-ViVersion {
    param([string]$Version)
    $parts = @($Version -split '\.' | ForEach-Object { $_ -replace '[^0-9]', '' })
    while ($parts.Count -lt 4) { $parts += '0' }
    return ($parts[0..3] -join '.')
}

# The MSVC runtime the exe needs at startup. windeployqt does not copy these
# (it drops a 25 MB vc_redist.x64.exe instead) and the .exe globs in CI never
# picked that up, which is why a clean machine could not start the widget.
# Deploying the CRT app-locally is a documented Microsoft option and keeps the
# per-user install free of an elevation prompt.
function Resolve-CrtDir {
    $candidates = New-Object System.Collections.Generic.List[string]

    if ($env:VCToolsRedistDir) {
        $candidates.Add((Join-Path $env:VCToolsRedistDir 'x64\Microsoft.VC143.CRT'))
    }
    if ($env:VCINSTALLDIR) {
        Get-ChildItem -Path (Join-Path $env:VCINSTALLDIR 'Redist\MSVC\*\x64\Microsoft.VC*.CRT') -Directory -ErrorAction SilentlyContinue |
            Sort-Object -Property FullName -Descending |
            ForEach-Object { $candidates.Add($_.FullName) }
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($install) {
            Get-ChildItem -Path (Join-Path $install 'VC\Redist\MSVC\*\x64\Microsoft.VC*.CRT') -Directory -ErrorAction SilentlyContinue |
                Sort-Object -Property FullName -Descending |
                ForEach-Object { $candidates.Add($_.FullName) }
        }
    }

    foreach ($dir in $candidates) {
        if (Test-Path -LiteralPath (Join-Path $dir 'msvcp140.dll')) { return $dir }
    }
    return $null
}

function Get-Sha256 {
    param([string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

# tokscale is a hard runtime dependency: without it the widget collects nothing
# (`tokscale_missing`). Reuse a verified copy when there is one, otherwise take
# the pinned release asset and verify its hash; a package that cannot scan usage
# is not worth shipping, so this throws instead of degrading.
function Install-Tokscale {
    param([string]$Destination, [string]$Explicit)

    $manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    $asset = $manifest.platforms.'win32-x64'
    $expected = $asset.sha256

    $candidates = New-Object System.Collections.Generic.List[string]
    if ($Explicit) { $candidates.Add($Explicit) }
    if ($env:APPDATA) { $candidates.Add((Join-Path (Join-Path $env:APPDATA 'Token Monitor') 'tokscale.exe')) }
    # The npm CLI package the Electron build uses ships the same pinned binary, so
    # a checkout that has run npm install can package without network access.
    # Every candidate is hash-checked, so "the same file by convention" is never
    # trusted: only a match against scripts/vendor/tokscale.json is reused.
    if ($asset.package) {
        $packageDir = Join-Path 'node_modules' ($asset.package -replace '/', '\')
        $candidates.Add((Join-Path $RepoRoot (Join-Path $packageDir 'bin\tokscale.exe')))
    }

    foreach ($candidate in $candidates) {
        if (-not (Test-Path -LiteralPath $candidate)) { continue }
        $hash = Get-Sha256 $candidate
        if ($hash -eq $expected) {
            Copy-Item -LiteralPath $candidate -Destination $Destination -Force
            Write-Host "tokscale: reused $candidate"
            return
        }
        Write-Warning "tokscale: ignoring $candidate (sha256 $hash does not match the pinned $expected)"
    }

    try {
        & (Join-Path $PSScriptRoot 'download-tokscale.ps1') -Dest $Destination -Force
    } catch {
        throw "Could not obtain tokscale.exe (pinned to $($manifest.releaseRepo) $($manifest.releaseTag)): $($_.Exception.Message). Pass -TokscalePath, or run qt\scripts\download-tokscale.ps1 once while online."
    }
    if (-not (Test-Path -LiteralPath $Destination)) {
        throw "tokscale download reported success but $Destination does not exist."
    }
    Write-Host "tokscale: downloaded $($manifest.releaseRepo) $($manifest.releaseTag)"
}

function Find-Makensis {
    param([string]$Explicit)

    $candidates = New-Object System.Collections.Generic.List[string]
    if ($Explicit) { $candidates.Add($Explicit) }

    $command = Get-Command makensis -ErrorAction SilentlyContinue
    if ($command) { $candidates.Add($command.Source) }

    if (${env:ProgramFiles(x86)}) { $candidates.Add((Join-Path ${env:ProgramFiles(x86)} 'NSIS\makensis.exe')) }
    if ($env:ProgramFiles) { $candidates.Add((Join-Path $env:ProgramFiles 'NSIS\makensis.exe')) }
    if ($env:ProgramData) { $candidates.Add((Join-Path $env:ProgramData 'chocolatey\bin\makensis.exe')) }
    if ($env:LOCALAPPDATA) {
        # electron-builder downloads NSIS for the Electron installer; reusing it
        # is what makes a local Qt installer possible with no extra download.
        Get-ChildItem -Path (Join-Path $env:LOCALAPPDATA 'electron-builder\Cache\nsis-*\*\makensis.exe') -ErrorAction SilentlyContinue |
            Sort-Object -Property FullName -Descending |
            ForEach-Object { $candidates.Add($_.FullName) }
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) { return $candidate }
    }
    return $null
}

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------

# A CI failure has to say where it stopped, so every phase stamps elapsed time.
$script:PackageWatch = [System.Diagnostics.Stopwatch]::StartNew()
function Write-Phase {
    param([string]$Text)
    Write-Host ''
    Write-Host ("--- {0} (t+{1:N1}s) ---" -f $Text, $script:PackageWatch.Elapsed.TotalSeconds)
}

Write-Host ''
Write-Host '=== Token Monitor (Qt) packaging ==='
Write-Phase 'build'

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') $Configuration
    if ($LASTEXITCODE) { throw "build.ps1 failed with exit code $LASTEXITCODE" }
} elseif (-not $SkipChecks) {
    & (Join-Path $PSScriptRoot 'checks.ps1')
}

foreach ($exe in @('TokenMonitorQt.exe', 'TokenMonitorHub.exe', 'TokenMonitorAgent.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $BuildDir $exe))) {
        throw "$BuildDir\$exe is missing; build first (drop -SkipBuild)."
    }
}

$Version = Get-AppVersion
$cmakeText = Get-Content -LiteralPath (Join-Path $Root 'CMakeLists.txt') -Raw
if ($cmakeText -match 'project\([^)]*VERSION\s+([0-9][0-9.]*)') {
    if ($Matches[1] -ne $Version) {
        Write-Warning "Version drift: tmon.h says $Version, CMakeLists.txt says $($Matches[1]). The package is named from tmon.h."
    }
}
Write-Host "Version: $Version"

if (-not (Test-Path -LiteralPath (Join-Path $BuildDir 'platforms\qwindows.dll'))) {
    Write-Host 'Deploying the Qt runtime into qt\build (windeploy.ps1)'
    & (Join-Path $PSScriptRoot 'windeploy.ps1')
    if ($LASTEXITCODE) { throw "windeploy.ps1 failed with exit code $LASTEXITCODE" }
}

# ---------------------------------------------------------------------------
# Stage
# ---------------------------------------------------------------------------

Write-Phase 'stage: whitelisted deployment'
Write-Host "Staging $Stage"
if (Test-Path -LiteralPath $Stage) { Remove-Item -LiteralPath $Stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $Stage | Out-Null

foreach ($exe in @('TokenMonitorQt.exe', 'TokenMonitorHub.exe', 'TokenMonitorAgent.exe')) {
    Copy-Item -LiteralPath (Join-Path $BuildDir $exe) -Destination $Stage -Force
}
Copy-Item -Path (Join-Path $BuildDir '*.dll') -Destination $Stage -Force

$pluginDirs = @('platforms', 'styles', 'tls', 'imageformats', 'iconengines', 'networkinformation', 'sqldrivers', 'generic', 'qml')
foreach ($dir in $pluginDirs) {
    $source = Join-Path $BuildDir $dir
    if (Test-Path -LiteralPath $source) {
        Copy-Item -LiteralPath $source -Destination $Stage -Recurse -Force
    }
}

$crtDir = Resolve-CrtDir
if (-not $crtDir) {
    throw 'Could not find the MSVC runtime DLLs (Microsoft.VC*.CRT). Run from a Developer PowerShell / vsdevcmd shell, or set VCToolsRedistDir.'
}
Write-Phase 'stage: app-local MSVC runtime'
Write-Host "MSVC runtime: $crtDir"
Copy-Item -Path (Join-Path $crtDir '*.dll') -Destination $Stage -Force

Write-Phase 'stage: tokscale'
Install-Tokscale -Destination (Join-Path $Stage 'tokscale.exe') -Explicit $TokscalePath
Copy-Item -LiteralPath (Join-Path $RepoRoot 'LICENSE') -Destination $Stage -Force
Copy-Item -LiteralPath (Join-Path $InstallerDir 'README.txt') -Destination $Stage -Force

# Required-file assertions: each of these has caused (or would cause) a silent
# runtime failure - a missing plugin only shows up as a dead feature.
Write-Phase 'assert staged deployment'
$required = @(
    'TokenMonitorQt.exe',
    'TokenMonitorHub.exe',
    'TokenMonitorAgent.exe',
    'msvcp140.dll',
    'vcruntime140.dll',
    'platforms\qwindows.dll',
    'tls\qschannelbackend.dll',
    'iconengines\qsvgicon.dll',
    'sqldrivers\qsqlite.dll',
    'tokscale.exe',
    'qml\QtQuick\Controls\Basic'
)
$missing = @($required | Where-Object { -not (Test-Path -LiteralPath (Join-Path $Stage $_)) })
if ($missing.Count) {
    throw "Staged deployment is incomplete: $($missing -join ', '). Check that windeployqt ran against a full kit."
}
if (-not (Test-Path -LiteralPath (Join-Path $Stage 'vcruntime140_1.dll'))) {
    Write-Warning 'vcruntime140_1.dll is not in the stage; the C++ runtime deploy looks partial.'
}

$stagedBytes = (Get-ChildItem -LiteralPath $Stage -Recurse -File | Measure-Object -Property Length -Sum).Sum
Write-Host ("Staged {0:N1} MB" -f ($stagedBytes / 1MB))

Write-Phase 'smoke test the staged tree'
& (Join-Path $PSScriptRoot 'smoke-test.ps1') -Dir $Stage -Label 'portable'

# ---------------------------------------------------------------------------
# Portable ZIP
# ---------------------------------------------------------------------------

$zipPath = Join-Path $OutRoot ("Token-Monitor-Qt-{0}-win-x64-portable.zip" -f $Version)
Remove-Item -LiteralPath $zipPath -Force -ErrorAction SilentlyContinue
Write-Phase 'portable ZIP'
Write-Host "Compressing $zipPath"
Compress-Archive -Path (Join-Path $Stage '*') -DestinationPath $zipPath -CompressionLevel Optimal

# ---------------------------------------------------------------------------
# Installer
# ---------------------------------------------------------------------------

$installerPath = Join-Path $OutRoot ("Token-Monitor-Qt-Setup-{0}.exe" -f $Version)
if ($NoInstaller) {
    Write-Host 'Skipping the installer (-NoInstaller).'
    Remove-Item -LiteralPath $installerPath -Force -ErrorAction SilentlyContinue
} else {
    Write-Phase 'NSIS installer'
    $makensis = Find-Makensis -Explicit $MakensisPath
    if (-not $makensis) {
        Write-Host ''
        Write-Host "Portable ZIP: $zipPath"
        throw @'
makensis.exe was not found, so no installer could be built.
Install NSIS ("choco install nsis" or https://nsis.sourceforge.io) and re-run.
The portable ZIP above is complete and can be shipped as-is.
'@
    }
    Write-Host "makensis: $makensis"
    $icon = Join-Path $Root 'resources\app.ico'
    # One argument string so quoting is ours: NSIS wants /D values quoted when
    # they contain spaces, and shell-level argument splitting would eat them.
    $arguments = @(
        "/DVERSION=$Version",
        "/DVI_VERSION=$(Get-ViVersion $Version)",
        "/DSRCDIR=`"$Stage`"",
        "/DOUTFILE=`"$installerPath`""
    )
    if (Test-Path -LiteralPath $icon) { $arguments += "/DAPPICO=`"$icon`"" }
    $arguments += "`"$(Join-Path $InstallerDir 'token-monitor-qt.nsi')`""

    $proc = Start-Process -FilePath $makensis -ArgumentList ($arguments -join ' ') -PassThru -Wait
    if ($proc.ExitCode -ne 0) { throw "makensis failed with exit code $($proc.ExitCode)" }
}

# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------

Write-Phase 'report'
Write-Host 'Artifacts'
foreach ($artifact in @($installerPath, $zipPath)) {
    if (Test-Path -LiteralPath $artifact) {
        $item = Get-Item -LiteralPath $artifact
        Write-Host ("  {0}" -f $item.FullName)
        Write-Host ("    {0:N1} MB  sha256 {1}" -f ($item.Length / 1MB), (Get-Sha256 $item.FullName))
    }
}
Write-Host ("  stage (portable tree): {0}" -f $Stage)

if ($VerifyInstall) {
    Write-Phase 'install/uninstall verification'
    & (Join-Path $PSScriptRoot 'verify-install.ps1') -InstallerPath $installerPath -PortableZipPath $zipPath -Version $Version
}
