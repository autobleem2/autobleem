//
// Created by screemer on 2018-12-16.
//
#pragma once
#include "../main.h"
#include <vector>

//******************
// Metadata
//******************
class Metadata {
public:
    std::string title;
    std::string publisher;
    int year = 0;
    std::string serial;
    std::string region;
    int players = 0;
    std::vector<char> bytes;    // the cover PNG, empty if none
    bool valid = false;

    std::string lastRegion="U";

    bool lookupBySerial(const std::string & serial);
    bool lookupByTitle(const std::string & title);
    void clean() { bytes.clear(); }
};
