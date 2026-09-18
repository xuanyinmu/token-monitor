$ErrorActionPreference = 'Stop'

function Find-VsDevCmd {
    $candidates = @(
        'D:\App\VS2022\BuildTools\Common7\Tools\VsDevCmd.bat',
        'D:\App\VS2022\Community\Common7\Tools\VsDevCmd.bat',
        'D:\App\VS2022\Professional\Common7\Tools\VsDevCmd.bat',
        'D:\App\VS2022\Enterprise\Common7\Tools\VsDevCmd.bat'
    )
    foreach ($path in $candidates) {
        if (Test-Path -LiteralPath $path) { return $path }
    }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($install) {
            $bat = Join-Path $install 'Common7\Tools\VsDevCmd.bat'
            if (Test-Path -LiteralPath $bat) { return $bat }
        }
    }
    throw 'VsDevCmd.bat not found under D:\App\VS2022 or vswhere.'
}

$vsDevCmd = Find-VsDevCmd
$cmd = "`"$vsDevCmd`" -arch=amd64 -host_arch=amd64 && set"
$envBlock = & cmd.exe /c $cmd
foreach ($line in $envBlock) {
    if ($line -match '^(.*?)=(.*)$') {
        Set-Item -Path "Env:$($matches[1])" -Value $matches[2]
    }
}
