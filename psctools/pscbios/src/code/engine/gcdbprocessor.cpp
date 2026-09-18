//
// Created by screemer on 04.05.2020.
//

#include <string>
#include <iostream>
#include <fstream>
#include <SDL2/SDL_config.h>
#include "gcdbprocessor.h"
#include "../main.h"

using namespace std;

void gcdbprocessor::readFile(string path) {
    cout << "Reading GamecontrollerDB " << path << endl;
    if (!lines.empty()) {
        for (int i = 0; i < lines.size(); i++) delete lines[i];
        lines.clear();
    }
    ifstream is(path);

    std::string line;
    while (std::getline(is, line)) {
        line = trim(line);
        cout << line << endl;
        if (line.empty()) continue;
        gcdbline *lineData = new gcdbline;
        lineData->line = line;
        if (line[0] == '#') lineData->isComment = true; else lineData->isComment = false;
        if (lineData->line == "#AutoBleem") autobleemSectionFound = true;
        if (lineData->isComment) {
            lines.emplace_back(lineData);
            continue;
        };
        // this is real mapping find guid and platform
        string guid = line.substr(0, line.find(',', 0));
        lineData->guid = guid;
        int platformpos = line.find("platform:");
        if (platformpos == line.npos) {
            // something weird no platform
            lines.emplace_back(lineData);
            continue;
        }
        int endplatform = line.find(",", platformpos);
        string platform = line.substr(platformpos + 9, endplatform - platformpos - 9);
        cout << "GUID:" << guid << " Platform:" << platform << endl;
        lineData->platform = platform;
        lineData->isAutoBleem = autobleemSectionFound;
        lines.emplace_back(lineData);

    }
    if (!autobleemSectionFound)
    {
        gcdbline *lineData = new gcdbline;
        lineData->line = "#AutoBleem";
        lines.emplace_back(lineData);
    }
    cout << "Total Mappings" << lines.size() << endl;
    is.close();
}

void gcdbprocessor::replaceMapping(string guid, string mapping) {
    string myplatform = SDL_GetPlatform();
    bool replaced = false;
    for (int i = 0; i < lines.size(); i++) {
        if ((lines[i]->platform == myplatform) && (lines[i]->guid == guid)) {
            lines[i]->line = mapping;
            replaced = true;
        }
    }
    if (!replaced) {
        gcdbline *lineData = new gcdbline;
        lineData->line = mapping;
        lineData->guid = guid;
        lineData->platform = myplatform;
        lineData->isAutoBleem = true;
        lines.emplace_back(lineData);
    }
}

void gcdbprocessor::saveFile(string path) {
    ofstream os(path);
    for (int i=0;i<lines.size();i++)
    {
        os << lines[i]->line << endl;
    }
    os.flush();
    os.close();
}

void gcdbprocessor::free() {
    for (int i = 0; i < lines.size(); i++) {
        delete lines[i];
    }
    lines.clear();
}