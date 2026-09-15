#include "database.h"
#include "../main.h"
#include "../util.h"
#include <iostream>
#include <vector>
#include "../main.h"
#include "../environment.h"

using namespace std;

                                  //*******************************
                                  // DATABASE SQL
                                  //*******************************

//*******************************
// covers?.db
//*******************************

// used by: querySerial
static const char SELECT_META[] = "SELECT SERIAL,TITLE, PUBLISHER, \
                                RELEASE,PLAYERS,  COVER FROM SERIALS s \
                                JOIN GAME g on s.GAME=g.id \
                                WHERE SERIAL=? OR SERIAL LIKE ?";

// used by: queryTitle
static const char SELECT_TITLE[] = "SELECT SERIAL,TITLE, PUBLISHER, \
                                RELEASE,PLAYERS, COVER FROM SERIALS s \
                                JOIN GAME g on s.GAME=g.id \
                                WHERE TITLE=?";

//*******************************
// RELEASE_YEAR is found in both internal.db and regional.db
// used by Database::updateYear() which is only called by VerMigration::migrate04_05()
// VerMigration appears to be no longer used
//*******************************
// used by: updateYear
static const char UPDATE_YEAR[] = "UPDATE GAME SET RELEASE_YEAR=? WHERE GAME_ID=?";

//*******************************
// regional.db
//*******************************

// used by: updateMemcard
static const char UPDATE_MEMCARD[] = "UPDATE GAME SET MEMCARD=? WHERE GAME_ID=?";

// used by: getGames
static const char GAMES_DATA[] = "SELECT g.GAME_ID, GAME_TITLE_STRING, PUBLISHER_NAME, RELEASE_YEAR, \
                                  PLAYERS, PATH, SSPATH, MEMCARD, d.BASENAME, HISTORY, LAST_PLAYED, \
                                  COUNT(d.GAME_ID) as NUMD \
                                  FROM GAME G JOIN DISC d ON g.GAME_ID=d.GAME_ID \
                                  GROUP BY g.GAME_ID HAVING MIN(d.DISC_NUMBER) \
                                  ORDER BY g.GAME_TITLE_STRING asc,d.DISC_NUMBER ASC";

// used by: refreshGameInternal
// used by: refreshGame
static const char GAMES_DATA_SINGLE[] = "SELECT g.GAME_ID, GAME_TITLE_STRING, PUBLISHER_NAME, RELEASE_YEAR, \
                                        PLAYERS, PATH, SSPATH, MEMCARD, d.BASENAME, HISTORY, LAST_PLAYED, \
                                        COUNT(d.GAME_ID) as NUMD \
                                        FROM GAME G JOIN DISC d ON g.GAME_ID=d.GAME_ID \
                                        WHERE g.GAME_ID=?  \
                                        GROUP BY g.GAME_ID HAVING MIN(d.DISC_NUMBER) \
                                        ORDER BY g.GAME_TITLE_STRING asc,d.DISC_NUMBER ASC";

// used by: insertGame
static const char INSERT_GAME[] = "INSERT INTO GAME ([GAME_ID],[GAME_TITLE_STRING],[PUBLISHER_NAME],[RELEASE_YEAR],\
                                   [PLAYERS],[RATING_IMAGE],[GAME_MANUAL_QR_IMAGE],[LINK_GAME_ID],\
                                   [PATH],[SSPATH],[MEMCARD]) \
                                   values (?,?,?,?,?,'CERO_A','QR_Code_GM','',?,?,?)";

// used by: createInitialDatabase
static const char CREATE_GAME_SQL[] = " CREATE TABLE IF NOT EXISTS GAME  \
     ( GAME_ID integer NOT NULL UNIQUE, \
       GAME_TITLE_STRING text, \
       PUBLISHER_NAME text, \
       RELEASE_YEAR integer,\
       PLAYERS integer,     \
       RATING_IMAGE text,   \
       GAME_MANUAL_QR_IMAGE text, \
       LINK_GAME_ID integer,\
       PATH    text null,   \
       SSPATH  text null,   \
       MEMCARD text null,   \
       HISTORY integer,     \
       LAST_PLAYED integer, \
       PRIMARY KEY ( GAME_ID ) )";

// used by: createInitialDatabase
static const char CREATE_DISC_SQL[] = " CREATE TABLE IF NOT EXISTS DISC \
     ( [GAME_ID] integer, \
       [DISC_NUMBER] integer, \
       [BASENAME] text, \
          UNIQUE ([GAME_ID], [DISC_NUMBER]) )";

// used by: createInitialDatabase
static const char CREATE_SUBDIR_ROW_SQL[] = " CREATE TABLE IF NOT EXISTS SUBDIR_ROWS  \
     ( SUBDIR_ROW_INDEX integer NOT NULL UNIQUE, \
       SUBDIR_ROW_NAME text, \
       INDENT_LEVEL integer,    \
       NUM_GAMES integer,    \
       PRIMARY KEY ( SUBDIR_ROW_INDEX ) )";

// used by: createInitialDatabase
static const char CREATE_SUBDIR_GAMES_TO_DISPLAY_ON_ROW_SQL[] = " CREATE TABLE IF NOT EXISTS SUBDIR_GAMES_TO_DISPLAY_ON_ROW  \
     ( SUBDIR_ROW_INDEX integer, GAME_ID integer )";

// used by: subDirRowsTableIsEmpty
static const char IS_SUBDIR_ROWS_TABLE_EMPTY[] = "SELECT count(*) FROM SUBDIR_ROWS";

// used by: insertSubDirRow
static const char INSERT_SUBDIR_ROW[] = "INSERT INTO SUBDIR_ROWS \
        ([SUBDIR_ROW_INDEX],[SUBDIR_ROW_NAME],[INDENT_LEVEL],[NUM_GAMES]) \
        values (?,?,?,?)";

// used by: getGameRowInfos
static const char GET_SUBDIR_ROW[] = "SELECT SUBDIR_ROW_INDEX, SUBDIR_ROW_NAME, INDENT_LEVEL, NUM_GAMES FROM \
        SUBDIR_ROWS ORDER BY SUBDIR_ROW_INDEX";

// used by: insertSubDirGames
static const char INSERT_SUBDIR_GAME[] = "INSERT INTO SUBDIR_GAMES_TO_DISPLAY_ON_ROW \
        ([SUBDIR_ROW_INDEX],[GAME_ID]) values (?,?)";

// used by: getGameRowGameInfos
static const char GET_SUBDIR_GAME[] = "SELECT SUBDIR_ROW_INDEX, GAME_ID FROM \
        SUBDIR_GAMES_TO_DISPLAY_ON_ROW ORDER BY SUBDIR_ROW_INDEX";

// used by: getGameIdsInRow
static const char GET_SUBDIR_GAME_ON_ROW[] = "SELECT GAME_ID FROM \
        SUBDIR_GAMES_TO_DISPLAY_ON_ROW WHERE SUBDIR_ROW_INDEX =?";

// used by: deleteGameIdInAllTables
static const char DELETE_GAME_ID_FROM_DISC[] = "DELETE FROM DISC WHERE GAME_ID =?";
static const char DELETE_GAME_ID_FROM_GAME[] = "DELETE FROM GAME WHERE GAME_ID =?";
static const char DELETE_GAME_ID_FROM_SUBDIR_GAMES_TO_DISPLAY_ON_ROW[] = "DELETE FROM SUBDIR_GAMES_TO_DISPLAY_ON_ROW WHERE GAME_ID =?";

//*******************************
// internal.db
//*******************************

// used by: refreshGameInternal
static const char GAMES_DATA_SINGLE_INTERNAL[] = "SELECT g.GAME_ID, GAME_TITLE_STRING, PUBLISHER_NAME, RELEASE_YEAR, PLAYERS, d.BASENAME,  COUNT(d.GAME_ID) as NUMD, \
                                     FAVORITE, PLAY_USING_RA, HISTORY, LAST_PLAYED FROM GAME G JOIN DISC d ON g.GAME_ID=d.GAME_ID \
                                     WHERE g.GAME_ID=?  \
                                     GROUP BY g.GAME_ID HAVING MIN(d.DISC_NUMBER) \
                                     ORDER BY g.GAME_TITLE_STRING asc,d.DISC_NUMBER ASC";

// used by: getInternalGames
static const char GAMES_DATA_INTERNAL[] = "SELECT g.GAME_ID, GAME_TITLE_STRING, PUBLISHER_NAME, RELEASE_YEAR, PLAYERS, d.BASENAME,  COUNT(d.GAME_ID) as NUMD, \
                                     FAVORITE, PLAY_USING_RA, HISTORY, LAST_PLAYED FROM GAME G JOIN DISC d ON g.GAME_ID=d.GAME_ID \
                                     GROUP BY g.GAME_ID HAVING MIN(d.DISC_NUMBER) \
                                     ORDER BY g.GAME_TITLE_STRING asc,d.DISC_NUMBER ASC";

// used by: addFavoriteColumn for the internal.db (USB games don't need it as they use the game.ini to flag favorites)
static const char ADD_FAVORITE_COLUMN[] = "ALTER TABLE GAME ADD COLUMN FAVORITE INT DEFAULT 0";

// used by: updateFavorite for the internal.db (USB games don't need it as they use the game.ini to flag favorites)
static const char UPDATE_FAVORITE[] = "UPDATE GAME SET FAVORITE=? WHERE GAME_ID=?";

// used by: addHistoryColumn for the internal.db and regional.db
static const char ADD_HISTORY_COLUMN[] = "ALTER TABLE GAME ADD COLUMN HISTORY INT DEFAULT 0";

// used by: updateHistory for the internal.db and regional.db
static const char UPDATE_HISTORY[] = "UPDATE GAME SET HISTORY=? WHERE GAME_ID=?";

// used by: addLastPlayedColumn for the internal.db and regional.db
static const char ADD_LAST_PLAYED_COLUMN[] = "ALTER TABLE GAME ADD COLUMN LAST_PLAYED INT DEFAULT 0";

// used by: updateDatePlayed for the internal.db and regional.db
static const char UPDATE_LAST_PLAYED[] = "UPDATE GAME SET LAST_PLAYED=? WHERE GAME_ID=?";

// used by: addPlayUsingRAColumn for the internal.db (USB games don't need it as they use the game.ini to flag favorites)
static const char ADD_PLAY_USING_RA_COLUMN[] = "ALTER TABLE GAME ADD COLUMN PLAY_USING_RA INT DEFAULT 0";

// used by: addPlayUsingRAColumn for the internal.db (USB games don't need it as they use the game.ini to flag favorites)
static const char UPDATE_PLAY_USING_RA[] = "UPDATE GAME SET PLAY_USING_RA=? WHERE GAME_ID=?";

//*******************************
// ????.db
//*******************************

// used by: updateTitle
static const char UPDATE_TITLE[] = "UPDATE GAME SET GAME_TITLE_STRING=? WHERE GAME_ID=?";

// used by: getNumGames
static const char NUM_GAMES[] = "SELECT COUNT(*) as ctn FROM GAME";

// used by: createInitialDatabase
static const char CREATE_LANGUAGE_SPECIFIC_SQL[] = "CREATE TABLE IF NOT EXISTS LANGUAGE_SPECIFIC \
      ( [DEFAULT_VALUE] text, \
        [LANGUAGE_ID] integer, \
        [VALUE] text, \
           UNIQUE ([DEFAULT_VALUE], [LANGUAGE_ID]) )";

// used by: beginTransaction
static const char BEGIN_TRANSACTION[] = "BEGIN TRANSACTION";
// used by: commit
static const char COMMIT[] = "COMMIT";
// used by: rollback
static const char ROLLBACK[] = "ROLLBACK";

// used by: truncate
static const char DELETE_GAME_DATA[] = "DELETE FROM GAME";
static const char DELETE_DISC_DATA[] = "DELETE FROM DISC";
static const char DELETE_LANGUAGE_DATA[] = "DELETE FROM LANGUAGE_SPECIFIC";
static const char DELETE_SUBDIR_ROW_DATA[] = "DELETE FROM SUBDIR_ROWS";
static const char DELETE_SUBDIR_GAME_DATA[] = "DELETE FROM SUBDIR_GAMES_TO_DISPLAY_ON_ROW";

// used by: insertDisc
static const char INSERT_DISC[] = "INSERT INTO DISC ([GAME_ID],[DISC_NUMBER],[BASENAME]) \
                values (?,?,?)";

//*******************************
// DATABASE code
//*******************************

namespace {

//*******************************
// Stmt
//*******************************
// owns a prepared statement and finalizes it when it goes out of scope, whatever path the function takes
class Stmt {
public:
    Stmt(sqlite3 *db, const char *sql, const char *caller) {
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            cerr << "Failed: db::" << caller << ", " << sqlite3_errmsg(db) << endl;
            cerr << "  sql: " << sql << endl;
            stmt = nullptr;
        }
    }
    ~Stmt() { sqlite3_finalize(stmt); }   // finalize(nullptr) is a harmless no-op
    Stmt(const Stmt &) = delete;
    Stmt &operator=(const Stmt &) = delete;

    bool ok() const { return stmt != nullptr; }

    void bind(int index, int value) { sqlite3_bind_int(stmt, index, value); }
    void bind(int index, const string &value) { sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT); }

    int step() { return sqlite3_step(stmt); }
    bool row() { return step() == SQLITE_ROW; }

    int colInt(int col) { return sqlite3_column_int(stmt, col); }
    // a NULL column comes back as "" instead of constructing a std::string from a null pointer
    string colText(int col) {
        const unsigned char *text = sqlite3_column_text(stmt, col);
        return text ? string(reinterpret_cast<const char *>(text)) : string();
    }
    vector<char> colBlob(int col) {
        const void *bytes = sqlite3_column_blob(stmt, col);
        int size = sqlite3_column_bytes(stmt, col);
        if (bytes == nullptr || size <= 0)
            return vector<char>();
        const char *begin = static_cast<const char *>(bytes);
        return vector<char>(begin, begin + size);
    }

private:
    sqlite3_stmt *stmt = nullptr;
};

//*******************************
// readGameIni
//*******************************
// USB games keep some flags in Game.ini rather than in the database
void readGameIni(PsGamePtr &game) {
    string gameIniPath = game->folder + sep + GAME_INI;
    if (DirEntry::exists(gameIniPath)) {
        IniFile ini;
        ini.load(gameIniPath);
        game->locked =  !(ini.values["automation"]=="1");
        game->hd =       (ini.values["highres"]=="1");
        game->favorite = (ini.values["favorite"] == "1");
        game->play_using_ra = (ini.values["play_using_ra"] == "true");
    }
}

//*******************************
// readMetadataRow
//*******************************
// columns of SELECT_META and SELECT_TITLE: SERIAL, TITLE, PUBLISHER, RELEASE, PLAYERS, COVER
void readMetadataRow(Stmt &stmt, Metadata *md) {
    md->title = stmt.colText(1);
    md->publisher = stmt.colText(2);
    Util::cleanPublisherString(md->publisher);
    md->year = stmt.colInt(3);
    md->players = stmt.colInt(4);
    md->bytes = stmt.colBlob(5);
    md->valid = true;
}

//*******************************
// readInternalGameRow
//*******************************
// columns of GAMES_DATA_INTERNAL and GAMES_DATA_SINGLE_INTERNAL
void readInternalGameRow(Stmt &stmt, PsGamePtr &psGame) {
    int id = stmt.colInt(0);
    psGame->gameId = id;
    psGame->title = stmt.colText(1);
    psGame->publisher = stmt.colText(2);
    Util::cleanPublisherString(psGame->publisher);
    psGame->year = stmt.colInt(3);
    psGame->players = stmt.colInt(4);
    psGame->folder = "/gaadata/" + to_string(id) + "/";
    psGame->ssFolder = Env::getPathToSaveStatesDir() + sep + to_string(id) + "/";
    psGame->base = stmt.colText(5);
    psGame->serial = psGame->base;
    psGame->region = SerialScanner::serialToRegion(psGame->serial);
    psGame->memcard = "SONY";
    psGame->internal = true;
    psGame->cds = stmt.colInt(6);
    psGame->favorite = (stmt.colInt(7) != 0);
    psGame->play_using_ra = (stmt.colInt(8) != 0);
    psGame->history = stmt.colInt(9);
    psGame->last_played = stmt.colInt(10);
}

//*******************************
// readUSBGameRow
//*******************************
// columns of GAMES_DATA and GAMES_DATA_SINGLE
void readUSBGameRow(Stmt &stmt, PsGamePtr &game) {
    game->gameId = stmt.colInt(0);
    game->title = stmt.colText(1);
    game->publisher = stmt.colText(2);
    Util::cleanPublisherString(game->publisher);
    game->year = stmt.colInt(3);
    game->players = stmt.colInt(4);
    game->folder = stmt.colText(5);
    game->ssFolder = stmt.colText(6);
    game->memcard = stmt.colText(7);
    game->base = stmt.colText(8);
    game->history = stmt.colInt(9);
    game->last_played = stmt.colInt(10);
    game->cds = stmt.colInt(11);
    readGameIni(game);
}

} // namespace

//*******************************
// Database::~Database
//*******************************
Database::~Database() {
    disconnect();
}

//*******************************
// Database::getNumGames
//*******************************
int Database::getNumGames() {
    Stmt stmt(db, NUM_GAMES, "getNumGames");
    if (stmt.ok() && stmt.row()) {
        return stmt.colInt(0);
    }
    return 0;
}

//*******************************
// Database::updateYear
// called by VerMigration::migrate04_05()
//*******************************
bool Database::updateYear(int id, int year) {
    Stmt stmt(db, UPDATE_YEAR, "updateYear");
    if (!stmt.ok()) return false;
    stmt.bind(1, year);
    stmt.bind(2, id);
    return stmt.step() == SQLITE_DONE;
}

//*******************************
// Database::updateMemcard
//*******************************
bool Database::updateMemcard(int id, string memcard) {
    Stmt stmt(db, UPDATE_MEMCARD, "updateMemcard");
    if (!stmt.ok()) return false;
    stmt.bind(1, memcard);
    stmt.bind(2, id);
    return stmt.step() == SQLITE_DONE;
}

//*******************************
// Database::updateTitle
//*******************************
bool Database::updateTitle(int id, string title) {
    Stmt stmt(db, UPDATE_TITLE, "updateTitle");
    if (!stmt.ok()) return false;
    stmt.bind(1, title);
    stmt.bind(2, id);
    return stmt.step() == SQLITE_DONE;
}

//*******************************
// Database::updateFavorite
//*******************************
bool Database::updateFavorite(int id, int favorite) {
    Stmt stmt(db, UPDATE_FAVORITE, "updateFavorite");
    if (!stmt.ok()) return false;
    stmt.bind(1, favorite);
    stmt.bind(2, id);
    return stmt.step() == SQLITE_DONE;
}

//*******************************
// Database::updatePlayUsingRA
//*******************************
bool Database::updatePlayUsingRA(int id, int play_using_ra) {
    Stmt stmt(db, UPDATE_PLAY_USING_RA, "updatePlayUsingRA");
    if (!stmt.ok()) return false;
    stmt.bind(1, play_using_ra);
    stmt.bind(2, id);
    return stmt.step() == SQLITE_DONE;
}

//*******************************
// Database::updateHistory
// 0 = not in history, 1-100 history from latest game played to oldest
//*******************************
bool Database::updateHistory(int id, int rank) {
    Stmt stmt(db, UPDATE_HISTORY, "updateHistory");
    if (!stmt.ok()) return false;
    stmt.bind(1, rank);
    stmt.bind(2, id);
    return stmt.step() == SQLITE_DONE;
}

//*******************************
// Database::updateDatePlayed
// seconds since 1970
//*******************************
bool Database::updateDatePlayed(int id, int date_in_seconds) {
    Stmt stmt(db, UPDATE_LAST_PLAYED, "updateDatePlayed");
    if (!stmt.ok()) return false;
    stmt.bind(1, date_in_seconds);
    stmt.bind(2, id);
    return stmt.step() == SQLITE_DONE;
}

//*******************************
// Database::queryTitle
//*******************************
bool Database::queryTitle(string title, Metadata *md) {
    Stmt stmt(db, SELECT_TITLE, "queryTitle");
    if (!stmt.ok()) return false;
    stmt.bind(1, title);
    if (stmt.row()) {
        readMetadataRow(stmt, md);
        return true;
    }
    return false;
}

//*******************************
// Database::querySerial
//*******************************
bool Database::querySerial(string serial, Metadata *md) {
    string serialLike = serial + "-%";
    Stmt stmt(db, SELECT_META, "querySerial");
    if (!stmt.ok()) return false;
    stmt.bind(1, serial);
    stmt.bind(2, serialLike);
    if (stmt.row()) {
        readMetadataRow(stmt, md);
        md->serial = serial;
        md->region = SerialScanner::serialToRegion(md->serial);
        //cout << "querySerial: " << "serial " << serial << ", " << md->title << endl;
        return true;
    }
    return false;
}

//*******************************
// Database::getInternalGames
//*******************************
bool Database::getInternalGames(PsGames *result) {
    result->clear();
    Stmt stmt(db, GAMES_DATA_INTERNAL, "getInternalGames");
    if (!stmt.ok()) return false;
    while (stmt.row()) {
        PsGamePtr psGame{new PsGame};
        readInternalGameRow(stmt, psGame);
        result->push_back(psGame);
        //cout << "getInternalGames: " << game->serial << ", " << game->title << endl;
    }
    return true;
}

//*******************************
// Database::refreshGameInternal
//*******************************
bool Database::refreshGameInternal(PsGamePtr &psGame) {
    Stmt stmt(db, GAMES_DATA_SINGLE_INTERNAL, "refreshGameInternal");
    if (!stmt.ok()) return false;
    stmt.bind(1, psGame->gameId);
    while (stmt.row()) {
        readInternalGameRow(stmt, psGame);
        readGameIni(psGame);
    }
    return true;
}

//*******************************
// Database::refreshGame
//*******************************
bool Database::refreshGame(PsGamePtr &game) {
    Stmt stmt(db, GAMES_DATA_SINGLE, "refreshGame");
    if (!stmt.ok()) return false;
    stmt.bind(1, game->gameId);
    while (stmt.row()) {
        readUSBGameRow(stmt, game);
    }
    return true;
}

//*******************************
// Database::getGames
//*******************************
bool Database::getGames(PsGames *result) {
    result->clear();
    Stmt stmt(db, GAMES_DATA, "getGames");
    if (!stmt.ok()) return false;
    while (stmt.row()) {
        PsGamePtr game{new PsGame};
        readUSBGameRow(stmt, game);
        //cout << "getGames: " << game->serial << ", " << game->title << endl;
        result->push_back(game);
    }
    return true;
}

//*******************************
// Database::getGameRowInfos
//*******************************
bool Database::getGameRowInfos(GameRowInfos *gameRowInfos) {
    Stmt stmt(db, GET_SUBDIR_ROW, "getGameRowInfos");
    if (!stmt.ok()) return false;
    while (stmt.row()) {
        SubDirRowInfo subDirRowInfo;
        subDirRowInfo.subDirRowIndex = stmt.colInt(0);
        subDirRowInfo.rowName = stmt.colText(1);
        subDirRowInfo.indentLevel = stmt.colInt(2);
        subDirRowInfo.numGames = stmt.colInt(3);

        cout << "SubDirRowInfo: " << string(subDirRowInfo.indentLevel * 2, ' ') << subDirRowInfo.rowName
                << ", index: " << subDirRowInfo.subDirRowIndex
                << ", indent: " << subDirRowInfo.indentLevel
                << ", numGames: " << subDirRowInfo.numGames << endl;

        gameRowInfos->emplace_back(subDirRowInfo);
    }
    return true;
}

//*******************************
// Database::getGameRowGameInfos
//*******************************
bool Database::getGameRowGameInfos(GameRowGames *gameRowGames) {
    Stmt stmt(db, GET_SUBDIR_GAME, "getGameRowGameInfos");
    if (!stmt.ok()) return false;
    while (stmt.row()) {
        GameRowGame gameRowGame;
        gameRowGame.rowIndex = stmt.colInt(0);
        gameRowGame.gameId = stmt.colInt(1);

        cout << "GameRowGame: " << gameRowGame.rowIndex << ", " << gameRowGame.gameId << endl;

        gameRowGames->emplace_back(gameRowGame);
    }
    return true;
}

//*******************************
// Database::getGameIdsInRow
//*******************************
bool Database::getGameIdsInRow(vector<int> *gameIdsInRow, int row) {
    Stmt stmt(db, GET_SUBDIR_GAME_ON_ROW, "getGameIdsInRow");
    if (!stmt.ok()) return false;
    stmt.bind(1, row);
    while (stmt.row()) {
        //cout << "GameId in Row: " << row << ", " << gameId << endl;
        gameIdsInRow->emplace_back(stmt.colInt(0));
    }
    return true;
}

//*******************************
// Database::insertDisc
//*******************************
bool Database::insertDisc(int id, int discNum, string discName) {
    Stmt stmt(db, INSERT_DISC, "insertDisc");
    if (!stmt.ok()) return false;
    stmt.bind(1, id);
    stmt.bind(2, discNum);
    stmt.bind(3, discName);
    return stmt.step() == SQLITE_DONE;
}

//*******************************
// Database::insertGame
//*******************************
bool Database::insertGame(int id, string title, string publisher, int players, int year, string path, string sspath,
                          string memcard) {
    Util::cleanPublisherString(publisher);
    Stmt stmt(db, INSERT_GAME, "insertGame");
    if (!stmt.ok()) return false;
    stmt.bind(1, id);
    stmt.bind(2, title);
    stmt.bind(3, publisher);
    stmt.bind(4, year);
    stmt.bind(5, players);
    stmt.bind(6, path);
    stmt.bind(7, sspath);
    stmt.bind(8, memcard);
    return stmt.step() == SQLITE_DONE;
}

//*******************************
// Database::subDirRowsTableIsEmpty
// returns true if no rows in table or failure
// *******************************
bool Database::subDirRowsTableIsEmpty() {
    Stmt stmt(db, IS_SUBDIR_ROWS_TABLE_EMPTY, "subDirRowsTableIsEmpty");
    if (stmt.ok() && stmt.row()) {
        return stmt.colInt(0) == 0;   // true if no rows in table
    }
    return true;
}

//*******************************
// Database::insertSubDirRow
//*******************************
bool Database::insertSubDirRow(int rowIndex, string rowName, int indentLevel, int numGames) {
    Stmt stmt(db, INSERT_SUBDIR_ROW, "insertSubDirRow");
    if (!stmt.ok()) return false;
    stmt.bind(1, rowIndex);
    stmt.bind(2, rowName);
    stmt.bind(3, indentLevel);
    stmt.bind(4, numGames);
    return stmt.step() == SQLITE_DONE;
}

//*******************************
// Database::insertSubDirGames
//*******************************
bool Database::insertSubDirGames(int rowIndex, int gameId) {
    Stmt stmt(db, INSERT_SUBDIR_GAME, "insertSubDirGames");
    if (!stmt.ok()) return false;
    stmt.bind(1, rowIndex);
    stmt.bind(2, gameId);
    return stmt.step() == SQLITE_DONE;
}

//*******************************
// Database::executeCreateStatement
//*******************************
bool Database::executeCreateStatement(const char *sql, const string &name) {
    char *errorReport = nullptr;
    cout << "Creating " << name << " (if not exists)" << endl;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errorReport);
    if (rc != SQLITE_OK) {
        cerr << "Failed: db:: executeCreateStatement, " << sql << ", " << name << endl;
        cerr << "Failed to create " << name << "  table/column  " << (errorReport ? errorReport : sqlite3_errmsg(db)) << endl;
        sqlite3_free(errorReport);
        return false;
    }
    return true;
}

//*******************************
// Database::executeStatement
//*******************************
bool Database::executeStatement(const char *sql, const string &outMsg, const string &errorMsg) {
    char *errorReport = nullptr;
    cout << outMsg << endl;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errorReport);
    if (rc != SQLITE_OK) {
        cerr << "Failed: db:: executeStatement, " << sql << ", " << outMsg<< ", " << errorMsg << endl;
        cerr << errorMsg << (errorReport ? errorReport : sqlite3_errmsg(db)) << endl;
        sqlite3_free(errorReport);
        return false;
    }
    return true;
}

//*******************************
// Database::connect
//*******************************
bool Database::connect(string fileName) {
    disconnect();   // in case connect is called twice
    int rc = sqlite3_open(fileName.c_str(), &db);
    if (rc != SQLITE_OK) {
        cerr << "Failed: db:: connect, " << fileName << endl;
        cout << "Cannot open database: " << (db ? sqlite3_errmsg(db) : "out of memory") << endl;
        sqlite3_close(db);  // sqlite3_open allocates a handle even on failure
        db = nullptr;
        return false;
    }
    cout << "Connected to DB " << fileName << endl;
    return true;
}

//*******************************
// Database::disconnect
//*******************************
void Database::disconnect() {
    if (db != nullptr) {
        cout << "Disconnecting DBs" << endl;
        sqlite3_db_cacheflush(db);
        sqlite3_close(db);
        db = nullptr;
    }
}

//*******************************
// Database::beginTransaction
//*******************************
bool Database::beginTransaction() {
    return executeStatement(BEGIN_TRANSACTION, "Begin Transaction", "Error beginning  transaction");
}

//*******************************
// Database::commit
//*******************************
bool Database::commit() {
    return executeStatement(COMMIT, "Commit", "Error on commit");
}

//*******************************
// Database::rollback
//*******************************
bool Database::rollback() {
    return executeStatement(ROLLBACK, "Rollback", "Error on rollback");
}

//*******************************
// Database::truncate
//*******************************
bool Database::truncate() {
    bool ok = true;
    ok &= executeStatement(DELETE_GAME_DATA, "Truncating all data", "Error truncating data");
    ok &= executeStatement(DELETE_DISC_DATA, "Truncating all data", "Error truncating data");
    ok &= executeStatement(DELETE_LANGUAGE_DATA, "Truncating all data", "Error truncating data");
    ok &= executeStatement(DELETE_SUBDIR_ROW_DATA, "Truncating all data", "Error truncating data");
    ok &= executeStatement(DELETE_SUBDIR_GAME_DATA, "Truncating all data", "Error truncating data");
    return ok;
}

//*******************************
// Database::createInitialDatabase
//*******************************
bool Database::createInitialDatabase() {
    if (!executeCreateStatement(CREATE_GAME_SQL, "GAME")) return false;
    executeCreateStatement(ADD_HISTORY_COLUMN, "History column" ); // add column to existing table
    executeCreateStatement(ADD_LAST_PLAYED_COLUMN, "Last_Played column" ); // add column to existing table
    if (!executeCreateStatement(CREATE_DISC_SQL, "DISC")) return false;
    if (!executeCreateStatement(CREATE_LANGUAGE_SPECIFIC_SQL, "LANGUAGE_SPECIFIC")) return false;
    if (!executeCreateStatement(CREATE_SUBDIR_ROW_SQL, "SUBDIR_ROWS")) return false;
    if (!executeCreateStatement(CREATE_SUBDIR_GAMES_TO_DISPLAY_ON_ROW_SQL, "SUBDIR_GAMES_TO_DISPLAY_ON_ROW")) return false;

    return true;
}

//*******************************
// Database::addFavoriteColumn
//*******************************
void Database::addFavoriteColumn() {
    executeCreateStatement(ADD_FAVORITE_COLUMN, "Favorite column" );
}

//*******************************
// Database::addPlayUsingRAColumn
//*******************************
void Database::addPlayUsingRAColumn() {
    executeCreateStatement(ADD_PLAY_USING_RA_COLUMN, "Play Using RA column" );
}

//*******************************
// Database::addHistoryColumn
//*******************************
void Database::addHistoryColumn() {
    executeCreateStatement(ADD_HISTORY_COLUMN, "History column" );
}

//*******************************
// Database::addLastPlayedColumn
//*******************************
void Database::addLastPlayedColumn() {
    executeCreateStatement(ADD_LAST_PLAYED_COLUMN, "Last_Played column" );
}

//*******************************
// Database::deleteGameIdFromOneTable
//*******************************
bool Database::deleteGameIdFromOneTable(int id, const string& cmd_str) {
    Stmt stmt(db, cmd_str.c_str(), "deleteGameIdFromOneTable");
    if (!stmt.ok()) return false;
    stmt.bind(1, id);
    return stmt.step() == SQLITE_DONE;
}

//*******************************
// Database::deleteGameIdFromAllTables
//*******************************
bool Database::deleteGameIdFromAllTables(int id) {
    if (!beginTransaction()) return false;   // all the statements must succeed or the DB won't be modified

    bool success = deleteGameIdFromOneTable(id, DELETE_GAME_ID_FROM_DISC)
                && deleteGameIdFromOneTable(id, DELETE_GAME_ID_FROM_GAME)
                && deleteGameIdFromOneTable(id, DELETE_GAME_ID_FROM_SUBDIR_GAMES_TO_DISPLAY_ON_ROW);

    if (success) {
        commit();
    } else {
        rollback();     // otherwise the transaction stays open and every later write would pile into it
    }

    return success;
}
