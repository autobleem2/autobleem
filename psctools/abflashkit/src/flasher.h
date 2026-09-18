//
// Created by screemer on 2020-03-02.
//

#ifndef ABFLASHKIT_FLASHER_H
#define ABFLASHKIT_FLASHER_H

#include <string>
#include <vector>

using namespace std;

class FileTag
{
public:
    string path;
    string name;
    FileTag(string path,string name)
    {
        this->path = path;
        this->name = name;
    }
} ;

class flasher {
public:
    void unzipFiles(string inputZip, string outputPath);
    void zipFiles(vector<FileTag> files, string outputPath);
    void addSignature(string outputPath);
    bool removeSignature(string outputPath);
    bool validateSignature(string outputPath);
    bool isAutoBleemBackup(string outputPath);
    string execUnixCommand(const char* cmd);
    bool exists(const string &_name);
    void setRecoveryMode(bool mode);
    bool validateKernel();
    void flashKernel();
};


#endif //ABFLASHKIT_FLASHER_H
