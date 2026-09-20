AutoBleem 2 - the installer for the PlayStation Classic
=======================================================

What it does: puts AutoBleem onto a USB stick from this PC, and later updates it.

  1. Plug a USB stick into the PC. Pick it in the box at the top (only removable drives are offered).
     The console needs a FAT32 stick (exFAT works with the AutoBleem kernel) - "Format..." makes one,
     erasing everything on it. Windows formats FAT32 up to 32 GB; for a bigger stick put fat32format.exe
     (Ridgecrop's free tool) next to this program, or use exFAT.
  2. Choose: the cover databases (Japan, USA, PAL - all three by default), RetroArch for the other
     systems' games (with its cores, apps and libraries; off by default), the BIOS files those cores
     need (needs RetroArch; about 300 MB, fetched from RetroBIOS), the sample games.
  3. Install. Then put the stick into the console's second controller port and boot it.

Run it again over a stick that has AutoBleem to update it: the launcher, the scripts, the themes and
the console tools are replaced; your games, save states, memory cards, settings, RetroArch's saves and
playlists, and anything else on the stick stay as they are.

autobleem-psc-<version>.tar.gz next to this program is the stick's file system - keep the two together.
Everything else comes from https://autobleem.retromenele.pl when asked for, so an install without
RetroArch, BIOS files or samples needs the network only for the cover databases (or not at all with
none selected).

For scripts: AutoBleemInstaller.exe --quiet --drive F: [--covers JUP] [--retroarch] [--bios] [--samples]
