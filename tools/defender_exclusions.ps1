# Windows Defender exclusions for working on AutoBleem: the two source trees, the MSYS2 and SysGCC
# toolchains (their compilers are what real-time scanning slows down most), Claude Code's memory and
# scratch directories, and the build tools as processes. Run once, elevated:
#   Start-Process powershell -Verb RunAs -ArgumentList '-ExecutionPolicy Bypass -File E:\Programming\autobleem-develop\tools\defender_exclusions.ps1'
# Nothing here is needed to build; it only stops Defender re-scanning every object file and header.
$paths = @(
    "E:\Programming\autobleem-develop",
    "E:\Programming\pcsx-rearmed-develop",
    "C:\msys64",
    "C:\sysGCC",
    "$env:USERPROFILE\.claude",
    "$env:LOCALAPPDATA\Temp\claude"
)
$processes = @("cc1plus.exe", "cc1.exe", "as.exe", "ld.exe", "collect2.exe", "ninja.exe", "cmake.exe",
               "clang-format.exe", "clang-tidy.exe", "autobleem-gui.exe")
foreach ($p in $paths) { Add-MpPreference -ExclusionPath $p }
foreach ($p in $processes) { Add-MpPreference -ExclusionProcess $p }
Write-Host "Excluded paths:";     (Get-MpPreference).ExclusionPath | ForEach-Object { "  $_" }
Write-Host "Excluded processes:"; (Get-MpPreference).ExclusionProcess | ForEach-Object { "  $_" }
Read-Host "Done - press Enter"
