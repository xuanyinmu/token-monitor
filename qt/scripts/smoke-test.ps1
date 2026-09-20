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
$tempDir = [System.IO.Path]::GetTempPath()
$stamp = [guid]::NewGuid().ToString('N')
$png = Join-Path $tempDir "tmon-smoke-$stamp.png"
$stdout = Join-Path $tempDir "tmon-smoke-$stamp.out"
$stderr = Join-Path $tempDir "tmon-smoke-$stamp.err"

try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"

    $rendered = $false
    $notes = New-Object System.Collections.Generic.List[string]
    foreach ($backend in @('default', 'software')) {
        if ($backend -eq 'software') { $env:QT_QUICK_BACKEND = 'software' }
        else { Remove-Item Env:QT_QUICK_BACKEND -ErrorAction SilentlyContinue }
        Remove-Item -LiteralPath $png -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $stdout, $stderr -Force -ErrorAction SilentlyContinue

        $watch = [System.Diagnostics.Stopwatch]::StartNew()
        $proc = Start-Process -FilePath $exe -ArgumentList @('--screenshot', $png) -PassThru `
            -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        $exited = $proc.WaitForExit($TimeoutSeconds * 1000)
        $watch.Stop()
        $elapsed = [int]$watch.Elapsed.TotalSeconds

        if ($exited -and (Test-RenderedPng -Path $png)) {
            $bytes = [System.IO.File]::ReadAllBytes($png)
            $width = [uint32]$bytes[19] + ([uint32]$bytes[18] -shl 8) + ([uint32]$bytes[17] -shl 16) + ([uint32]$bytes[16] -shl 24)
            $height = [uint32]$bytes[23] + ([uint32]$bytes[22] -shl 8) + ([uint32]$bytes[21] -shl 16) + ([uint32]$bytes[20] -shl 24)
            Write-Host "[$Label] QML rendered ($backend backend) after ${elapsed}s: ${width}x${height}, $($bytes.Length) byte PNG"
            $rendered = $true
            break
        }

        if (-not $exited) {
            # A hang is not evidence that the deployment is broken: the default
            # backend needs a working GPU/driver path, which a CI runner may not
            # have. Kill it and let the software backend have its turn.
            $proc.Kill()
            $proc.WaitForExit(15000) | Out-Null
            $reason = "did not exit within ${TimeoutSeconds}s (killed)"
        } else {
            $proc.Refresh()
            $reason = "exited $($proc.ExitCode) without a usable PNG"
        }
        $notes.Add("$backend backend $reason after ${elapsed}s")
        Write-Warning "[$Label] $backend backend $reason after ${elapsed}s"
        # The widget's own diagnostics are the useful part of a CI failure.
        foreach ($stream in @($stdout, $stderr)) {
            if ((Test-Path -LiteralPath $stream) -and (Get-Item -LiteralPath $stream).Length -gt 0) {
                Write-Warning "[$Label] $(Split-Path -Leaf $stream) tail:"
                Get-Content -LiteralPath $stream -Tail 20 | ForEach-Object { Write-Warning "  $_" }
            }
        }
    }
    if (-not $rendered) {
        throw "[$Label] smoke test failed: the deployment cannot paint the widget out of its own directory ($($notes -join '; '))."
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
    Remove-Item -LiteralPath $stdout, $stderr -Force -ErrorAction SilentlyContinue
}
