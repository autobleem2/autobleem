//
// ThumbnailLookup over a RetroArch tree built in a temp dir. AutoBleem-NG's thumbnail_lookup_test, on
// doctest: exact hits, tag stripping, the rdb name before the title, the fuzzy fallback and its scoring,
// and the user's screenshots and save-state pictures.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/temp_dir.h"

#include <ableem/engine/thumbnail_lookup.h>

#include <string>

using ableem::ThumbnailLookup;
using std::string;

namespace {

const char *const kSystem = "Sony - PlayStation";

struct ThumbTree {
    ThumbTree() : tmp("thumbs") {
        string ra = "retroarch";
        boxarts = tmp.makeSubDir(ra + "/thumbnails/" + kSystem + "/Named_Boxarts");
        titles = tmp.makeSubDir(ra + "/thumbnails/" + kSystem + "/Named_Titles");
        snaps = tmp.makeSubDir(ra + "/thumbnails/" + kSystem + "/Named_Snaps");
        screenshots = tmp.makeSubDir(ra + "/screenshots");
        states = tmp.makeSubDir(ra + "/states");
        env.setUsbRoot(tmp.path());
        env.setRetroarchDir(tmp.at(ra));
    }
    // a file by its full path
    void touch(const string &path) const {
        string rel = path.substr(tmp.path().size() + 1);
        tmp.writeFile(rel, "");
    }

    EnvFixture env;
    TempDir tmp;
    string boxarts, titles, snaps, screenshots, states;
};

} // namespace

TEST_CASE("escapeName replaces what libretro-thumbnails replaces") {
    CHECK(ThumbnailLookup::escapeName("Foo: Bar") == "Foo_ Bar");
    CHECK(ThumbnailLookup::escapeName("a/b/c/d") == "a_b_c_d");
    CHECK(ThumbnailLookup::escapeName("Castlevania - Symphony of the Night") == "Castlevania - Symphony of the Night");
    CHECK(ThumbnailLookup::escapeName("?|") == "__");
}

TEST_CASE("an exact name is found as png, jpg or jpeg, png first") {
    ThumbTree t;
    ThumbnailLookup lookup;
    t.touch(t.boxarts + "/Wild Arms (USA).png");
    CHECK(lookup.findThumbnail(kSystem, "Wild Arms (USA)", "Named_Boxarts") == t.boxarts + "/Wild Arms (USA).png");

    t.touch(t.boxarts + "/Jpeg Game.jpg");
    CHECK(lookup.findThumbnail(kSystem, "Jpeg Game", "Named_Boxarts") == t.boxarts + "/Jpeg Game.jpg");

    t.touch(t.boxarts + "/Foo.png");
    t.touch(t.boxarts + "/Foo.jpg");
    CHECK(lookup.findThumbnail(kSystem, "Foo", "Named_Boxarts") == t.boxarts + "/Foo.png");

    CHECK(lookup.findThumbnail(kSystem, "Nope", "Named_Boxarts") == "");
    CHECK(lookup.findThumbnail("", "Foo", "Named_Boxarts") == "");
    CHECK(lookup.findThumbnail(kSystem, "", "Named_Boxarts") == "");
    CHECK(lookup.findThumbnail(kSystem, "Foo", "") == "");

    t.touch(t.boxarts + "/Foo_ Bar.png"); // the title's colon is escaped before looking
    CHECK(lookup.findThumbnail(kSystem, "Foo: Bar", "Named_Boxarts") == t.boxarts + "/Foo_ Bar.png");
}

TEST_CASE("findBoxArt: Named_Boxarts, else Named_Titles, else Named_Snaps") {
    ThumbTree t;
    ThumbnailLookup lookup;
    t.touch(t.snaps + "/Game.png");
    CHECK(lookup.findBoxArt(kSystem, "Game") == t.snaps + "/Game.png");
    t.touch(t.titles + "/Game.png");
    CHECK(lookup.findBoxArt(kSystem, "Game") == t.titles + "/Game.png");
    t.touch(t.boxarts + "/Game.png");
    CHECK(lookup.findBoxArt(kSystem, "Game") == t.boxarts + "/Game.png");
}

TEST_CASE("trailing tags are peeled one at a time, never the name itself") {
    ThumbTree t;
    ThumbnailLookup lookup;
    CHECK(lookup.findThumbnail(kSystem, "Persona (USA)", "Named_Boxarts") == ""); // the bare name misses cleanly
    t.touch(t.boxarts + "/Persona.jpg");
    CHECK(lookup.findThumbnail(kSystem, "Persona (USA)", "Named_Boxarts") == t.boxarts + "/Persona.jpg");

    t.touch(t.boxarts + "/Castlevania - Symphony of the Night.jpg");
    CHECK(lookup.findThumbnail(kSystem, "Castlevania - Symphony of the Night (USA) (Greatest Hits)", "Named_Boxarts") ==
          t.boxarts + "/Castlevania - Symphony of the Night.jpg");
}

TEST_CASE("the rdb's record name is tried before the title, stripping included") {
    ThumbTree t;
    ThumbnailLookup lookup;
    t.touch(t.boxarts + "/Metal Gear Solid (USA) (Disc 1).png");
    t.touch(t.boxarts + "/Metal Gear Solid.png");
    CHECK(lookup.findThumbnail(kSystem, "Metal Gear Solid", "Named_Boxarts", "Metal Gear Solid (USA) (Disc 1)") ==
          t.boxarts + "/Metal Gear Solid (USA) (Disc 1).png");

    t.touch(t.boxarts + "/Persona.jpg");
    CHECK(lookup.findThumbnail(kSystem, "Revelations - Persona", "Named_Boxarts", "Persona (USA)") ==
          t.boxarts + "/Persona.jpg");
    CHECK(lookup.findBoxArt(kSystem, "Wrong Title", "Persona (USA)") == t.boxarts + "/Persona.jpg");
    CHECK(lookup.findThumbnail(kSystem, "Some Title", "Named_Boxarts", "Some Record (USA)") == "");

    t.touch(t.boxarts + "/Game.png");
    CHECK(lookup.findThumbnail(kSystem, "Game", "Named_Boxarts", "") == t.boxarts + "/Game.png");

    t.touch(t.snaps + "/Persona.jpg");
    CHECK(lookup.findSnap(kSystem, "Wrong Title", "", "Persona (USA)") == t.snaps + "/Persona.jpg");
}

TEST_CASE("the fuzzy fallback takes another region's file, prefers shared tags, needs the word boundary") {
    ThumbTree t;
    ThumbnailLookup lookup;
    t.touch(t.boxarts + "/Doom (Europe) (EDC).jpg");
    CHECK(lookup.findThumbnail(kSystem, "Doom (USA)", "Named_Boxarts") == t.boxarts + "/Doom (Europe) (EDC).jpg");

    t.touch(t.boxarts + "/Suikoden (USA) (Rev 1).jpg");
    t.touch(t.boxarts + "/Suikoden (Europe).jpg");
    lookup.clearCache();
    CHECK(lookup.findThumbnail(kSystem, "Suikoden (USA)", "Named_Boxarts") ==
          t.boxarts + "/Suikoden (USA) (Rev 1).jpg");

    t.touch(t.boxarts + "/Doom 2 - Hell on Earth (USA).jpg");
    lookup.clearCache();
    CHECK(lookup.findThumbnail(kSystem, "Doom 2 (USA)", "Named_Boxarts") == ""); // "Doom 2 (" matches nothing

    t.touch(t.boxarts + "/Foo (Japan).jpg");
    t.touch(t.boxarts + "/Foo (Europe).jpg");
    t.touch(t.boxarts + "/Foo (USA).jpg");
    lookup.clearCache();
    CHECK(lookup.findThumbnail(kSystem, "Foo (Asia)", "Named_Boxarts") ==
          t.boxarts + "/Foo (USA).jpg"); // region tiebreak
    CHECK(lookup.findThumbnail(kSystem, "Foo (Europe)", "Named_Boxarts") ==
          t.boxarts + "/Foo (Europe).jpg"); // the exact hit first

    t.touch(t.boxarts + "/Foo_ Bar (USA) (Rev 1).png");
    lookup.clearCache();
    CHECK(lookup.findThumbnail(kSystem, "Foo: Bar (USA)", "Named_Boxarts") ==
          t.boxarts + "/Foo_ Bar (USA) (Rev 1).png");
}

TEST_CASE("the directory listing is cached until clearCache") {
    ThumbTree t;
    ThumbnailLookup lookup;
    t.touch(t.boxarts + "/Bar (Europe).jpg");
    CHECK(lookup.findThumbnail(kSystem, "Bar (USA)", "Named_Boxarts") == t.boxarts + "/Bar (Europe).jpg");
    t.touch(t.boxarts + "/Bar (USA) (Rev 1).jpg");
    CHECK(lookup.findThumbnail(kSystem, "Bar (USA)", "Named_Boxarts") ==
          t.boxarts + "/Bar (Europe).jpg"); // stale listing
    lookup.clearCache();
    CHECK(lookup.findThumbnail(kSystem, "Bar (USA)", "Named_Boxarts") == t.boxarts + "/Bar (USA) (Rev 1).jpg");
}

TEST_CASE("the user's own screenshot wins, newest first, then the save state's picture, then Named_Snaps") {
    ThumbTree t;
    ThumbnailLookup lookup;
    CHECK(lookup.findLocalScreenshot("/x/Game.chd") == "");
    CHECK(lookup.findLocalScreenshot("") == "");

    t.touch(t.snaps + "/Game.png");
    CHECK(lookup.findSnap(kSystem, "Game", "/x/Game.chd") == t.snaps + "/Game.png");

    t.touch(t.states + "/Game.state.auto.png");
    lookup.clearCache();
    CHECK(lookup.findLocalScreenshot("/x/Game.chd") == t.states + "/Game.state.auto.png");

    t.touch(t.screenshots + "/Game-260101-120000.png");
    t.touch(t.screenshots + "/Game-260201-080000.png");
    t.touch(t.screenshots + "/Game 2-260301-080000.png"); // another game
    t.touch(t.screenshots + "/Gamer.png");                // not this one either
    lookup.clearCache();
    CHECK(lookup.findLocalScreenshot("/x/Game.chd") == t.screenshots + "/Game-260201-080000.png");
    CHECK(lookup.findSnap(kSystem, "Game", "/x/Game.chd") == t.screenshots + "/Game-260201-080000.png");

    t.touch(t.screenshots + "/Wild Arms (USA).png");
    lookup.clearCache();
    CHECK(lookup.findLocalScreenshot("/games/Wild Arms (USA).cue") == t.screenshots + "/Wild Arms (USA).png");
}
