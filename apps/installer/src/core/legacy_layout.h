//
// LegacyLayout: an AutoBleem 1.0 / AutoBleem-NG / RetroBoot stick brought to the layout of 2026-09 in
// place, before it is updated - nothing of the user's is lost: the games, save states, memory cards,
// ROMs, playlists, RetroArch's saves and thumbnails all stay, moved where the launcher looks now.
//
//   retroarch/ at the root (RetroBoot's tree)   ->  RetroArch/bin
//   retroarch/system                            ->  RetroArch/bios
//   roms/ at the root                           ->  RetroArch/roms
//   themes/                                     ->  Themes/ (a case rename; FAT does not care, git does)
//   retroarch.cfg, every playlist               ->  the paths rewritten (/media/RetroArch/bin, /roms, /bios);
//                                                   RetroBoot's Applications.lpl removed
//   retroboot/assets/lib, lib, modules          ->  Autobleem/lib/apps, retroarch, modules (copied)
//   Apps/<name> reaching into retroarch/apps    ->  self-contained: the files moved in, the scripts pointed
//                                                   at /media/Apps/<name>, /media/System/Logs and
//                                                   Autobleem/rc/app_env.sh; Apps/retroboot removed
//
// The same steps as tools/install_autobleem.py's `layout` stage, which ran on the owner's stick first.
//
#pragma once

#include <functional>
#include <string>

class LegacyLayout {
public:
    using Say = std::function<void(const std::string &)>;

    // an old layout is there: retroarch/ at the root with retroarch.cfg or cores/ in it and no bin/,
    // or roms/ at the root
    static bool detect(const std::string &root);

    // converts in place; every step is a line to `say`. False with `error` when a move fails.
    static bool migrate(const std::string &root, const Say &say, std::string &error);

    // the path rewrites for a text file (retroarch.cfg, a playlist, an app's script); the text as it
    // should be - the same when nothing applies
    static std::string rewritePaths(const std::string &text);
    // a RetroBoot-era run.sh: the loadconfig.sh/init_libs.sh block replaced by app_env.sh
    static std::string rewriteRunScript(const std::string &text);
};
