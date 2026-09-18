$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
if (-not ('WinCap' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public class WinCap {
  public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr lParam);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hWnd, StringBuilder sb, int max);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern uint SendInput(uint n, INPUT[] i, int size);
  [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr hwnd, int attr, out RECT r, int size);
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
  [DllImport("user32.dll")] public static extern int GetSystemMetrics(int n);
  public static void Click(int x, int y) {
    int sw = GetSystemMetrics(0), sh = GetSystemMetrics(1);
    if (sw < 1) sw = 1; if (sh < 1) sh = 1;
    INPUT[] a = new INPUT[3];
    int ax = x * 65535 / sw, ay = y * 65535 / sh;
    a[0].type = 0; a[0].mi.dx = ax; a[0].mi.dy = ay; a[0].mi.dwFlags = 0x8001;
    a[1].type = 0; a[1].mi.dx = ax; a[1].mi.dy = ay; a[1].mi.dwFlags = 0x8002;
    a[2].type = 0; a[2].mi.dx = ax; a[2].mi.dy = ay; a[2].mi.dwFlags = 0x8004;
    SendInput(3, a, System.Runtime.InteropServices.Marshal.SizeOf(typeof(INPUT)));
  }
  public static bool RunEnum() {
    Hwnds.Clear();
    return EnumWindows(Collect, IntPtr.Zero);
  }
}
'@
}
[WinCap]::SetProcessDPIAware() | Out-Null

function Get-PidWindows([uint32]$procId) {
    [WinCap]::TargetPid = $procId
    [void][WinCap]::RunEnum()
    return @([WinCap]::Hwnds)
}
function Get-FrameRect([IntPtr]$hwnd) {
    $r = New-Object WinCap+RECT
    $ok = [WinCap]::DwmGetWindowAttribute($hwnd, 9, [ref]$r, [System.Runtime.InteropServices.Marshal]::SizeOf($r))
    if ($ok -ne 0) { [void][WinCap]::GetWindowRect($hwnd, [ref]$r) }
    return $r
}
function Get-MainHwnd([uint32]$procId) {
    foreach ($h in (Get-PidWindows $procId)) {
        $r = New-Object WinCap+RECT
        [void][WinCap]::GetWindowRect($h, [ref]$r)
        if (($r.Right - $r.Left) -ge 280 -and ($r.Bottom - $r.Top) -ge 400) { return $h }
    }
    return [IntPtr]::Zero
}
function Save-Hwnd([IntPtr]$hwnd, [string]$path) {
    $r = New-Object WinCap+RECT
    [void][WinCap]::GetWindowRect($hwnd, [ref]$r)
    $w = [Math]::Max(1, $r.Right - $r.Left)
    $h = [Math]::Max(1, $r.Bottom - $r.Top)
    [void][WinCap]::ShowWindow($hwnd, 9)
    [void][WinCap]::SetForegroundWindow($hwnd)
    Start-Sleep -Milliseconds 200
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][WinCap]::PrintWindow($hwnd, $hdc, 2)
    $g.ReleaseHdc($hdc)
    $g.Dispose()
    $dir = Split-Path $path
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    $fr = Get-FrameRect $hwnd
    $sw = [Math]::Max(1, $fr.Right - $fr.Left)
    $sh = [Math]::Max(1, $fr.Bottom - $fr.Top)
    $screen = New-Object System.Drawing.Bitmap $sw, $sh
    $sg = [System.Drawing.Graphics]::FromImage($screen)
    $sg.CopyFromScreen($fr.Left, $fr.Top, 0, 0, (New-Object System.Drawing.Size $sw, $sh))
    $sg.Dispose()
    $screenPath = $path -replace '\.png$', '-screen.png'
    $screen.Save($screenPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $screen.Dispose()
    Write-Host ("saved {0} {1}x{2}" -f $path, $w, $h)
}
function Click-Client([IntPtr]$hwnd, [int]$cx, [int]$cy) {
    $fr = Get-FrameRect $hwnd
    [void][WinCap]::SetForegroundWindow($hwnd)
    Start-Sleep -Milliseconds 80
    [WinCap]::Click(($fr.Left + $cx), ($fr.Top + $cy))
}

$dir = 'D:\Code\Project\token-monitor\qt\build\compare'
New-Item -ItemType Directory -Path $dir -Force | Out-Null

$qtProc = Get-Process TokenMonitorQt -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $qtProc) { throw 'TokenMonitorQt not running' }
$qtHwnd = Get-MainHwnd ([uint32]$qtProc.Id)
if ($qtHwnd -eq [IntPtr]::Zero) { throw 'Qt hwnd missing' }
$fr = Get-FrameRect $qtHwnd
$w = $fr.Right - $fr.Left
$h = $fr.Bottom - $fr.Top
Write-Host ("Qt pid {0} frame {1}x{2}" -f $qtProc.Id, $w, $h)

Save-Hwnd $qtHwnd (Join-Path $dir 'qt-chrome-home.png')
# MONTH center of the middle period tab (not TOTAL — that x can land on Close)
Click-Client $qtHwnd ([int]($w - 14 - 148 + 49 + 24)) 27
Start-Sleep -Milliseconds 400
Save-Hwnd $qtHwnd (Join-Path $dir 'qt-chrome-month.png')
# Settings gear (do not click TOTAL: that x can land on Close if actions overlay is up)
Click-Client $qtHwnd ($w - 22) ($h - 22)
Start-Sleep -Milliseconds 600
Save-Hwnd $qtHwnd (Join-Path $dir 'qt-chrome-settings.png')
# expand 主画面 (2nd accordion)
Click-Client $qtHwnd 80 110
Start-Sleep -Milliseconds 500
Save-Hwnd $qtHwnd (Join-Path $dir 'qt-chrome-settings-main.png')
# collapse 主画面 then open 外观 (4th header when collapsed)
Click-Client $qtHwnd 80 110
Start-Sleep -Milliseconds 350
Click-Client $qtHwnd 80 190
Start-Sleep -Milliseconds 800
Save-Hwnd $qtHwnd (Join-Path $dir 'qt-chrome-settings-appearance.png')
# 常规 (1st header)
Click-Client $qtHwnd 80 70
Start-Sleep -Milliseconds 400
Save-Hwnd $qtHwnd (Join-Path $dir 'qt-chrome-settings-general.png')

$eProc = Get-Process | Where-Object { $_.ProcessName -eq 'Token Monitor' } | Select-Object -First 1
if (-not $eProc) { Write-Host 'Electron not found'; return }
$eHwnd = Get-MainHwnd ([uint32]$eProc.Id)
if ($eHwnd -eq [IntPtr]::Zero) { Write-Host 'Electron hwnd missing'; return }
if (-not ('WinPos' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public class WinPos {
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
'@
}
$wr = New-Object WinPos+RECT
$cr = New-Object WinPos+RECT
[void][WinPos]::GetWindowRect($eHwnd, [ref]$wr)
[void][WinPos]::GetClientRect($eHwnd, [ref]$cr)
$extraW = ($wr.Right - $wr.Left) - ($cr.Right - $cr.Left)
$extraH = ($wr.Bottom - $wr.Top) - ($cr.Bottom - $cr.Top)
[void][WinPos]::SetWindowPos($eHwnd, [IntPtr]::Zero, $wr.Left, $wr.Top, 340 + $extraW, 650 + $extraH, 0x0004)
Start-Sleep -Milliseconds 400
$er = Get-FrameRect $eHwnd
$ew = $er.Right - $er.Left
$eh = $er.Bottom - $er.Top
Write-Host ("Electron pid {0} frame {1}x{2} extra {3}x{4}" -f $eProc.Id, $ew, $eh, $extraW, $extraH)
Save-Hwnd $eHwnd (Join-Path $dir 'el-chrome-current.png')
# Gear in the 14px padded footer (client-ish: 14px inset)
Click-Client $eHwnd ($ew - 28) ($eh - 28)
Start-Sleep -Milliseconds 700
Save-Hwnd $eHwnd (Join-Path $dir 'el-chrome-settings.png')
Click-Client $eHwnd 90 118
Start-Sleep -Milliseconds 600
Save-Hwnd $eHwnd (Join-Path $dir 'el-chrome-settings-main.png')
Write-Host 'done'
