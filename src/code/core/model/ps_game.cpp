//
// Created by screemer on 2/12/19.
//

#include "ps_game.h"
#include "../main.h"

using namespace std;

//*******************************
// PsGame::fromRecords
//*******************************
PsGames PsGame::fromRecords(const ableem::GameRecords &records) {
    PsGames games;
    games.reserve(records.size());
    for (const auto &record : records) {
        PsGamePtr game{new PsGame};
        static_cast<ableem::GameRecord &>(*game) = record;
        games.push_back(game);
    }
    return games;
}

//*******************************
// PsGames += PsGames
//*******************************
void operator+=(PsGames &dest, const PsGames &src) {
    copy(begin(src), end(src), back_inserter(dest));
}
