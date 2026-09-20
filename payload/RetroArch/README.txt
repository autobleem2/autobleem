RetroArch on the AutoBleem stick
================================

Everything of RetroArch's lives under this folder:

  bin/    RetroArch's own tree - the retroarch binary (AutoBleem's build, github.com/autobleem/retroarch-psc),
          cores/ and info/, assets/, autoconfig/, database/, config/, playlists/, saves/, savestates/,
          thumbnails/, retroarch.cfg. What the download repository's psc/retroarch, psc/cores and the
          libretro bundles provide; the installer lays it out.
  bios/   RetroArch's system directory (retroarch.cfg: system_directory) - the BIOS files the cores look
          for. biospack.txt in there lists what the cores on this stick can use and where the installer
          fetches it from; the console copies its own PlayStation BIOS in at the first RetroArch launch.
  roms/   The other systems' games, one folder per system named as RetroArch's databases are
          ("Nintendo - Nintendo Entertainment System", "Sega - Mega Drive - Genesis", ...). AutoBleem's
          scan (or UpdateRoms.exe from a PC) writes a playlist for each into bin/playlists.

The launch scripts are AutoBleem's own (Autobleem/rc/launch_rb.sh, retroarch.sh); RetroBoot is not needed.
