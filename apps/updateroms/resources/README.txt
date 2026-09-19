UpdateRoms - the RetroArch library of this stick, from a PC
============================================================

The PlayStation Classic usually has no network. This program does, on the PC the stick is plugged into,
what the console's own scan would do with one:

  1. every ROM you copied into roms/<system>/ goes into that system's playlist (retroarch/playlists/),
     with the paths the console will see;
  2. each game is looked up in RetroArch's databases by the ROM's checksum and gets its proper name -
     the databases themselves are downloaded first if the stick has none (about 40 MB, once);
  3. the box art of every game that has none is fetched from libretro's thumbnail server into
     retroarch/thumbnails/<system>/Named_Boxarts/.

Run UpdateRoms.exe from this folder on the stick. It finds the stick by itself; a window shows what it is
on and a log, and says Close when it is done. Then eject the stick and boot the console: the games appear
in the RetroArch set, named, with covers. Run it again after copying more ROMs in - it only adds what is
new and fetches what is missing.

  UpdateRoms.exe E:\           the stick at E: (when run from elsewhere)
  UpdateRoms.exe E:\ --quiet   no window, the log on the console - for scripts
  UpdateRoms.exe E:\ --target rpi   a Raspberry Pi card whose kind it could not tell

It never touches your PlayStation games, their database or the themes. A playlist RetroArch itself wrote
(with its own scanner on the console) is kept and added to, never replaced. A cover the server does not
have is noted in Named_Boxarts/.autobleem-missing.txt and not asked for again; delete that file to retry.

Requires Windows 10 or later (curl.exe, which every Windows has since 2018, does the downloading); the
program itself is one file with nothing to install. The same folder works for a Raspberry Pi's SD card
in a card reader.
