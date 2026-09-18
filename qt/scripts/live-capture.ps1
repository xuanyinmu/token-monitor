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
  public const int DWMWA_EXTENDED_FRAME_BOUNDS = 9;
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

function Save-Hwnd([IntPtr]$hwnd, [string]$path) {
    $r = New-Object WinCap+RECT
    [void][WinCap]::GetWindowRect($hwnd, [ref]$r)
    $w = [Math]::Max(1, $r.Right - $r.Left)
    $h = [Math]::Max(1, $r.Bottom - $r.Top)
    [void][WinCap]::ShowWindow($hwnd, 9)
    [void][WinCap]::SetForegroundWindow($hwnd)
    Start-Sleep -Milliseconds 250
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
    Write-Host ("saved {0} {1}x{2} {3}" -f $path, $w, $h, (Get-Item $path).Length)
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
    Write-Host ("saved {0} {1}x{2}" -f $screenPath, $sw, $sh)
}

function Get-MainHwnd([uint32]$procId, [int]$minW = 280, [int]$minH = 400) {
    foreach ($h in (Get-PidWindows $procId)) {
        $r = New-Object WinCap+RECT
        [void][WinCap]::GetWindowRect($h, [ref]$r)
        $w = $r.Right - $r.Left
        $hgt = $r.Bottom - $r.Top
        $sb = New-Object System.Text.StringBuilder 256
        [void][WinCap]::GetWindowText($h, $sb, 256)
        Write-Host ("  hwnd {0} '{1}' {2}x{3}" -f $h, $sb.ToString(), $w, $hgt)
        if ($w -ge $minW -and $hgt -ge $minH) { return $h }
    }
    return [IntPtr]::Zero
}

function Click-Client([IntPtr]$hwnd, [int]$cx, [int]$cy) {
    $fr = Get-FrameRect $hwnd
    $x = $fr.Left + $cx
    $y = $fr.Top + $cy
    [void][WinCap]::SetForegroundWindow($hwnd)
    Start-Sleep -Milliseconds 80
    [WinCap]::Click($x, $y)
}

$dir = 'D:\Code\Project\token-monitor\qt\build\compare'
New-Item -ItemType Directory -Path $dir -Force | Out-Null

$qtExe = 'D:\Code\Project\token-monitor\qt\build\TokenMonitorQt.exe'
$qt = Start-Process -FilePath $qtExe -ArgumentList '--view','home','--open-dashboard' -PassThru -WorkingDirectory (Split-Path $qtExe)
Write-Host "started Qt pid $($qt.Id)"
Start-Sleep -Seconds 18

$qtHwnd = Get-MainHwnd ([uint32]$qt.Id) 300 500
if ($qtHwnd -eq [IntPtr]::Zero) { throw 'Qt window not found' }
Save-Hwnd $qtHwnd (Join-Path $dir 'qt-live-home.png')

# Footer current button ~75, height-15. Cycle: home->tool->status->device->model->project->session->limits->trends
$names = @('tool','status','device','model','project','session','limits','trends')
foreach ($name in $names) {
    $r = New-Object WinCap+RECT
    [void][WinCap]::GetWindowRect($qtHwnd, [ref]$r)
    $hgt = $r.Bottom - $r.Top
    Click-Client $qtHwnd 70 ($hgt - 16)
    Start-Sleep -Milliseconds 700
    Save-Hwnd $qtHwnd (Join-Path $dir ("qt-live-{0}.png" -f $name))
}

# Settings gear ~ width-18, height-16
$r = New-Object WinCap+RECT
[void][WinCap]::GetWindowRect($qtHwnd, [ref]$r)
Click-Client $qtHwnd (($r.Right - $r.Left) - 18) (($r.Bottom - $r.Top) - 16)
Start-Sleep -Milliseconds 800
Save-Hwnd $qtHwnd (Join-Path $dir 'qt-live-settings.png')

# Close settings, go to trends ↗ (already on trends before settings). Re-open trends via cycle if needed.
Click-Client $qtHwnd (($r.Right - $r.Left) - 18) (($r.Bottom - $r.Top) - 16)
Start-Sleep -Milliseconds 400
# ↗ is top-right of trends content: below title+total ~ 30+70+20 = 120, x = width-22
Click-Client $qtHwnd (($r.Right - $r.Left) - 22) 128
Start-Sleep -Milliseconds 1200
$dashHwnd = [IntPtr]::Zero
foreach ($h in (Get-PidWindows ([uint32]$qt.Id))) {
    $rr = New-Object WinCap+RECT
    [void][WinCap]::GetWindowRect($h, [ref]$rr)
    $w = $rr.Right - $rr.Left; $hh = $rr.Bottom - $rr.Top
    if ($w -ge 500 -and $hh -ge 400) { $dashHwnd = $h }
}
if ($dashHwnd -ne [IntPtr]::Zero) {
    Save-Hwnd $dashHwnd (Join-Path $dir 'qt-live-dashboard.png')
} else {
    Write-Host 'Qt dashboard window not found'
}

$eProc = Get-Process | Where-Object {
    $_.ProcessName -match 'Token Monitor' -or
    ($_.Path -and $_.Path -match 'Token-Monitor|Token Monitor.exe')
} | Select-Object -First 1
if (-not $eProc) {
    Write-Host 'Electron process not found'
    Get-Process | Where-Object { $_.MainWindowTitle -match 'Token Monitor' } | Format-Table Id,ProcessName,MainWindowTitle -AutoSize
    return
}
$ePid = [int]$eProc.Id
Write-Host "Electron pid $ePid name=$($eProc.ProcessName)"
$eHwnd = Get-MainHwnd ([uint32]$ePid) 300 500
if ($eHwnd -eq [IntPtr]::Zero) { Write-Host 'Electron main window not found'; return }
Save-Hwnd $eHwnd (Join-Path $dir 'electron-live-current.png')

# Cycle Electron footer current a few times to capture settings via gear
$er = Get-FrameRect $eHwnd
$ew = $er.Right - $er.Left
$eh = $er.Bottom - $er.Top
# Client is inset; click current at ~70 from left, ~16 from bottom of FRAME
Click-Client $eHwnd 80 ($eh - 28)
Start-Sleep -Milliseconds 600
Save-Hwnd $eHwnd (Join-Path $dir 'electron-live-cycle1.png')
Click-Client $eHwnd 80 ($eh - 28)
Start-Sleep -Milliseconds 600
Save-Hwnd $eHwnd (Join-Path $dir 'electron-live-cycle2.png')
Click-Client $eHwnd 80 ($eh - 28)
Start-Sleep -Milliseconds 600
Save-Hwnd $eHwnd (Join-Path $dir 'electron-live-cycle3.png')
Click-Client $eHwnd ($ew - 28) ($eh - 28)
Start-Sleep -Milliseconds 800
Save-Hwnd $eHwnd (Join-Path $dir 'electron-live-settings.png')

Write-Host 'done'
Get-ChildItem $dir -Filter '*.png' | Sort-Object LastWriteTime | Select-Object Name,Length,LastWriteTime
