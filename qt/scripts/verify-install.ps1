<#
.SYNOPSIS
  End-to-end verification of the Qt installer: silent install, smoke test,
  silent uninstall, residue assertions - with the machine's existing user data
  backed up and restored around the test.

.DESCRIPTION
  The point of this script is the one claim that cannot be checked by reading
  code: "install and uninstall leave nothing behind". It therefore performs a
  real install and a real uninstall, and asserts the complete footprint listed
  in qt/installer/token-monitor-qt.nsi.

  Safety, in order:

    1. It refuses to run while a Token Monitor process is alive (the uninstall
       test deletes the directory the running app holds open).
    2. It mirrors %APPDATA%\Token Monitor, %LOCALAPPDATA%\Token Monitor and
       %LOCALAPPDATA%\Javis into qt\out\backup, records a SHA256 manifest of
       every file, and saves the HKCU Run value, the uninstall registry key and
       the two shortcuts.
    3. After each uninstall pass it restores that state and re-hashes the tree;
       a mismatch is an error, not a warning. So the destructive part of the
       test cannot leave the machine in a different state than it found it.

  Passes:
    A. portable ZIP: extract, assert the ZIP root has no extra folder, smoke test
       with an isolated user data directory (TOKEN_MONITOR_USER_DATA) so it does
       not touch real data.
    B. setup /S       -> installed files, shortcuts, ARPP entry and Run value
                       exist; smoke test; uninstall /S -> everything gone.
    C. setup /S       -> uninstall /S /KEEPDATA -> shared files (and the
                       Electron-only markers) survive, Qt-owned files are gone.

.PARAMETER InstallerPath
  Token-Monitor-Qt-Setup-<version>.exe from package.ps1.

.PARAMETER PortableZipPath
  Optional portable ZIP to verify. Defaults to the ZIP next to the installer.

.PARAMETER Version
  Only used in log output.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$InstallerPath,
    [string]$PortableZipPath,
    [string]$Version = '',
    # Skip the environment preflight. Only useful to look at a partial result in
    # an environment that cannot write the user data directories / HKCU; the
    # assertions it skips are the ones that prove the uninstall contract.
    [switch]$SkipEnvironmentCheck
)

$ErrorActionPreference = 'Stop'

$ScriptDir = $PSScriptRoot
$Root = Split-Path -Parent $ScriptDir
$RepositoryRoot = Split-Path -Parent $Root
$OutRoot = Join-Path $Root 'out'
$BackupRoot = Join-Path $OutRoot 'backup'
$WorkRoot = Join-Path ([System.IO.Path]::GetTempPath()) 'token-monitor-qt-verify'
$InstallDir = Join-Path $WorkRoot 'install'
$PortableDir = Join-Path $WorkRoot 'portable'
$PortableDataDir = Join-Path $WorkRoot 'portable-userdata'

$InstallerPath = (Resolve-Path -LiteralPath $InstallerPath).Path
if (-not $PortableZipPath) {
    $candidate = Get-ChildItem -LiteralPath $OutRoot -Filter '*-portable.zip' -ErrorAction SilentlyContinue |
        Sort-Object -Property LastWriteTime -Descending | Select-Object -First 1
    if ($candidate) { $PortableZipPath = $candidate.FullName }
}

$AppDataDir = Join-Path $env:APPDATA 'Token Monitor'
$LocalDataDir = Join-Path $env:LOCALAPPDATA 'Token Monitor'
$JavisCacheDir = Join-Path $env:LOCALAPPDATA 'Javis'
$RunKeyPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
$RunValueName = 'TokenMonitor'
$ElectronRunValueName = 'com.javis.tokenmonitor'
$UninstallKeyPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\TokenMonitorQt'
$ShortcutPaths = @(
    (Join-Path ([Environment]::GetFolderPath('Programs')) 'Token Monitor (Qt).lnk'),
    (Join-Path ([Environment]::GetFolderPath('Desktop')) 'Token Monitor (Qt).lnk')
)
$DataDirs = @(
    [pscustomobject]@{ Name = 'roaming-token-monitor'; Path = $AppDataDir },
    [pscustomobject]@{ Name = 'local-token-monitor'; Path = $LocalDataDir },
    [pscustomobject]@{ Name = 'local-javis'; Path = $JavisCacheDir }
)

function Write-Step {
    param([string]$Text)
    Write-Host ''
    Write-Host "=== $Text ==="
}

function Assert-PathExists {
    param([string]$Path, [string]$What)
    if (-not (Test-Path -LiteralPath $Path)) { throw "Expected $What to exist: $Path" }
}

function Assert-PathAbsent {
    param([string]$Path, [string]$What)
    if (Test-Path -LiteralPath $Path) { throw "Uninstall left $What behind: $Path" }
}

function Assert-Gone {
    param([string]$Path, [string]$What, [int]$TimeoutSeconds = 20)
    # The NSIS uninstaller may finish its own deletion just after the process
    # exits; poll briefly instead of failing on a race.
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while (Test-Path -LiteralPath $Path) {
        if ((Get-Date) -gt $deadline) { throw "Uninstall left $What behind: $Path" }
        Start-Sleep -Milliseconds 500
    }
}

function Get-TreeManifest {
    param([string]$BasePath)
    if (-not (Test-Path -LiteralPath $BasePath)) { return @() }
    $root = (Resolve-Path -LiteralPath $BasePath).Path
    return @(Get-ChildItem -LiteralPath $root -Recurse -File -Force -ErrorAction SilentlyContinue |
        Sort-Object -Property FullName |
        ForEach-Object {
            $relative = $_.FullName.Substring($root.Length).TrimStart('\')
            "{0}  {1}" -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $relative
        })
}

function Invoke-RobocopyMirror {
    param([string]$Source, [string]$Destination)
    if (-not (Test-Path -LiteralPath $Source)) {
        if (Test-Path -LiteralPath $Destination) { Remove-Item -LiteralPath $Destination -Recurse -Force }
        return
    }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    $null = robocopy $Source $Destination /MIR /COPY:DAT /R:1 /W:1 /NFL /NDL /NJH /NJS /NP
    # robocopy: 0..7 are success codes, 8+ are failures.
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE): $Source -> $Destination" }
}

function Test-WritableDir {
    param([string]$Dir)
    # Walk up to the nearest existing ancestor: creating the directory needs the
    # same right as writing inside it, and on a clean machine the data
    # directories do not exist yet.
    $target = $Dir
    while ($target -and -not (Test-Path -LiteralPath $target)) { $target = Split-Path -Parent $target }
    if (-not $target) { return $false }
    $probe = Join-Path $target ("_tmon_probe_{0}.tmp" -f ([guid]::NewGuid().ToString('N')))
    try {
        Set-Content -LiteralPath $probe -Value 'probe' -ErrorAction Stop
        Remove-Item -LiteralPath $probe -Force -ErrorAction Stop
        return $true
    } catch {
        return $false
    }
}

# The whole point of this script is a real install/uninstall cycle, so it has to
# refuse an environment where that cycle cannot actually happen: a sandbox that
# denies writes outside its own tree would let the install succeed while every
# deletion silently fails, and the run would "prove" the opposite of the truth.
function Assert-VerificationEnvironment {
    $blocked = New-Object System.Collections.Generic.List[string]

    foreach ($dir in @($AppDataDir, $LocalDataDir, $JavisCacheDir)) {
        if (-not (Test-WritableDir $dir)) { $blocked.Add("write access to $dir (the widget's data and caches)") }
    }
    $shortcutDir = Split-Path -Parent $ShortcutPaths[0]
    if ($shortcutDir -and -not (Test-WritableDir $shortcutDir)) {
        $blocked.Add("write access to $shortcutDir (the installer creates the shortcuts there)")
    }
    try {
        New-Item -Path 'HKCU:\Software\_tmon_environment_probe' -Force -ErrorAction Stop | Out-Null
        Remove-Item 'HKCU:\Software\_tmon_environment_probe' -Recurse -Force -ErrorAction Stop
    } catch {
        $blocked.Add('write access to HKCU (autostart value and "Apps & features" entry)')
    }

    if ($blocked.Count) {
        $list = ($blocked | ForEach-Object { "  - $_" }) -join "`n"
        throw @"
This environment cannot run the install verification, so its result would be meaningless.
Missing:
$list

The verification installs the product, uninstalls it and asserts that nothing is left
behind, which needs write access to the user data directories, the Start menu and HKCU.
Run it from a normal (unsandboxed) shell or let CI run it. -SkipEnvironmentCheck runs a
partial pass, but the skipped assertions are exactly the ones that prove the contract.
"@
    }
}

function Test-TokenMonitorRunning {
    $found = New-Object System.Collections.Generic.List[string]
    foreach ($name in @('TokenMonitorQt', 'TokenMonitorHub', 'TokenMonitorAgent', 'TokenMonitor')) {
        if (Get-Process -Name $name -ErrorAction SilentlyContinue) { $found.Add($name) }
    }
    # The Electron build runs as electron.exe in development; only refuse when it
    # is actually this project's widget, since other Electron apps are unrelated.
    $electron = @(Get-CimInstance Win32_Process -Filter "Name = 'electron.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -and $_.CommandLine -match 'token-monitor' })
    if ($electron.Count) { $found.Add('electron (this project)') }
    return $found
}

function Backup-State {
    $state = [ordered]@{
        CreatedAt  = (Get-Date).ToString('o')
        Data       = @()
        RunExisted = $false
        RunValue   = $null
        ElectronRunValue = $null
        Shortcuts  = @()
        UninstallKeyExported = $false
    }

    if (Test-Path -LiteralPath $BackupRoot) { Remove-Item -LiteralPath $BackupRoot -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $BackupRoot | Out-Null

    foreach ($dir in $DataDirs) {
        $existed = Test-Path -LiteralPath $dir.Path
        $manifest = @()
        if ($existed) {
            Invoke-RobocopyMirror -Source $dir.Path -Destination (Join-Path $BackupRoot $dir.Name)
            $manifest = Get-TreeManifest -BasePath $dir.Path
        }
        $state.Data += [ordered]@{ Name = $dir.Name; Path = $dir.Path; Existed = $existed; Manifest = $manifest }
    }

    if (Test-Path -LiteralPath $RunKeyPath) {
        $properties = Get-ItemProperty -Path $RunKeyPath
        if ($properties.PSObject.Properties.Name -contains $RunValueName) {
            $state.RunExisted = $true
            $state.RunValue = $properties.$RunValueName
        }
        if ($properties.PSObject.Properties.Name -contains $ElectronRunValueName) {
            $state.ElectronRunValue = $properties.$ElectronRunValueName
        }
    }

    foreach ($shortcut in $ShortcutPaths) {
        $state.Shortcuts += [ordered]@{ Path = $shortcut; Existed = (Test-Path -LiteralPath $shortcut) }
    }

    if (Test-Path -LiteralPath $UninstallKeyPath) {
        $export = Join-Path $BackupRoot 'uninstall-key.reg'
        & reg.exe export 'HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\TokenMonitorQt' $export /y | Out-Null
        $state.UninstallKeyExported = ($LASTEXITCODE -eq 0)
    }

    $state | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $BackupRoot 'state.json') -Encoding UTF8
    return $state
}

function Restore-State {
    param($State)

    foreach ($entry in $State.Data) {
        $backup = Join-Path $BackupRoot $entry.Name
        if ($entry.Existed) {
            Invoke-RobocopyMirror -Source $backup -Destination $entry.Path
        } elseif (Test-Path -LiteralPath $entry.Path) {
            Remove-Item -LiteralPath $entry.Path -Recurse -Force
        }
    }

    if ($State.RunExisted) {
        New-Item -Path $RunKeyPath -Force | Out-Null
        Set-ItemProperty -Path $RunKeyPath -Name $RunValueName -Value $State.RunValue
    } else {
        Remove-ItemProperty -Path $RunKeyPath -Name $RunValueName -ErrorAction SilentlyContinue
    }

    foreach ($shortcut in $State.Shortcuts) {
        if ($shortcut.Existed) { continue }   # a shortcut we did not create is not ours to recreate
        Remove-Item -LiteralPath $shortcut.Path -Force -ErrorAction SilentlyContinue
    }

    $export = Join-Path $BackupRoot 'uninstall-key.reg'
    if ($State.UninstallKeyExported -and (Test-Path -LiteralPath $export)) {
        & reg.exe import $export | Out-Null
    } else {
        Remove-Item -Path $UninstallKeyPath -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Assert-ManifestsMatch {
    param($State)
    foreach ($entry in $State.Data) {
        $expected = @($entry.Manifest)
        $actual = @(Get-TreeManifest -BasePath $entry.Path)
        if ($expected.Count -ne $actual.Count) {
            throw "Restore mismatch for $($entry.Path): $($expected.Count) files before, $($actual.Count) after."
        }
        for ($i = 0; $i -lt $expected.Count; $i++) {
            if ($expected[$i] -ne $actual[$i]) {
                throw "Restore mismatch for $($entry.Path):$($expected[$i]) vs $($actual[$i])"
            }
        }
        Write-Host "  restored and re-hashed $($entry.Path) ($($actual.Count) files)"
    }
}

function Invoke-SilentExecutable {
    param([string]$FilePath, [string]$Arguments, [string]$What)
    $proc = Start-Process -FilePath $FilePath -ArgumentList $Arguments -PassThru -Wait
    if ($proc.ExitCode -ne 0) { throw "$What exited with code $($proc.ExitCode)" }
}

function Get-SavedRunValue {
    param($State)
    # The Electron build's own login item must survive every uninstall pass.
    if (-not $State.ElectronRunValue) { return }
    if (-not (Test-Path -LiteralPath $RunKeyPath)) { throw 'The uninstaller removed the whole HKCU Run key.' }
    $current = (Get-ItemProperty -Path $RunKeyPath).$ElectronRunValueName
    if ($current -ne $State.ElectronRunValue) {
        throw "Uninstall disturbed the Electron login item '$ElectronRunValueName' (was '$($State.ElectronRunValue)', now '$current')."
    }
}

# ---------------------------------------------------------------------------
# Preconditions
# ---------------------------------------------------------------------------

Write-Host ''
Write-Host "=== Token Monitor (Qt) install verification (version $Version) ==="
$running = Test-TokenMonitorRunning
if ($running.Count) {
    throw "Token Monitor processes are running ($($running -join ', ')). Close them and re-run: the uninstall pass deletes the directories they hold open."
}
if (-not $SkipEnvironmentCheck) {
    Assert-VerificationEnvironment
    Write-Host 'Environment check passed: user data, Start menu and HKCU are writable.'
}
Write-Host 'This is a self-cleaning test: it installs into a temporary directory, uninstalls again and restores the machine state. It does not install the product.'
if (-not (Test-Path -LiteralPath (Join-Path $ScriptDir 'smoke-test.ps1'))) {
    throw 'qt\scripts\smoke-test.ps1 is missing.'
}

if (Test-Path -LiteralPath $WorkRoot) { Remove-Item -LiteralPath $WorkRoot -Recurse -Force }
New-Item -ItemType Directory -Force -Path $WorkRoot | Out-Null

# ---------------------------------------------------------------------------
# A. Portable ZIP
# ---------------------------------------------------------------------------

if ($PortableZipPath -and (Test-Path -LiteralPath $PortableZipPath)) {
    Write-Step "A. Portable ZIP: $(Split-Path -Leaf $PortableZipPath)"
    Expand-Archive -LiteralPath $PortableZipPath -DestinationPath $PortableDir -Force
    # The ZIP must not carry an extra top-level folder, or "extract and run"
    # turns into "extract, find the folder, run".
    Assert-PathExists (Join-Path $PortableDir 'TokenMonitorQt.exe') 'TokenMonitorQt.exe at the ZIP root'
    Assert-PathExists (Join-Path $PortableDir 'sqldrivers\qsqlite.dll') 'the SQLite driver plugin'
    Assert-PathExists (Join-Path $PortableDir 'tokscale.exe') 'the bundled tokscale scanner'
    $savedUserData = $env:TOKEN_MONITOR_USER_DATA
    try {
        # Isolate this pass: the install/uninstall passes below own the real
        # data directories, and their restore check must not see this test.
        $env:TOKEN_MONITOR_USER_DATA = $PortableDataDir
        & (Join-Path $ScriptDir 'smoke-test.ps1') -Dir $PortableDir -Label 'portable'
    } finally {
        if ($savedUserData) { $env:TOKEN_MONITOR_USER_DATA = $savedUserData }
        else { Remove-Item Env:TOKEN_MONITOR_USER_DATA -ErrorAction SilentlyContinue }
    }
}

# ---------------------------------------------------------------------------
# Backup
# ---------------------------------------------------------------------------

Write-Step 'Backing up the machine state'
$state = Backup-State
foreach ($entry in $state.Data) {
    Write-Host ("  {0}: {1} ({2} files)" -f $entry.Path, ($(if ($entry.Existed) { 'exists' } else { 'absent' })), $entry.Manifest.Count)
}
Write-Host "  backup + SHA256 manifest: $BackupRoot"

# ---------------------------------------------------------------------------
# B. Install, smoke test, uninstall everything
# ---------------------------------------------------------------------------

Write-Step "B. Silent install into $InstallDir"
Invoke-SilentExecutable -FilePath $InstallerPath -Arguments "/S /D=$InstallDir" -What 'the installer'

Assert-PathExists (Join-Path $InstallDir 'TokenMonitorQt.exe') 'the widget'
Assert-PathExists (Join-Path $InstallDir 'TokenMonitorHub.exe') 'the hub CLI'
Assert-PathExists (Join-Path $InstallDir 'TokenMonitorAgent.exe') 'the agent CLI'
Assert-PathExists (Join-Path $InstallDir 'tokscale.exe') 'the bundled tokscale scanner'
Assert-PathExists (Join-Path $InstallDir 'msvcp140.dll') 'the app-local MSVC runtime'
Assert-PathExists (Join-Path $InstallDir 'sqldrivers\qsqlite.dll') 'the SQLite driver plugin'
Assert-PathExists (Join-Path $InstallDir 'Uninstall.exe') 'the uninstaller'
Assert-PathExists $ShortcutPaths[0] 'the Start-menu shortcut'
Assert-PathExists $UninstallKeyPath 'the "Apps & features" entry'
$runValue = (Get-ItemProperty -Path $RunKeyPath -ErrorAction SilentlyContinue).$RunValueName
# Windows needs the value quoted: the install path contains a space.
$expectedRunValue = '"' + (Join-Path $InstallDir 'TokenMonitorQt.exe') + '"'
if ($runValue -ne $expectedRunValue) {
    throw "The autostart Run value is '$runValue'; expected '$expectedRunValue' (a quoted path to the installed widget)."
}
Write-Host '  installed files, shortcuts, autostart value and uninstall entry are present'

& (Join-Path $ScriptDir 'smoke-test.ps1') -Dir $InstallDir -Label 'installed'

Write-Step 'B. Silent uninstall (/S: remove everything, shared data included)'
Invoke-SilentExecutable -FilePath (Join-Path $InstallDir 'Uninstall.exe') -Arguments '/S' -What 'the uninstaller'

Assert-Gone $InstallDir 'the install directory'
Assert-Gone $ShortcutPaths[0] 'the Start-menu shortcut'
Assert-Gone $ShortcutPaths[1] 'the desktop shortcut'
Assert-Gone $AppDataDir 'the shared user data directory (silent uninstall deletes it by contract)'
Assert-Gone $LocalDataDir 'the Qt cache directory under %LOCALAPPDATA%'
Assert-Gone $JavisCacheDir "Qt's %LOCALAPPDATA%\Javis cache directory"
Assert-Gone $UninstallKeyPath 'the "Apps & features" entry'
if ((Get-ItemProperty -Path $RunKeyPath -ErrorAction SilentlyContinue).$RunValueName) {
    throw "Uninstall left the HKCU Run value '$RunValueName' behind."
}
Get-SavedRunValue -State $state
Write-Host '  no residue: install dir, shortcuts, autostart value, uninstall entry, data and caches are gone'

Write-Step 'Restoring the machine state'
Restore-State -State $state
Assert-ManifestsMatch -State $state

# ---------------------------------------------------------------------------
# C. /KEEPDATA keeps the files shared with the Electron build
# ---------------------------------------------------------------------------

Write-Step "C. Silent install + uninstall /S /KEEPDATA"

# Make the shared-directory branch testable on a machine that has no Electron
# build: the Chromium marker is what tells the uninstaller the directory is
# shared, and the qt-* probe is a file only this build owns. Both are seeded
# after the backup, so the restore removes them again. A real marker is never
# overwritten, since its content belongs to the Electron build.
$sharedMarker = Join-Path $AppDataDir 'Local State'
New-Item -ItemType Directory -Force -Path $AppDataDir | Out-Null
if (-not (Test-Path -LiteralPath $sharedMarker)) {
    Set-Content -LiteralPath $sharedMarker -Value 'verification marker' -Encoding ascii
    Write-Host "  seeded $sharedMarker (no Electron marker was present)"
}
$qtOwnedProbe = Join-Path $AppDataDir 'qt-verify-probe.json'
Set-Content -LiteralPath $qtOwnedProbe -Value '{"probe":true}' -Encoding ascii

Invoke-SilentExecutable -FilePath $InstallerPath -Arguments "/S /D=$InstallDir" -What 'the installer'
& (Join-Path $ScriptDir 'smoke-test.ps1') -Dir $InstallDir -Label 'installed (keepdata pass)'

# Snapshot what the uninstaller is about to look at, instead of assuming what a
# profile contains. A clean machine has no settings.json at all - the widget only
# writes one once a setting changes - so "the directory existed before the run"
# says nothing about which files are in it. What is checkable is the delta: every
# file that is not Qt-owned must survive /KEEPDATA.
$beforeKeepData = @(Get-ChildItem -LiteralPath $AppDataDir -File -Force -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty Name)
$qtOwnedNames = @('limits-snapshot.json', 'tokscale.exe')
$sharedBefore = @($beforeKeepData | Where-Object { $qtOwnedNames -notcontains $_ -and $_ -notlike 'qt-*' })

Invoke-SilentExecutable -FilePath (Join-Path $InstallDir 'Uninstall.exe') -Arguments '/S /KEEPDATA' -What 'the uninstaller'

Assert-Gone $InstallDir 'the install directory (keepdata pass)'
Assert-Gone $ShortcutPaths[0] 'the Start-menu shortcut (keepdata pass)'
Assert-Gone $LocalDataDir 'the Qt cache directory (keepdata pass)'
Assert-Gone $qtOwnedProbe 'the Qt-owned qt-* scratch file (keepdata pass)'
Assert-PathExists $sharedMarker 'the shared Chromium marker kept by /KEEPDATA'
if ((Get-ItemProperty -Path $RunKeyPath -ErrorAction SilentlyContinue).$RunValueName) {
    throw "Uninstall left the HKCU Run value '$RunValueName' behind (keepdata pass)."
}
Assert-PathAbsent (Join-Path $AppDataDir 'limits-snapshot.json') 'Qt-only limits-snapshot.json'
Assert-PathAbsent (Join-Path $AppDataDir 'data') "Qt-only data\ directory"
Assert-PathAbsent (Join-Path $AppDataDir 'tokscale.exe') 'the Qt-installed tokscale.exe'
if (Test-Path -LiteralPath (Join-Path $AppDataDir 'qt-*.json')) {
    throw "Uninstall left Qt scratch files behind: $AppDataDir\qt-*.json"
}
foreach ($name in $sharedBefore) {
    Assert-PathExists (Join-Path $AppDataDir $name) "the shared file '$name' kept by /KEEPDATA"
}
Write-Host ("  /KEEPDATA kept {0} non-Qt file(s): {1}" -f $sharedBefore.Count, ($sharedBefore -join ', '))
Get-SavedRunValue -State $state
Write-Host '  /KEEPDATA removed the Qt-owned files and kept the shared ones'

Write-Step 'Restoring the machine state'
Restore-State -State $state
Assert-ManifestsMatch -State $state

Write-Host ''
Write-Host '=== Install verification passed: install, uninstall (full and /KEEPDATA) and restore are consistent ==='
Write-Host '=== Nothing is installed: the test uninstalled everything it created and restored the machine state ==='

# Tidy up what the test itself produced. The temporary tree holds an extracted
# copy of the portable ZIP and the throw-away install directory; the backup holds
# a copy of the user data (credentials.json included), so it should not outlive a
# successful run. Both are kept when anything fails, which is where the
# diagnostics are, and the step headers name the paths.
Remove-Item -LiteralPath $WorkRoot -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $BackupRoot -Recurse -Force -ErrorAction SilentlyContinue
foreach ($path in @($WorkRoot, $BackupRoot)) {
    if (Test-Path -LiteralPath $path) {
        Write-Host "Note: could not remove $path (leftovers from the test, not an installation)."
    }
}
