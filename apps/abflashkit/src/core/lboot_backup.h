//
// LbootBackup: what an LBOOT.EPB is - the console's partitions zipped, with Sony's recovery trailer on the
// end - and the checks the tool makes on one. The file is what the stock recovery restores from a USB
// stick when the bootloader's recovery flag is set.
//
#pragma once

#include <string>
#include <vector>

//******************
// LbootBackup
//******************
class LbootBackup {
public:
    struct Partition {
        std::string device; // /dev/disk/by-partlabel/<name>
        std::string entry;  // the file name inside the zip
    };
    // the partitions a flash backs up before touching anything (no rootfs: AutoBleem does not alter it,
    // per madmonkey), and the full set the Full backup writes
    static std::vector<Partition> partitionsForFlash();
    static std::vector<Partition> partitionsForFullBackup();

    // the trailer appended after the zip's end; false when the file cannot be written
    static bool appendSignature(const std::string &path);
    // the file ends in the trailer's last nine bytes ("autobleem")
    static bool isAutoBleemBackup(const std::string &path);

    // the md5s of the stock firmware's images, what a restore expects to find inside a backup
    static const char *const VanillaBootMd5;   // boot.img
    static const char *const VanillaRootfsMd5; // rootfs.ext4

    struct Contents {
        bool hasBoot = false;
        bool bootIsVanilla = false;
        bool hasRootfs = false; // an ABFK 1.0a backup has none
        bool rootfsIsVanilla = false;
        bool isVanilla() const { return bootIsVanilla && (!hasRootfs || rootfsIsVanilla); }
    };
    // what an extracted backup holds, judged by those md5s
    static Contents inspect(const std::string &extractedDir);
};
