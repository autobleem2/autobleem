// lib_ableem - engine: one game as stored in regional.db / internal.db (plus the flags USB games keep in
// their Game.ini). The application's own game type derives from this and adds whatever the UI needs.
#pragma once

#include <ctime>
#include <string>
#include <vector>

namespace ableem {

//******************
// GameRecord
//******************
struct GameRecord {
    int gameId = 0;
    std::string title;
    std::string publisher;
    int year = 0;
    std::string serial;
    std::string region;
    int players = 0;

    std::string memcard;    // "SONY" or the name of a !MemCards set
    std::string folder;     // game folder.  internal example: "/gaadata/8/", USB example: "/media/Games/Racing/007 Racing"
    std::string ssFolder;   // !SaveStates folder.  ex: "/Games/!SaveStates/8", "/Games/!SaveStates/007 Racing"

    std::string base;       // file name of the game.  not sure if extension is included.
                            // code looks for .pbp extension and replaces it with cue.  but elsewhere .png is appended without removing extension.

    bool internal = false;  // one of the console's built-in games (internal.db) rather than a USB game (regional.db)
    bool hd = false;
    bool locked = false;    // Game.ini "Automation" == 0: the user edited the ini, the scanner must not overwrite it
    int cds = 1;
    // special flags
    bool favorite = false;
    bool play_using_ra = false;
    int history = 0;        // 0 = not in history list.  1-100 if in the history list
    time_t last_played = 0; // in seconds since 1970
};

using GameRecords = std::vector<GameRecord>;

} // namespace ableem
