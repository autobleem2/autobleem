//
// Created by screemer on 2/2/19.
//

#include "coverdb.h"
#include "../util.h"
#include <iostream>
#include "../DirEntry.h"
#include "../environment.h"

using namespace std;

//*******************************
// Coverdb::Coverdb()
//*******************************
Coverdb::Coverdb()
{
    regionStr[0] = "U";
    regionStr[1] = "P";
    regionStr[2] = "J";

    for (int i=0;i<3;i++)
    {
        auto filename = Env::getPathToCoversDBDir() + sep + "covers" + regionStr[i] + ".db";
        if (DirEntry::exists(filename)) {
                covers[i].reset(new Database());
                if (!covers[i]->connect(filename)) {
                    cout << "failed to open database " << filename << endl;
                    covers[i].reset();
                }
        }
        else {
            cout << "database file " << filename << " not found" << endl;
        }
    }
}

//*******************************
// Coverdb::~Coverdb()
//*******************************
Coverdb::~Coverdb()
{
    // the unique_ptrs disconnect and delete the databases
}

//*******************************
// Coverdb::isValid
//*******************************
bool Coverdb::isValid()
{
    bool valid = false;
    for (const auto &db : covers)
    {
        if (db!= nullptr) valid = true;
    }
    return valid;
}