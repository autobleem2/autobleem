//
// Created by screemer on 2/2/19.
//
#pragma once

#include "database.h"
#include <memory>

//******************
// Coverdb
//******************
class Coverdb {
public:
    std::unique_ptr<Database> covers[3];    // U, P, J. nullptr when that region's covers db is not installed
    std::string regionStr[3];

    bool isValid();
    Coverdb();
    ~Coverdb();

};

