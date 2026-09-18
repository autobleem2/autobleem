//
// Created by screemer on 2020-03-02.
//

#include <iostream>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>

#include "flasher.h"
#include "../zip/zip.h"
#include "gui/gui.h"
#include "rawc.h"

#include "lang.h"

#define KERNEL_IMG "kernel/boot.img"
#define KERNEL_SUM "kernel/boot.md5"
#define REC_IMG    "kernel/recovery-"
#define MISC_PART "/dev/disk/by-partlabel/MISC"
#define BOOT_PART "/dev/disk/by-partlabel/BOOTIMG1"

void flasher::unzipFiles(string inputZip, string outputPath)
{
    cout << "Inflating files from " << outputPath << endl;
    struct zip_t *zip = zip_open(inputZip.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'r');
    int filesToExtract = zip_total_entries(zip);
    for (int i=0;i<filesToExtract;i++)
    {
        zip_entry_openbyindex(zip,i);
        {
            string filename = zip_entry_name(zip);

                zip_entry_fread(zip, string(outputPath + "/" + zip_entry_name(zip)).c_str());

        }
        zip_entry_close(zip);
    }

    zip_close(zip);
    cout << "Done" << endl;
}
void flasher::zipFiles(vector<FileTag> files, string outputPath) {
    shared_ptr<Gui> gui(Gui::getInstance());
    cout << "Zipping files to " << outputPath << endl;
    struct zip_t *zip = zip_open(outputPath.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
    for (FileTag file:files) {
        gui->drawText("Zipping:" + file.name + "   Please wait...");
        zip_entry_open(zip, file.name.c_str());
        {
            zip_entry_fwrite(zip, file.path.c_str());
        }
        zip_entry_close(zip);
    }
    zip_close(zip);
    cout << "Done" << endl;
}

void flasher::addSignature(string outputPath) {
    std::ofstream outfile;
    outfile.open(outputPath, std::ios_base::app);
    for (int i = 0; i < rawc_len; i++) {
        outfile << rawc[i];
    }
    outfile.flush();
    outfile.close();
}

bool flasher::removeSignature(string outputPath) {

    int rawcpos = -1;
    int startpos = 0;

    std::fstream inf(outputPath, std::ios::binary | std::ios::in | std::ios::out);

    inf.seekg(-10000, ios::end);
    startpos = inf.tellg();

    char rawc[5];
    rawc[4] = 0;
    while (inf.readsome(rawc, 4)) {
        if (string("RAWC") == rawc) {
            rawcpos = startpos;
            break;
        }
        startpos++;
        inf.seekg(startpos);
    }
    inf.seekg(0, ios::end);
    int fileSize = inf.tellg();
    if (rawcpos != -1) {
        int bytesToCut = fileSize - rawcpos;
        cout << "RAWC Found: " << rawcpos << "   FileSize:" << fileSize << "   To Remove: " << bytesToCut << endl;

        inf.close();
        FILE *f = fopen(outputPath.c_str(), "rb+");
        ftruncate(fileno(f), rawcpos);
        fflush(f);
        fclose(f);
        addSignature(outputPath);
        return true;
    } else {
        inf.close();
        return false;
    }


}

bool flasher::isAutoBleemBackup(string outputPath) {
    string image = outputPath;
    std::ifstream inf{image};

    inf.seekg(-9, ios::end);
    char signatureTxt[10];
    signatureTxt[9] = 0;
    inf.readsome(signatureTxt, 9);

    inf.close();
    string s = signatureTxt;
    return s == "autobleem";
}


bool flasher::validateSignature(string outputPath) {
    string command = "image_verify_tool " + outputPath + " /tmp/validateResult.txt";

    shared_ptr<Gui> gui(Gui::getInstance());
    gui->drawText("Validating backup...   Please wait...");
    string result = execUnixCommand(command.c_str());
    if (result.find("fail") != string::npos) {
        gui->drawText("Validating backup...   Invalid...");
        return false;
    }
    gui->drawText("Validating backup...   Valid...");
    return true;
}

string flasher::execUnixCommand(const char *cmd) {
    array<char, 2048> buffer;
    string result;
    cout << "Exec:" << cmd << endl;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    if (!result.empty()) {
        result.erase(remove(result.begin(), result.end(), '\n'));
    }
    return result;
}

bool flasher::exists(const string &_name) {
    auto name = _name;
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

void flasher::setRecoveryMode(bool mode) {
    string modeName = mode ? string("on") : string("off");
    string command = string("dd if=") + REC_IMG + modeName + ".img of=" + MISC_PART;

    shared_ptr<Gui> gui(Gui::getInstance());
    gui->drawText("Setting recovery mode mode: " + modeName);
    execUnixCommand(command.c_str());
}

bool flasher::validateKernel() {
    // Check if both image and md5 exists
    if (!exists(KERNEL_IMG)) {
        return false;
    }
    if (!exists(KERNEL_SUM)) {
        return false;
    }

    string md5calculated = execUnixCommand((string("md5sum ") + KERNEL_IMG).c_str()).substr(0, 32);
    string md5inFile = execUnixCommand((string("cat ") + KERNEL_SUM).c_str()).substr(0, 32);

    if (md5calculated != md5inFile) {
        shared_ptr<Gui> gui(Gui::getInstance());
        SDL_Delay(3000);
        return false;
    }

    return true;
}

void flasher::flashKernel() {
    string command = string("dd if=") + KERNEL_IMG + " of=" + BOOT_PART;

    shared_ptr<Gui> gui(Gui::getInstance());
    gui->drawText(_("Flashing KERNEL IMAGE"));
    execUnixCommand(command.c_str());
    gui->drawText(_("Kernel flashed"));
}