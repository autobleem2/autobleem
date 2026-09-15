//
// Created by screemer on 2/12/19.
//

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <ableem/engine/game_record.h>

//******************
// PsGame
//******************
// A game as the UI sees it. The database part is ableem::GameRecord; this adds what only the launcher knows
// (RetroArch / App entries and the resume points under ssFolder).
class PsGame : public ableem::GameRecord {
public:
    bool foreign = false; // to state it is not PS1 game (RA)
    bool app = false;

    // RB and App params
    std::string core_path;
    std::string image_path;
    std::string core_name;

    std::string readme_path;
    std::string startup;
    bool kernel = false;

    std::string db_name;

    // what the database hands out is plain records; wrap them for the UI
    static std::vector<std::shared_ptr<PsGame>> fromRecords(const ableem::GameRecords &records);

    void setMemCard(std::string name);
    std::string findResumePicture();
    bool isResumeSlotActive(int slot);
    std::string findResumePicture(int slot);
    void storeResumePicture(int slot);
    bool isCleanExit();
    void removeResumePoint(int slot);
};

using PsGamePtr = std::shared_ptr<PsGame>;
using PsGames = std::vector<PsGamePtr>;

void operator += (PsGames &dest, const PsGames &src);
