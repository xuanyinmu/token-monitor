$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
if (-not ('WinCapP' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class WinCapP {
  public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr lParam);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern uint SendInput(uint n, INPUT[] i, int size);
  [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr hwnd, int attr, out RECT r, int size);
  [DllImport("user32.dll")] public static extern int GetSystemMetrics(int n);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct INPUT { public uint type; public MOUSEINPUT mi; }
  [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT {
    public int dx, dy; public uint mouseData, dwFlags, time; public IntPtr dwExtraInfo;
  }
  public static List<IntPtr> Hwnds = new List<IntPtr>();
  public static uint TargetPid;
  public static bool Collect(IntPtr hWnd, IntPtr lParam) {
    uint pid; GetWindowThreadProcessId(hWnd, out pid);
    if (pid == TargetPid && IsWindowVisible(hWnd)) Hwnds.Add(hWnd);
    return true;
  }
  public static void Click(int x, int y) {
    int sw = GetSystemMetrics(0), sh = GetSystemMetrics(1);
    if (sw < 1) sw = 1; if (sh < 1) sh = 1;
    INPUT[] a = new INPUT[3];
    int ax = x * 65535 / sw, ay = y * 65535 / sh;
    a[0].type = 0; a[0].mi.dx = ax; a[0].mi.dy = ay; a[0].mi.dwFlags = 0x8001;
    a[1].type = 0; a[1].mi.dx = ax; a[1].mi.dy = ay; a[1].mi.dwFlags = 0x8002;
    a[2].type = 0; a[2].mi.dx = ax; a[2].mi.dy = ay; a[2].mi.dwFlags = 0x8004;
    SendInput(3, a, Marshal.SizeOf(typeof(INPUT)));
  }
  public static bool RunEnum() { Hwnds.Clear(); return EnumWindows(Collect, IntPtr.Zero); }
}
'@
}

function Get-MainHwnd([uint32]$procId) {
    [WinCapP]::TargetPid = $procId
    [void][WinCapP]::RunEnum()
    foreach ($h in [WinCapP]::Hwnds) {
        $r = New-Object WinCapP+RECT
        [void][WinCapP]::GetWindowRect($h, [ref]$r)
        if (($r.Right - $r.Left) -ge 280 -and ($r.Bottom - $r.Top) -ge 400) { return $h }
    }
    return [IntPtr]::Zero
}
function Click-C([IntPtr]$hwnd, [int]$cx, [int]$cy) {
    $fr = New-Object WinCapP+RECT
    $ok = [WinCapP]::DwmGetWindowAttribute($hwnd, 9, [ref]$fr, [Runtime.InteropServices.Marshal]::SizeOf($fr))
    if ($ok -ne 0) { [void][WinCapP]::GetWindowRect($hwnd, [ref]$fr) }
    [void][WinCapP]::SetForegroundWindow($hwnd)
    Start-Sleep -Milliseconds 40
    [WinCapP]::Click(($fr.Left + $cx), ($fr.Top + $cy))
}
function Save-H([IntPtr]$hwnd, [string]$path) {
    $r = New-Object WinCapP+RECT
    [void][WinCapP]::GetWindowRect($hwnd, [ref]$r)
    $w = [Math]::Max(1, $r.Right - $r.Left)
    $hh = [Math]::Max(1, $r.Bottom - $r.Top)
    [void][WinCapP]::ShowWindow($hwnd, 9)
    [void][WinCapP]::SetForegroundWindow($hwnd)
    Start-Sleep -Milliseconds 150
    $bmp = New-Object System.Drawing.Bitmap $w, $hh
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][WinCapP]::PrintWindow($hwnd, $hdc, 2)
    $g.ReleaseHdc($hdc)
    $g.Dispose()
    $dir = Split-Path $path
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host ("saved {0} {1}x{2}" -f $path, $w, $hh)
}

$dir = 'D:\Code\Project\token-monitor\qt\build\compare'
$qt = Get-Process TokenMonitorQt | Sort-Object StartTime -Descending | Select-Object -First 1
if (-not $qt) { throw 'TokenMonitorQt not running' }
$hwnd = Get-MainHwnd ([uint32]$qt.Id)
if ($hwnd -eq [IntPtr]::Zero) { throw 'hwnd missing' }
Write-Host ("pid {0}" -f $qt.Id)

# From home: open settings (collapsed), then 常规, then language combo.
Click-C $hwnd 318 628
Start-Sleep -Milliseconds 400
Save-H $hwnd (Join-Path $dir 'qt-leftover4.png')
Click-C $hwnd 80 72
Start-Sleep -Milliseconds 350
Click-C $hwnd 270 90
Start-Sleep -Milliseconds 500
Save-H $hwnd (Join-Path $dir 'qt-lang4.png')
Click-C $hwnd 40 420
Start-Sleep -Milliseconds 200
Click-C $hwnd 80 72
Start-Sleep -Milliseconds 280
Click-C $hwnd 80 350
Start-Sleep -Milliseconds 500
Save-H $hwnd (Join-Path $dir 'qt-sync4.png')
Write-Host done
