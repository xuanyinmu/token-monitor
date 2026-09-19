<#
.SYNOPSIS
  Smoke tests a deployed Token Monitor (Qt) tree.

.DESCRIPTION
  Proves a directory works on its own: PATH is reduced to System32 for the
  child processes, so a runtime that only resolves because the development
  machine has Qt on PATH fails here. The widget is asked for a screenshot
  (preview mode paints the whole QML tree and exits), and the two CLI binaries
  are asked for --help.

  Used by package.ps1 (the staged tree) and verify-install.ps1 (the installed
  tree and an extracted portable ZIP), so the acceptance test is identical for
  every delivery form.

.PARAMETER Dir
  Deployment root that contains TokenMonitorQt.exe.

.PARAMETER Label
  Name used in the log output, e.g. 'portable' or 'installed'.
#>
param(
    [Parameter(Mandatory = $true)][string]$Dir,
    [string]$Label = 'deployment',
    [int]$TimeoutSeconds = 90
)

$ErrorActionPreference = 'Stop'

$exe = Join-Path $Dir 'TokenMonitorQt.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "[$Label] smoke test: $exe is missing" }

# A real PNG at the widget's window size. Byte size alone is a bad proxy (the
# frame is mostly a translucent 340x650 surface, ~26 KB), so the signature and
# the IHDR dimensions are read instead - that is what proves Qt Quick actually
# built and painted the QML tree with the deployed styles.
function Test-RenderedPng {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $false }
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 8192) { return $false }
    if ((($bytes[0..7] | ForEach-Object { $_.ToString('X2') }) -join '') -ne '89504E470D0A1A0A') { return $false }
    $width = [uint32]$bytes[19] + ([uint32]$bytes[18] -shl 8) + ([uint32]$bytes[17] -shl 16) + ([uint32]$bytes[16] -shl 24)
    $height = [uint32]$bytes[23] + ([uint32]$bytes[22] -shl 8) + ([uint32]$bytes[21] -shl 16) + ([uint32]$bytes[20] -shl 24)
    return ($width -ge 200 -and $height -ge 200)
}

$savedPath = $env:PATH
$savedBackend = $env:QT_QUICK_BACKEND
$png = Join-Path ([System.IO.Path]::GetTempPath()) ("tmon-smoke-{0}.png" -f ([guid]::NewGuid().ToString('N')))

try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"

    $rendered = $false
    foreach ($backend in @('default', 'software')) {
        if ($backend -eq 'software') { $env:QT_QUICK_BACKEND = 'software' }
        else { Remove-Item Env:QT_QUICK_BACKEND -ErrorAction SilentlyContinue }
        Remove-Item -LiteralPath $png -Force -ErrorAction SilentlyContinue

        $proc = Start-Process -FilePath $exe -ArgumentList @('--screenshot', $png) -PassThru
        if (-not $proc.WaitForExit($TimeoutSeconds * 1000)) {
            $proc.Kill()
            throw "[$Label] smoke test: --screenshot did not exit within ${TimeoutSeconds}s"
        }
        if (Test-RenderedPng -Path $png) {
            $bytes = [System.IO.File]::ReadAllBytes($png)
            $width = [uint32]$bytes[19] + ([uint32]$bytes[18] -shl 8) + ([uint32]$bytes[17] -shl 16) + ([uint32]$bytes[16] -shl 24)
            $height = [uint32]$bytes[23] + ([uint32]$bytes[22] -shl 8) + ([uint32]$bytes[21] -shl 16) + ([uint32]$bytes[20] -shl 24)
            Write-Host "[$Label] QML rendered ($backend backend): ${width}x${height}, $($bytes.Length) byte PNG"
            $rendered = $true
            break
        }
        $proc.Refresh()
        Write-Warning "[$Label] --screenshot produced no usable PNG with the $backend backend (exit $($proc.ExitCode))"
    }
    if (-not $rendered) {
        throw "[$Label] smoke test failed: the deployment cannot paint the widget out of its own directory."
    }

    foreach ($cli in @('TokenMonitorHub.exe', 'TokenMonitorAgent.exe')) {
        $cliPath = Join-Path $Dir $cli
        if (-not (Test-Path -LiteralPath $cliPath)) { throw "[$Label] smoke test: $cliPath is missing" }
        & $cliPath --help | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "[$Label] smoke test: $cli --help exited $LASTEXITCODE" }
    }

    Write-Host "[$Label] smoke test passed"
} finally {
    $env:PATH = $savedPath
    if ($savedBackend) { $env:QT_QUICK_BACKEND = $savedBackend }
    else { Remove-Item Env:QT_QUICK_BACKEND -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $png -Force -ErrorAction SilentlyContinue
}
