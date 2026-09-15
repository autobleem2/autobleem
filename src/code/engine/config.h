//
// Created by screemer on 23.12.18.
//
#pragma once

#include "../main.h"

//******************
// Config
//******************
class Config {
public:
    IniFile inifile;
    Config();
    void save();
};
