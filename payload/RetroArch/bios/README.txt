RetroArch's system directory (retroarch.cfg: system_directory = "/media/RetroArch/bios"): the BIOS files
the cores look for, laid out as RetroBIOS (github.com/Abdess/retrobios) does - scph5501.bin at the top,
dc/, fbneo/, PPSSPP/, ... in their folders. biospack.txt lists every file the cores on the stick can use,
with its size, SHA-256 and where the installer fetches it from (tools/biospack.py --arch psc writes it;
--check verifies this folder against it). The console copies its own PlayStation BIOS (scph5500/5501/5502)
in at the first RetroArch launch.
