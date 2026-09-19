param(
    # Where to put tokscale.exe. Defaults to the app's userData directory, the
    # path Paths::tokscaleBinary() prefers at runtime.
    [string]$Dest,
    # Re-download even when an already-verified copy sits at $Dest.
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$ManifestPath = Join-Path $RepoRoot 'scripts\vendor\tokscale.json'
$Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
$Asset = $Manifest.platforms.'win32-x64'
$Tag = $Manifest.releaseTag
$Url = "https://github.com/$($Manifest.releaseRepo)/releases/download/$Tag/$($Asset.asset)"

if (-not $Dest) { $Dest = Join-Path (Join-Path $env:APPDATA 'Token Monitor') 'tokscale.exe' }

if ((Test-Path -LiteralPath $Dest) -and -not $Force) {
    $existing = (Get-FileHash -LiteralPath $Dest -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($existing -eq $Asset.sha256) {
        Write-Host "tokscale already present and verified: $Dest"
        return
    }
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Dest) | Out-Null
Write-Host "Downloading $Url"
Invoke-WebRequest -Uri $Url -OutFile $Dest
$Hash = (Get-FileHash -LiteralPath $Dest -Algorithm SHA256).Hash.ToLowerInvariant()
if ($Hash -ne $Asset.sha256) {
    Remove-Item -LiteralPath $Dest -Force
    throw "tokscale sha256 mismatch: got $Hash expected $($Asset.sha256)"
}
Write-Host "Installed $Dest"
