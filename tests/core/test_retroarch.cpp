//
// RetroArchService: RetroArch's playlists as sets of games, and which core plays each entry.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/string_maker.h"
#include "../support/temp_dir.h"

#include "core/services/environment.h"
#include "core/services/retroarch.h"

#include <string>
#include <vector>

using std::string;
using std::vector;

namespace {

// A USB root with a RetroArch install on it: two cores with their .info files, a few ROMs, and whatever
// playlists a test writes. Playlist entries name /media paths the way RetroArch writes them; the service
// maps them onto the USB root, which here is the temp tree.
struct RetroArchTree {
    RetroArchTree() : tmp("retroarch") {
        env.setUsbRoot(tmp.path());
        env.setWorkingPath(tmp.path()); // where platform/<platform>.cores.cfg is looked for

        addCore("snes9x_libretro", "Nintendo - SNES (Snes9x)", "sfc|smc",
                "Nintendo - Super Nintendo Entertainment System");
        addCore("bsnes_libretro", "Nintendo - SNES (bsnes)", "sfc", "Nintendo - Super Nintendo Entertainment System");
        addCore("stella_libretro", "Atari - 2600 (Stella)", "a26|bin", "Atari - 2600");

        tmp.writeFile("roms/snes/Chrono Trigger.sfc", "rom");
        tmp.writeFile("roms/snes/Earthbound.sfc", "rom");
        tmp.writeFile("roms/atari/Pitfall.a26", "rom");
        tmp.writeFile("roms/snes/pack.zip", "archive");
    }

    void addCore(const string &file, const string &displayName, const string &extensions, const string &database) {
        tmp.writeFile("retroarch/info/" + file + ".info", "display_name = \"" + displayName +
                                                              "\"\n"
                                                              "supported_extensions = \"" +
                                                              extensions +
                                                              "\"\n"
                                                              "database = \"" +
                                                              database + "\"\n");
        tmp.writeFile("retroarch/cores/" + file + ".so", "core");
    }

    struct Entry {
        string path, label, core_path, core_name, db_name;
    };

    static string json(const vector<Entry> &entries) {
        string out = "{\n  \"version\": \"1.0\",\n  \"items\": [\n";
        for (size_t i = 0; i < entries.size(); i++) {
            const Entry &e = entries[i];
            out += "    {\n      \"path\": \"" + e.path + "\",\n      \"label\": \"" + e.label +
                   "\",\n"
                   "      \"core_path\": \"" +
                   e.core_path + "\",\n      \"core_name\": \"" + e.core_name +
                   "\",\n"
                   "      \"crc32\": \"00000000|crc\",\n      \"db_name\": \"" +
                   e.db_name + "\"\n    }";
            out += (i + 1 < entries.size()) ? ",\n" : "\n";
        }
        return out + "  ]\n}\n";
    }

    static string sixLine(const vector<Entry> &entries) {
        string out;
        for (const Entry &e : entries) {
            out += e.path + "\n" + e.label + "\n" + e.core_path + "\n" + e.core_name + "\n00000000|crc\n" + e.db_name +
                   "\n";
        }
        return out;
    }

    void writePlaylist(const string &name, const vector<Entry> &entries, bool asJson = true) {
        tmp.writeFile("retroarch/playlists/" + name + ".lpl", asJson ? json(entries) : sixLine(entries));
    }

    static Entry snes(const string &rom, const string &label, const string &core = "DETECT") {
        return Entry{"/media/roms/snes/" + rom, label, core, core == "DETECT" ? "DETECT" : "Nintendo - SNES (Snes9x)",
                     "Nintendo - Super Nintendo Entertainment System.lpl"};
    }

    // what RetroArch itself writes into content_favorites.lpl / content_history.lpl: the core it used, but
    // no db_name, and for history no label either
    static Entry byRetroArch(const string &rom, const string &label) {
        return Entry{"/media/roms/snes/" + rom, label, "/media/retroarch/cores/snes9x_libretro.so",
                     "Nintendo - SNES (Snes9x)", ""};
    }

    string core(const string &file) const { return tmp.at("retroarch/cores/" + file + ".so"); }
    // this host's platform file, as RetroArchService looks for it
    static string coresCfg() { return string("platform/") + Env::platformName() + ".cores.cfg"; }

    EnvFixture env;
    TempDir tmp;
    RetroArchService service;
};

vector<string> titles(const PsGames &games) {
    vector<string> out;
    for (const PsGamePtr &g : games)
        out.push_back(g->title);
    return out;
}

} // namespace

TEST_CASE("without a RetroArch install there are no playlists, and asking is harmless") {
    EnvFixture env;
    TempDir tmp("noretroarch");
    env.setUsbRoot(tmp.path());
    RetroArchService service;

    CHECK(service.playlistNames().empty());
    CHECK(service.gamesInPlaylist("Nintendo - SNES").empty());
    CHECK(service.gameCount("Nintendo - SNES") == 0);
}

TEST_CASE("playlists are listed by name, without AutoBleem's own and the Apps list, skipping what is not one") {
    RetroArchTree ra;
    ra.writePlaylist("Nintendo - SNES", {ra.snes("Chrono Trigger.sfc", "Chrono Trigger")});
    ra.writePlaylist("Atari - 2600",
                     {{"/media/roms/atari/Pitfall.a26", "Pitfall", "DETECT", "DETECT", "Atari - 2600.lpl"}}, false);
    ra.writePlaylist("AutoBleem", {ra.snes("Chrono Trigger.sfc", "Chrono Trigger")});    // our own export
    ra.writePlaylist("Applications", {ra.snes("Chrono Trigger.sfc", "Chrono Trigger")}); // the Apps list
    ra.tmp.writeFile("retroarch/playlists/Empty.lpl", "");
    ra.tmp.writeFile("retroarch/playlists/notes.txt", "not a playlist");
    ra.writePlaylist("Nothing valid", {ra.snes("Missing.sfc", "Missing")}); // every entry dropped

    CHECK(ra.service.playlistNames() == vector<string>{"Atari - 2600", "Nintendo - SNES"});
    CHECK(ra.service.gameCount("Nintendo - SNES") == 1);
    CHECK(ra.service.gameCount("Atari - 2600") == 1); // the six-line format reads the same as JSON
}

TEST_CASE("playlists are sorted by name in byte order, whatever order the directory lists them in") {
    RetroArchTree ra;
    // The lower-case name is deliberate: NTFS already hands directory entries back in case-insensitive
    // order, which would put "atari" before "Sega" and hide a missing sort. Byte order puts it after.
    ra.writePlaylist("Sega - Genesis", {ra.snes("Chrono Trigger.sfc", "Chrono Trigger")});
    ra.writePlaylist("atari - 2600",
                     {{"/media/roms/atari/Pitfall.a26", "Pitfall", "DETECT", "DETECT", "Atari - 2600.lpl"}});
    ra.writePlaylist("Nintendo - SNES", {ra.snes("Earthbound.sfc", "Earthbound")});

    CHECK(ra.service.playlistNames() == vector<string>{"Nintendo - SNES", "Sega - Genesis", "atari - 2600"});
}

TEST_CASE("an entry becomes a foreign game on the USB root, playing with the core it names when that exists") {
    RetroArchTree ra;
    ra.writePlaylist("Nintendo - SNES",
                     {ra.snes("Chrono Trigger.sfc", "Chrono Trigger", "/media/retroarch/cores/snes9x_libretro.so")});

    PsGames games = ra.service.gamesInPlaylist("Nintendo - SNES");
    REQUIRE(games.size() == 1);
    const PsGame &game = *games[0];
    CHECK(game.title == "Chrono Trigger");
    CHECK(game.foreign);
    CHECK_FALSE(game.app);
    CHECK(game.locked);
    CHECK(game.image_path == ra.tmp.at("roms/snes/Chrono Trigger.sfc")); // /media mapped onto the USB root
    CHECK(game.core_path == ra.core("snes9x_libretro"));
    CHECK(game.core_name == "Nintendo - SNES (Snes9x)");
    CHECK(game.db_name == "Nintendo - Super Nintendo Entertainment System.lpl");
}

TEST_CASE("a DETECT entry gets the core whose .info lists the playlist's database") {
    RetroArchTree ra;
    ra.writePlaylist("Nintendo - SNES", {ra.snes("Chrono Trigger.sfc", "Chrono Trigger")});
    ra.writePlaylist("Atari - 2600",
                     {{"/media/roms/atari/Pitfall.a26", "Pitfall", "DETECT", "DETECT", "Atari - 2600.lpl"}});

    PsGames snes = ra.service.gamesInPlaylist("Nintendo - SNES");
    REQUIRE(snes.size() == 1);
    // both SNES cores list the database; the one with more extensions is preferred
    CHECK(snes[0]->core_path == ra.core("snes9x_libretro"));
    CHECK(snes[0]->core_name == "Nintendo - SNES (Snes9x)");

    PsGames atari = ra.service.gamesInPlaylist("Atari - 2600");
    REQUIRE(atari.size() == 1);
    CHECK(atari[0]->core_path == ra.core("stella_libretro"));
}

TEST_CASE("an .info whose core is not installed takes no part in the mapping") {
    RetroArchTree ra;
    // the info bundle describes every core there is; this one would win on extensions, but its .so is missing
    ra.tmp.writeFile("retroarch/info/mesen_libretro.info",
                     "display_name = \"Nintendo - SNES (Mesen)\"\n"
                     "supported_extensions = \"sfc|smc|fig|swc\"\n"
                     "database = \"Nintendo - Super Nintendo Entertainment System\"\n");
    ra.tmp.writeFile(ra.coresCfg(), "Nintendo - Super Nintendo Entertainment System=Mesen\n");
    ra.writePlaylist("Nintendo - SNES", {ra.snes("Chrono Trigger.sfc", "Chrono Trigger")});

    PsGames games = ra.service.gamesInPlaylist("Nintendo - SNES");
    REQUIRE(games.size() == 1);
    CHECK(games[0]->core_path == ra.core("snes9x_libretro"));
}

TEST_CASE("a playlist path is mapped onto the USB root only when it is not there already") {
    // the console: the USB root is /media, a playlist path is what it is
    CHECK(RetroArchService::mapPlaylistPath("/media/roms/x.sfc", "/media") == "/media/roms/x.sfc");
    // a dev host: a console playlist's /media is the fake USB tree
    CHECK(RetroArchService::mapPlaylistPath("/media/roms/x.sfc", "C:/usb") == "C:/usb/roms/x.sfc");
    CHECK(RetroArchService::mapPlaylistPath("/media/retroarch/cores/a.so", "/home/me/usb") ==
          "/home/me/usb/retroarch/cores/a.so");
    // a Pi: the USB root is /media/autobleem and RetroArch writes its real mount point - no double prefix
    CHECK(RetroArchService::mapPlaylistPath("/media/autobleem/RetroArch/roms/x.sfc", "/media/autobleem") ==
          "/media/autobleem/RetroArch/roms/x.sfc");
    // a console playlist carried onto a Pi still maps
    CHECK(RetroArchService::mapPlaylistPath("/media/roms/x.sfc", "/media/autobleem") == "/media/autobleem/roms/x.sfc");
    // not a /media path: untouched (DETECT, a Windows path)
    CHECK(RetroArchService::mapPlaylistPath("DETECT", "/media/autobleem") == "DETECT");
    CHECK(RetroArchService::mapPlaylistPath("C:/usb/roms/x.sfc", "C:/usb") == "C:/usb/roms/x.sfc");
}

TEST_CASE("a core the entry names but which is not installed is re-detected") {
    RetroArchTree ra;
    ra.writePlaylist("Nintendo - SNES",
                     {ra.snes("Chrono Trigger.sfc", "Chrono Trigger", "/media/retroarch/cores/gone_libretro.so")});

    PsGames games = ra.service.gamesInPlaylist("Nintendo - SNES");
    REQUIRE(games.size() == 1);
    CHECK(games[0]->core_path == ra.core("snes9x_libretro"));
}

TEST_CASE("resources/platform/<platform>.cores.cfg picks the core for a database ahead of the .info mapping") {
    RetroArchTree ra;
    ra.tmp.writeFile(ra.coresCfg(), "# a comment\n\nNintendo - Super Nintendo Entertainment System=bsnes\n");
    ra.writePlaylist("Nintendo - SNES", {ra.snes("Chrono Trigger.sfc", "Chrono Trigger")});

    PsGames games = ra.service.gamesInPlaylist("Nintendo - SNES");
    REQUIRE(games.size() == 1);
    CHECK(games[0]->core_path == ra.core("bsnes_libretro"));
    CHECK(games[0]->core_name == "Nintendo - SNES (bsnes)");
}

TEST_CASE("entries whose ROM or core cannot be found are dropped; an entry inside an archive needs the archive") {
    RetroArchTree ra;
    ra.writePlaylist("Nintendo - SNES", {
                                            ra.snes("Chrono Trigger.sfc", "Chrono Trigger"),
                                            ra.snes("Missing.sfc", "Missing ROM"),
                                            ra.snes("pack.zip#Earthbound.sfc", "In an archive"),
                                            ra.snes("gone.zip#Earthbound.sfc", "In a missing archive"),
                                            {"/media/roms/snes/Earthbound.sfc", "No core for this", "DETECT", "DETECT",
                                             "Nintendo - Virtual Boy.lpl"},
                                        });

    CHECK(titles(ra.service.gamesInPlaylist("Nintendo - SNES")) == vector<string>{"Chrono Trigger", "In an archive"});
}

TEST_CASE("Favorites and History follow the playlists, with what RetroArch left blank filled in from them") {
    RetroArchTree ra;
    ra.writePlaylist("Nintendo - SNES",
                     {ra.snes("Chrono Trigger.sfc", "Chrono Trigger"), ra.snes("Earthbound.sfc", "Earthbound")});
    // RetroArch writes a favorite with no db_name, and a history entry with no label either
    ra.tmp.writeFile("retroarch/content_favorites.lpl",
                     RetroArchTree::json({
                         RetroArchTree::byRetroArch("Earthbound.sfc", "Earthbound"),
                         RetroArchTree::byRetroArch("Chrono Trigger.sfc", "Chrono Trigger"),
                     }));
    ra.tmp.writeFile("retroarch/content_history.lpl", RetroArchTree::json({
                                                          RetroArchTree::byRetroArch("Chrono Trigger.sfc", ""),
                                                      }));

    CHECK(ra.service.playlistNames() == vector<string>{"Nintendo - SNES", "Favorites", "History"});
    CHECK(ra.service.favoritesPlaylistName() == "Favorites");
    CHECK(ra.service.historyPlaylistName() == "History");

    PsGames favorites = ra.service.gamesInPlaylist("Favorites");
    REQUIRE(favorites.size() == 2);
    CHECK(titles(favorites) == vector<string>{"Earthbound", "Chrono Trigger"}); // RetroArch's order, not sorted
    CHECK(favorites[0]->db_name == "Nintendo - Super Nintendo Entertainment System.lpl");

    PsGames history = ra.service.gamesInPlaylist("History");
    REQUIRE(history.size() == 1);
    CHECK(history[0]->title == "Chrono Trigger"); // the blank label filled in from the SNES playlist
    CHECK(history[0]->db_name == "Nintendo - Super Nintendo Entertainment System.lpl");
}

TEST_CASE("a favorite or history entry with no playlist behind it is dropped") {
    RetroArchTree ra;
    ra.writePlaylist("Nintendo - SNES", {ra.snes("Chrono Trigger.sfc", "Chrono Trigger")});
    ra.tmp.writeFile("retroarch/content_favorites.lpl",
                     RetroArchTree::json({
                         RetroArchTree::byRetroArch("Earthbound.sfc", "Earthbound"), // not in any playlist
                         RetroArchTree::byRetroArch("Chrono Trigger.sfc", "Chrono Trigger"),
                     }));

    // Earthbound's ROM and core both exist, but nothing can fill in its db_name, so there is no boxart for
    // it and no way to tell what it is - it is dropped
    CHECK(titles(ra.service.gamesInPlaylist("Favorites")) == vector<string>{"Chrono Trigger"});
}

TEST_CASE("reloadFavoritesAndHistory picks up what RetroArch changed while it ran") {
    RetroArchTree ra;
    ra.writePlaylist("Nintendo - SNES",
                     {ra.snes("Chrono Trigger.sfc", "Chrono Trigger"), ra.snes("Earthbound.sfc", "Earthbound")});
    ra.tmp.writeFile("retroarch/content_favorites.lpl",
                     RetroArchTree::json({
                         RetroArchTree::byRetroArch("Chrono Trigger.sfc", "Chrono Trigger"),
                     }));
    REQUIRE(ra.service.playlistNames() == vector<string>{"Nintendo - SNES", "Favorites"});
    REQUIRE(ra.service.gameCount("Favorites") == 1);

    // RetroArch ran: a second favorite, and a history file that did not exist before
    ra.tmp.writeFile("retroarch/content_favorites.lpl",
                     RetroArchTree::json({
                         RetroArchTree::byRetroArch("Chrono Trigger.sfc", "Chrono Trigger"),
                         RetroArchTree::byRetroArch("Earthbound.sfc", "Earthbound"),
                     }));
    ra.tmp.writeFile("retroarch/content_history.lpl", RetroArchTree::json({
                                                          RetroArchTree::byRetroArch("Earthbound.sfc", ""),
                                                      }));
    ra.service.reloadFavoritesAndHistory();

    CHECK(ra.service.playlistNames() == vector<string>{"Nintendo - SNES", "Favorites", "History"});
    CHECK(ra.service.gameCount("Favorites") == 2);
    CHECK(titles(ra.service.gamesInPlaylist("History")) == vector<string>{"Earthbound"});
}

TEST_CASE("escapeName is how RetroArch names a boxart file after a title") {
    CHECK(RetroArchService::escapeName("Star Wars: Episode I / Racer?") == "Star Wars_ Episode I _ Racer_");
    CHECK(RetroArchService::escapeName("Plain") == "Plain");
}
