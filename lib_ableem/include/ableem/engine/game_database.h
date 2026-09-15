// lib_ableem - engine: the SQLite game databases. One class serves regional.db (USB games, written by the
// scanner), internal.db (the console's built-in games with a few columns added) and the covers*.db files
// (see CoverDatabase). Every query fails soft: a false return and a line on stderr, never an exception.
#pragma once

#include <string>
#include <vector>

#include "game_metadata.h"
#include "game_record.h"

struct sqlite3;

namespace ableem {

//******************
// SubDirRowInfo
//******************
// one row of the "select game dir" menu: a sub-directory of the games dir and how many games it shows
struct SubDirRowInfo {
    int subDirRowIndex = 0;
    std::string rowName = "";
    int indentLevel = 0;
    int numGames = 0;
};

using SubDirRowInfos = std::vector<SubDirRowInfo>;

//******************
// SubDirRowGame
//******************
struct SubDirRowGame {
    int rowIndex = 0;
    int gameId = 0;
};

using SubDirRowGames = std::vector<SubDirRowGame>;

//******************
// GameDatabase
//******************
class GameDatabase {
public:
    GameDatabase() {}
    ~GameDatabase();    // closes
    GameDatabase(const GameDatabase &) = delete;
    GameDatabase &operator=(const GameDatabase &) = delete;

    bool open(const std::string &fileName);     // creates the file when missing
    void close();
    bool createSchema();                        // CREATE TABLE IF NOT EXISTS for every table regional.db needs
    void addFavoriteColumnIfMissing();          // internal.db only: the stock schema lacks these four columns
    void addPlayUsingRAColumnIfMissing();
    void addHistoryColumnIfMissing();
    void addLastPlayedColumnIfMissing();
    bool clearAllTables();

    bool beginTransaction();
    bool commit();
    bool rollback();

    bool insertGame(int id, std::string title, std::string publisher, int players, int year, std::string path, std::string sspath,
                    std::string memcard);
    bool insertDisc(int id, int discNum, std::string discName);

    bool subDirRowsTableIsEmpty();
    bool insertSubDirRow(int rowIndex, std::string rowName, int indentLevel, int numGames);
    bool insertSubDirRowGame(int rowIndex, int gameId);

    // covers*.db
    bool findMetadataBySerial(std::string serial, GameMetadata *md);
    bool findMetadataByTitle(std::string title, GameMetadata *md);

    int countGames();
    bool updateYear(int id, int year);
    bool updateMemcard(int id, std::string memcard);

    GameRecords loadUsbGames();         // regional.db; Game.ini flags are merged in. empty on error.
    GameRecords loadInternalGames();    // internal.db
    bool loadSubDirRows(SubDirRowInfos *rows);
    bool loadSubDirRowGames(SubDirRowGames *rowGames);
    bool loadGameIdsInSubDirRow(std::vector<int> *gameIds, int row);

    bool updateTitle(int id, std::string title);
    bool updateFavorite(int id, int fav);
    bool updatePlayUsingRA(int id, int play_using_ra);
    bool updateHistory(int id, int rank);   // 0 = not in history, 1-100 history from latest game played to oldest
    bool updateDatePlayed(int id, int date_in_seconds);   // seconds since 1970
    bool reloadUsbGame(GameRecord &game);           // re-reads the row game.gameId (regional.db)
    bool reloadInternalGame(GameRecord &game);      // same for internal.db

    bool deleteGame(int id);                        // from every table, in one transaction

private:
    sqlite3 *db = nullptr;
    bool deleteGameIdFromOneTable(int id, const char *sql);
    bool executeCreateStatement(const char *sql, const std::string &name);
    bool executeStatement(const char *sql, const std::string &outMsg, const std::string &errorMsg);
};

} // namespace ableem
