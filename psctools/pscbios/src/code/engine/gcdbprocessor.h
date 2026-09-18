//
// Created by screemer on 04.05.2020.
//

#pragma once

#include <string>
#include <vector>

using namespace std;
struct gcdbline {
    string line="";
    string guid="";
    string platform="";
    bool isComment=false;
    bool isAutoBleem=false;
};

class gcdbprocessor {
    bool autobleemSectionFound = false;
    vector<gcdbline *> lines;
public:
    void readFile(string path);
    void replaceMapping(string guid, string mapping);
    void saveFile(string path);
    void free();
};

