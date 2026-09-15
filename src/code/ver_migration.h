//
// Created by screemer on 2019-01-27.
//

#pragma once

#include <string>
#include "main.h"

#if 0   // no longer used
//******************
// VerMigration
//******************
class VerMigration {
public:
    std::string getLastRunVersion();
    void migrate04_05(GameDatabase * db);
    void migrate(GameDatabase * db);
};
#endif
