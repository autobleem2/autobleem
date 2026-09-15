# Drives the Windows development build of autobleem-gui for smoke testing without a gamepad.
# Starts the exe on a fake usb root (layout: see CLAUDE.md "Smoke test layout"), posts key messages straight to
# its window (no focus needed), takes a full screen shot after every key, and stops it at the end.
# stdout/stderr go to <usb>\System\Logs\AB_out.txt / AB_err.txt, screenshots to the folder above <usb>.
#
# usage: powershell -ExecutionPolicy Bypass -File tools\win_drive.ps1 -Usb <fake usb root> [-Sequence "<keys>"] [-InitialWait 8]
#   Sequence: key names separated by ';' each followed by a wait in seconds, e.g. "x;3;space;6"
#   key names (see PadMapper translateKeyboardToPad): x o s t = cross circle square triangle,
#     i j k l = d-pad, space = Start, b = Select, q e 1 2 = L1 R1 L2 R2, esc = power off
param([Parameter(Mandatory=$true)][string]$Usb, [string]$Sequence = "", [int]$InitialWait = 8)
$U = $Usb
$S = Split-Path -Parent $U
$Exe = Join-Path (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)) "build_win\autobleem-gui.exe"
$env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  public static IntPtr FindByPid(uint pid) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); if (p == pid && IsWindowVisible(h)) { found = h; return false; } return true; }, IntPtr.Zero);
    return found;
  }
}
"@
# name -> virtual key, scancode
$keys = @{ x=@(0x58,0x2D); o=@(0x4F,0x18); s=@(0x53,0x1F); t=@(0x54,0x14); space=@(0x20,0x39); b=@(0x42,0x30)
           q=@(0x51,0x10); e=@(0x45,0x12); '1'=@(0x31,0x02); '2'=@(0x32,0x03); esc=@(0x1B,0x01)
           i=@(0x49,0x17); j=@(0x4A,0x24); k=@(0x4B,0x25); l=@(0x4C,0x26) }
function Press($h, $name) {
  $k = $keys[$name]; if ($null -eq $k) { "unknown key $name"; return }
  $vk = [IntPtr]$k[0]; $sc = $k[1]
  $down = [IntPtr](($sc -shl 16) -bor 1)
  $up   = [IntPtr](($sc -shl 16) -bor 1 -bor (1 -shl 30) -bor (1 -shl 31))
  [W]::PostMessage($h, 0x100, $vk, $down) | Out-Null   # WM_KEYDOWN
  Start-Sleep -Milliseconds 120
  [W]::PostMessage($h, 0x101, $vk, $up) | Out-Null     # WM_KEYUP
}
Copy-Item $Exe "$U\Autobleem\bin\autobleem\" -Force
Set-Location "$U\Autobleem\bin\autobleem"
$p = Start-Process -FilePath ".\autobleem-gui.exe" -ArgumentList "`"$U`"" -RedirectStandardOutput "$U\System\Logs\AB_out.txt" -RedirectStandardError "$U\System\Logs\AB_err.txt" -PassThru
Start-Sleep $InitialWait
$h = [W]::FindByPid($p.Id)
"window handle: $h"
$shot = 0
function Shot { $script:shot++; $vs = [System.Windows.Forms.SystemInformation]::VirtualScreen; $b = New-Object System.Drawing.Bitmap $vs.Width,$vs.Height; $g = [System.Drawing.Graphics]::FromImage($b); $g.CopyFromScreen($vs.X,$vs.Y,0,0,$b.Size); $b.Save("$S\shot$script:shot.png"); $g.Dispose(); $b.Dispose() }
Shot
if ($Sequence -ne "") {
  $parts = $Sequence.Split(';')
  for ($i = 0; $i -lt $parts.Length; $i += 2) {
    if ($p.HasExited) { break }
    Press $h $parts[$i]
    $w = 2; if ($i + 1 -lt $parts.Length) { $w = [int]$parts[$i+1] }
    Start-Sleep $w
    Shot
  }
}
$alive = -not $p.HasExited
"alive at end: $alive exit: $($p.ExitCode)"
if ($alive) { Stop-Process -Id $p.Id -Force }
