$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$ManifestPath = Join-Path $RepoRoot 'scripts\vendor\tokscale.json'
$Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
$Asset = $Manifest.platforms.'win32-x64'
$Tag = $Manifest.releaseTag
$Url = "https://github.com/$($Manifest.releaseRepo)/releases/download/$Tag/$($Asset.asset)"
$DestDir = Join-Path $env:APPDATA 'Token Monitor'
$Dest = Join-Path $DestDir 'tokscale.exe'
New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
Write-Host "Downloading $Url"
Invoke-WebRequest -Uri $Url -OutFile $Dest
$Hash = (Get-FileHash -LiteralPath $Dest -Algorithm SHA256).Hash.ToLowerInvariant()
if ($Hash -ne $Asset.sha256) {
    Remove-Item -LiteralPath $Dest -Force
    throw "tokscale sha256 mismatch: got $Hash expected $($Asset.sha256)"
}
Write-Host "Installed $Dest"
