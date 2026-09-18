$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
if (-not ([System.Management.Automation.PSTypeName]'NativeWin2').Type) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public class NativeWin2 {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdcBlt, uint nFlags);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hWnd, StringBuilder sb, int max);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc lpEnumFunc, IntPtr lParam);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
  public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
  public static List<ulong> Hwnds = new List<ulong>();
  public static uint TargetPid;
  public static bool Collect(IntPtr hWnd, IntPtr lParam) {
    uint pid;
    GetWindowThreadProcessId(hWnd, out pid);
    if (pid == TargetPid) Hwnds.Add((ulong)hWnd);
    return true;
  }
}
'@
}

$pids = @(Get-Process | Where-Object { $_.ProcessName -match 'electron' } | Select-Object -ExpandProperty Id)
Write-Host ("electron pids: {0}" -f ($pids -join ','))
if (-not $pids) { throw 'electron is not running' }

$dir = 'D:\Code\Project\token-monitor\qt\build\compare'
$idx = 0
foreach ($pid in $pids) {
    [NativeWin2]::Hwnds.Clear()
    [NativeWin2]::TargetPid = [uint32]$pid
    [void][NativeWin2]::EnumWindows([NativeWin2+EnumProc][NativeWin2]::Collect, [IntPtr]::Zero)
    foreach ($raw in [NativeWin2]::Hwnds) {
        $hwnd = [IntPtr]$raw
        $sb = New-Object System.Text.StringBuilder 512
        [void][NativeWin2]::GetWindowText($hwnd, $sb, 512)
        $title = $sb.ToString()
        $rect = New-Object NativeWin2+RECT
        [void][NativeWin2]::GetWindowRect($hwnd, [ref]$rect)
        $width = $rect.Right - $rect.Left
        $height = $rect.Bottom - $rect.Top
        $vis = [NativeWin2]::IsWindowVisible($hwnd)
        Write-Host ("pid {0} hwnd {1} vis={2} '{3}' {4}x{5}" -f $pid, $hwnd, $vis, $title, $width, $height)
        if ($width -lt 280 -or $height -lt 400) { continue }
        [void][NativeWin2]::ShowWindow($hwnd, 9)
        [void][NativeWin2]::SetForegroundWindow($hwnd)
        Start-Sleep -Milliseconds 400
        $bmp = New-Object System.Drawing.Bitmap $width, $height
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $hdc = $g.GetHdc()
        [void][NativeWin2]::PrintWindow($hwnd, $hdc, 2)
        $g.ReleaseHdc($hdc)
        $g.Dispose()
        $name = if ($title) { $title } else { 'hidden' }
        $safe = ($name -replace '[^a-zA-Z0-9]+','-').Trim('-')
        $path = Join-Path $dir ("electron-{0}-{1}.png" -f $idx, $safe)
        $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        Write-Host "saved $path $((Get-Item $path).Length)"
        $idx++
    }
}
if ($idx -eq 0) { throw 'no Token Monitor-sized window captured' }
