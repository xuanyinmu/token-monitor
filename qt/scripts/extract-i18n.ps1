$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent (Split-Path -Parent $scriptDir)
$path = Join-Path $repoRoot 'src\electron\renderer\i18n.js'
$outDir = Join-Path $scriptDir '..\resources'
$out = Join-Path $outDir 'i18n.json'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$lines = Get-Content -LiteralPath $path -Encoding UTF8
$messages = [ordered]@{}
$lang = $null
$current = $null
$inMessages = $false

function Unquote([string]$raw) {
    $raw = $raw.Trim().TrimEnd(',')
    if ($raw.Length -ge 2 -and $raw.StartsWith("'") -and $raw.EndsWith("'")) {
        return $raw.Substring(1, $raw.Length - 2).Replace("\'", "'").Replace('\\', '\')
    }
    if ($raw.Length -ge 2 -and $raw.StartsWith('"') -and $raw.EndsWith('"')) {
        return [System.Text.RegularExpressions.Regex]::Unescape($raw.Substring(1, $raw.Length - 2))
    }
    return $raw
}

foreach ($line in $lines) {
    if (-not $inMessages) {
        if ($line -match 'const MESSAGES = \{') { $inMessages = $true }
        continue
    }
    if ($line -match "^\s+'?([A-Za-z][A-Za-z0-9-]*)'?:\s*\{") {
        if ($lang -and $current) { $messages[$lang] = $current }
        $lang = $Matches[1]
        $current = [ordered]@{}
        continue
    }
    if ($line -match "^\s+\};\s*$") {
        if ($lang -and $current) { $messages[$lang] = $current }
        break
    }
    if ($line -match "^\s+'([^']+)':\s*(.+)$") {
        $current[$Matches[1]] = Unquote $Matches[2]
    } elseif ($line -match '^\s+"([^"]+)":\s*(.+)$') {
        $current[$Matches[1]] = Unquote $Matches[2]
    }
}

$payload = [ordered]@{ languages = @($messages.Keys); messages = $messages }
$json = $payload | ConvertTo-Json -Depth 6 -Compress
[System.IO.File]::WriteAllText($out, $json, [System.Text.UTF8Encoding]::new($false))
Write-Host "wrote $out ($($messages.Keys.Count) locales)"
