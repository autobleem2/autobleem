//
// ResumePointService: the save-state slots under a game's !SaveStates folder.
//
#include "doctest/doctest.h"

#include "../support/string_maker.h"
#include "../support/temp_dir.h"

#include "core/services/resume_point.h"

#include <ableem/engine/environment.h>
#include <ableem/engine/filesystem.h>

#include <memory>
#include <string>

using std::string;

namespace {

// A game with the folder layout PCSX leaves behind. Nothing here needs a database - a resume point is
// entirely a matter of files under game.ssFolder.
struct Resume {
    Resume() : tmp("resume") {
        tmp.makeSubDir("Games/Tekken 3");
        tmp.makeSubDir("Games/!SaveStates/Tekken 3/sstates");
        tmp.makeSubDir("Games/!SaveStates/Tekken 3/screenshots");

        game = std::make_shared<PsGame>();
        game->gameId = 1;
        game->title = "Tekken 3";
        game->folder = tmp.at("Games/Tekken 3");
        game->ssFolder = tmp.at("Games/!SaveStates/Tekken 3");
    }

    string ss(const string &relative) const { return game->ssFolder + ableem::sep + relative; }
    bool exists(const string &relative) const { return ableem::DirEntry::exists(ss(relative)); }

    // what PCSX writes on a clean exit: a filename file whose second line names the state, plus the state
    // and screenshot themselves
    void pcsxExitsHavingWritten(const string &stateName) {
        tmp.writeFile("Games/!SaveStates/Tekken 3/filename.txt",
                      "/media/Games/Tekken 3/Tekken 3.cue\n" + stateName + "\n");
        tmp.writeFile("Games/!SaveStates/Tekken 3/sstates/" + stateName + ".000", "the state");
        tmp.writeFile("Games/!SaveStates/Tekken 3/screenshots/" + stateName + ".png", "the screenshot");
        tmp.writeFile("Games/!SaveStates/Tekken 3/lastcdimg.txt", "/media/Games/Tekken 3/Tekken 3.cue\n");
    }

    TempDir tmp;
    PsGamePtr game;
    ResumePointService service;
};

} // namespace

TEST_CASE("a game with nothing saved has no active slots and no picture") {
    Resume r;

    for (int slot = 0; slot < ResumePointService::SlotCount; slot++) {
        CHECK_FALSE(r.service.slotIsActive(*r.game, slot));
        CHECK(r.service.pictureForSlot(*r.game, slot) == "");
    }
    CHECK(r.service.lastPicture(*r.game) == "");
}

TEST_CASE("saving after a run keeps the state, the filename file and the disc note as that slot") {
    Resume r;
    r.pcsxExitsHavingWritten("TEKKEN3");

    r.service.saveAfterLaunch(*r.game, 2);

    CHECK(r.exists("sstates/TEKKEN3.002.res")); // the state, kept for slot 2
    CHECK(r.exists("filename.txt.res"));        // the shared kept copy
    CHECK(r.exists("filename.2.txt.res"));      // and slot 2's own
    CHECK(r.exists("lastcdimg.2.txt"));         // which disc it was playing

    // what PCSX left is moved, not copied
    CHECK_FALSE(r.exists("sstates/TEKKEN3.000"));
    CHECK_FALSE(r.exists("filename.txt"));
    CHECK_FALSE(r.exists("lastcdimg.txt"));
}

TEST_CASE("a run that did not exit cleanly saves nothing") {
    Resume r;
    // no filename.txt: PCSX was killed rather than quit
    r.tmp.writeFile("Games/!SaveStates/Tekken 3/sstates/TEKKEN3.000", "the state");

    CHECK_FALSE(r.service.exitedCleanly(*r.game));

    r.service.saveAfterLaunch(*r.game, 0);

    CHECK_FALSE(r.exists("filename.txt.res"));
    CHECK_FALSE(r.exists("sstates/TEKKEN3.000.res"));
}

TEST_CASE("exitedCleanly is true once PCSX has written its filename file") {
    Resume r;
    CHECK_FALSE(r.service.exitedCleanly(*r.game));

    r.pcsxExitsHavingWritten("TEKKEN3");
    CHECK(r.service.exitedCleanly(*r.game));
}

TEST_CASE("storing the picture makes the slot active and moves the screenshot into place") {
    Resume r;
    r.pcsxExitsHavingWritten("TEKKEN3");
    r.service.saveAfterLaunch(*r.game, 1);

    CHECK_FALSE(r.service.slotIsActive(*r.game, 1)); // the state is kept, but no picture yet

    r.service.storePictureForSlot(*r.game, 1);

    CHECK(r.service.slotIsActive(*r.game, 1));
    CHECK(r.service.pictureForSlot(*r.game, 1) == r.ss("screenshots/TEKKEN3.1.png.res"));
    CHECK_FALSE(r.exists("screenshots/TEKKEN3.png")); // moved, not copied
}

TEST_CASE("slot 0's picture is the one without a number in its name") {
    Resume r;
    r.pcsxExitsHavingWritten("TEKKEN3");
    r.service.saveAfterLaunch(*r.game, 0);
    r.service.storePictureForSlot(*r.game, 0);

    // the naming is not uniform across slots, and callers rely on it
    CHECK(r.service.pictureForSlot(*r.game, 0) == r.ss("screenshots/TEKKEN3.png.res"));
}

TEST_CASE("slots are independent of each other") {
    Resume r;

    r.pcsxExitsHavingWritten("TEKKEN3");
    r.service.saveAfterLaunch(*r.game, 0);
    r.service.storePictureForSlot(*r.game, 0);

    r.pcsxExitsHavingWritten("TEKKEN3");
    r.service.saveAfterLaunch(*r.game, 3);
    r.service.storePictureForSlot(*r.game, 3);

    CHECK(r.service.slotIsActive(*r.game, 0));
    CHECK(r.service.slotIsActive(*r.game, 3));
    CHECK_FALSE(r.service.slotIsActive(*r.game, 1));
    CHECK_FALSE(r.service.slotIsActive(*r.game, 2));
}

TEST_CASE("removing a slot clears its state and its picture") {
    Resume r;
    r.pcsxExitsHavingWritten("TEKKEN3");
    r.service.saveAfterLaunch(*r.game, 2);
    r.service.storePictureForSlot(*r.game, 2);
    REQUIRE(r.service.slotIsActive(*r.game, 2));

    r.service.removeSlot(*r.game, 2);

    CHECK_FALSE(r.service.slotIsActive(*r.game, 2));
    CHECK_FALSE(r.exists("sstates/TEKKEN3.002.res"));
    CHECK_FALSE(r.exists("screenshots/TEKKEN3.2.png.res"));
}

TEST_CASE("preparing for a launch puts the slot's state back where PCSX will find it") {
    Resume r;
    r.pcsxExitsHavingWritten("TEKKEN3");
    r.service.saveAfterLaunch(*r.game, 1);
    REQUIRE_FALSE(r.exists("sstates/TEKKEN3.000"));

    r.service.prepareForLaunch(*r.game, 1);

    CHECK(r.exists("sstates/TEKKEN3.000")); // PCSX loads this
    CHECK(r.exists("lastcdimg.1.txt"));     // pointing at the disc image in the game's own folder

    // The note is rewritten to point into this game's folder rather than wherever the state was recorded.
    // Compared by prefix: the file is written in text mode, so the line ending is the platform's.
    string note = r.tmp.readFile("Games/!SaveStates/Tekken 3/lastcdimg.1.txt");
    string expected = r.game->folder + ableem::sep + "Tekken 3.cue";
    CHECK(note.compare(0, expected.size(), expected) == 0);
}

TEST_CASE("preparing clears what the previous run left behind") {
    Resume r;
    r.pcsxExitsHavingWritten("TEKKEN3");
    // a crash leaves these lying around, and PCSX will not overwrite them
    REQUIRE(r.exists("filename.txt"));
    REQUIRE(r.exists("sstates/TEKKEN3.000"));
    REQUIRE(r.exists("screenshots/TEKKEN3.png"));

    r.service.prepareForLaunch(*r.game, -1);

    CHECK_FALSE(r.exists("filename.txt"));
    CHECK_FALSE(r.exists("sstates/TEKKEN3.000"));
    CHECK_FALSE(r.exists("screenshots/TEKKEN3.png"));
}

TEST_CASE("preparing slot -1 starts from the beginning and restores nothing") {
    Resume r;
    r.pcsxExitsHavingWritten("TEKKEN3");
    r.service.saveAfterLaunch(*r.game, 0);

    r.service.prepareForLaunch(*r.game, -1);

    CHECK_FALSE(r.exists("sstates/TEKKEN3.000")); // nothing put back for PCSX to load
    CHECK(r.exists("sstates/TEKKEN3.000.res"));   // but the slot itself is untouched
}

TEST_CASE("a full save, resume and re-save round trip keeps the slot") {
    Resume r;

    r.pcsxExitsHavingWritten("TEKKEN3");
    r.service.saveAfterLaunch(*r.game, 1);
    r.service.storePictureForSlot(*r.game, 1);
    REQUIRE(r.service.slotIsActive(*r.game, 1));

    // resume from it, play, and quit again
    r.service.prepareForLaunch(*r.game, 1);
    r.pcsxExitsHavingWritten("TEKKEN3");
    r.service.saveAfterLaunch(*r.game, 1);
    r.service.storePictureForSlot(*r.game, 1);

    CHECK(r.service.slotIsActive(*r.game, 1));
    CHECK(r.exists("sstates/TEKKEN3.001.res"));
}

TEST_CASE("a foreign entry has no resume points at all") {
    Resume r;
    r.game->foreign = true;
    r.pcsxExitsHavingWritten("TEKKEN3");

    CHECK(r.service.exitedCleanly(*r.game)); // nothing writes one, so nothing can be missing
    CHECK(r.service.lastPicture(*r.game) == "");
    CHECK_FALSE(r.service.slotIsActive(*r.game, 0));

    // and the calls that write are no-ops rather than touching a folder it does not own
    r.service.storePictureForSlot(*r.game, 0);
    CHECK(r.exists("screenshots/TEKKEN3.png"));
    r.service.removeSlot(*r.game, 0);
}
