# Drives the Windows development build of autobleem-gui for smoke testing without a gamepad.
# Starts the exe on a fake usb root (layout: see CLAUDE.md "Smoke test layout"), posts key messages straight to
# its window (no focus needed), takes a full screen shot after every key, and stops it at the end.
# stdout/stderr go to <usb>\System\Logs\AB_out.txt / AB_err.txt, screenshots to the folder above <usb>.
#
# usage: powershell -ExecutionPolicy Bypass -File tools\win_drive.ps1 -Usb <fake usb root> [-Sequence "<keys>"] [-InitialWait 8]
#   Sequence: key names separated by ';' each followed by a wait in seconds, e.g. "x;3;space;6"
#     "q+e" holds the first key while tapping the last (L2 + R2 opens the launcher's system menu - Re-Scan,
#     RetroArch, Memory Cards, Game Manager, Power Off among its items; a bare R2 does nothing)
#   key names (see PadMapper translateKeyboardToPad): x o s t = cross circle square triangle,
#     i j k l = d-pad, space = Start, b = Select, q e 1 2 = L1 R1 L2 R2, esc = power off
#   -Tool <name>: drive a console tool from usb\Apps\<name>\<name>.exe instead (staged by make_usb.py);
#     its stdout/stderr go to <usb>\System\Logs\<name>_out.txt / <name>_err.txt
param([Parameter(Mandatory=$true)][string]$Usb, [string]$Sequence = "", [int]$InitialWait = 8, [string]$Tool = "")
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
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, System.Text.StringBuilder s, int n);
  public static string Title(IntPtr h) { var sb = new System.Text.StringBuilder(256); GetWindowTextW(h, sb, 256); return sb.ToString(); }
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, System.Text.StringBuilder s, int n);
  public static string ClassName(IntPtr h) { var sb = new System.Text.StringBuilder(256); GetClassNameW(h, sb, 256); return sb.ToString(); }
  // the process's SDL window ("SDL_app"), not its console - keys posted to the console go nowhere
  public static IntPtr FindByPid(uint pid) {
    IntPtr found = IntPtr.Zero, any = IntPtr.Zero;
    EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); if (p == pid) { if (ClassName(h) == "SDL_app") { found = h; return false; } if (IsWindowVisible(h)) any = h; } return true; }, IntPtr.Zero);
    return found != IntPtr.Zero ? found : any;
  }
}
"@
# name -> virtual key, scancode
$keys = @{ x=@(0x58,0x2D); o=@(0x4F,0x18); s=@(0x53,0x1F); t=@(0x54,0x14); space=@(0x20,0x39); b=@(0x42,0x30)
           q=@(0x51,0x10); e=@(0x45,0x12); '1'=@(0x31,0x02); '2'=@(0x32,0x03); esc=@(0x1B,0x01)
           i=@(0x49,0x17); j=@(0x4A,0x24); k=@(0x4B,0x25); l=@(0x4C,0x26) }
function KeyDown($h, $name) {
  $k = $keys[$name]; if ($null -eq $k) { "unknown key $name"; return }
  [W]::PostMessage($h, 0x100, [IntPtr]$k[0], [IntPtr](($k[1] -shl 16) -bor 1)) | Out-Null   # WM_KEYDOWN
}
function KeyUp($h, $name) {
  $k = $keys[$name]; if ($null -eq $k) { return }
  [W]::PostMessage($h, 0x101, [IntPtr]$k[0], [IntPtr](($k[1] -shl 16) -bor 1 -bor (1 -shl 30) -bor (1 -shl 31))) | Out-Null   # WM_KEYUP
}
# "q+o": hold every key but the last (L1 is a modifier in the classic menu), tap the last, release
function Press($h, $chord) {
  $names = $chord.Split('+')
  $held = @(); if ($names.Length -gt 1) { $held = $names[0..($names.Length - 2)] }
  foreach ($n in $held) { KeyDown $h $n; Start-Sleep -Milliseconds 150 }
  KeyDown $h $names[-1]
  Start-Sleep -Milliseconds 120
  KeyUp $h $names[-1]
  [array]::Reverse($held)
  foreach ($n in $held) { Start-Sleep -Milliseconds 150; KeyUp $h $n }
}
if ($Tool -ne "") {
  Set-Location "$U\Apps\$Tool"
  $p = Start-Process -FilePath ".\$Tool.exe" -ArgumentList "`"$U`"" -RedirectStandardOutput "$U\System\Logs\${Tool}_out.txt" -RedirectStandardError "$U\System\Logs\${Tool}_err.txt" -PassThru
} else {
  Copy-Item $Exe "$U\Autobleem\bin\autobleem\" -Force
  Set-Location "$U\Autobleem\bin\autobleem"
  # (not -WindowStyle Hidden: SDL's window would start hidden too; the console it gets is a nuisance on the
  # screenshots only - AB_SHOT=<file%d.bmp> in the environment has the launcher save its own frames instead)
  $p = Start-Process -FilePath ".\autobleem-gui.exe" -ArgumentList "`"$U`"" -RedirectStandardOutput "$U\System\Logs\AB_out.txt" -RedirectStandardError "$U\System\Logs\AB_err.txt" -PassThru
}
Start-Sleep $InitialWait
$h = [W]::FindByPid($p.Id)
for ($try = 0; $try -lt 20 -and [W]::ClassName($h) -ne "SDL_app"; $try++) { Start-Sleep -Milliseconds 500; $h = [W]::FindByPid($p.Id) }
"window handle: $h ($([W]::ClassName($h)) '$([W]::Title($h))')"
# SDL only turns posted key messages into events for the window that has the keyboard focus
[W]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 300
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
