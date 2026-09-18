#include <iostream>

#include "src/gui/gui.h"
#include "src/flasher.h"
#include "src/led.h"
#include "src/gui/gui_confirm.h"
#include "src/lang.h"
#include "src/environment.h"
#include "src/DirEntry.h"
#include "src/gui/abl.h"

#define  SEL_QUIT    0
#define  SEL_FLASH   1
#define  SEL_RESTORE 2
#define  SEL_BACKUP  3

#define VER_ABFK "1.0b"

int selection = 0;

void waitForJoy() {
    shared_ptr<Gui> gui(Gui::getInstance());
    bool ret = true;
    while (ret) {
        gui->drawText(_("ABFlashKit") + VER_ABFK + "  |@X| "+_("Flash Kernel")+"  |@S| "+_("Full backup")+"  |@T| "+_("Restore Mode")+"  |@O|  "+_("Quit")+"|");
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            gui->mapper.handleHotPlug(&e);

            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.scancode == SDL_SCANCODE_SLEEP) {
                    gui->drawText(_("Exiting..."));
                    selection = SEL_QUIT;
                    ret = false;
                }
            }

            if (e.type == SDL_QUIT) {
                ret = false;
            }
            switch (e.type) {
                case SDL_CONTROLLERBUTTONDOWN:
                    if (e.cbutton.button == SDL_BTN_CIRCLE) {
                        selection = SEL_QUIT;
                        ret = false;

                    };
                    if (e.cbutton.button == SDL_BTN_CROSS) {
                        selection = SEL_FLASH;
                        ret = false;

                    };

                    if (e.jbutton.button == SDL_BTN_TRIANGLE) {
                        selection = SEL_RESTORE;
                        ret = false;

                    };

                    if (e.jbutton.button == SDL_BTN_SQUARE) {
                        selection = SEL_BACKUP;
                        ret = false;

                    };


            }

        }
    }
}

extern bool private_singleArgPassed;
extern string   private_pathToUSBDrive;

int main(int argc, char *argv[]) {
#if defined(__x86_64__) || defined(_M_X64)
    if (argc >= 1+1) {
        private_pathToUSBDrive = argv[1];
    }
#else
    private_pathToUSBDrive = "/media";
#endif
    Env::autobleemKernel = DirEntry::exists("/autobleem");
    shared_ptr<Lang> lang(Lang::getInstance());
    shared_ptr<Gui> gui(Gui::getInstance());




    gui->mapper.init();
    gui->mapper.probePads();
    lang->load(gui->cfg.inifile.values["language"]);

    led *Led = new led();
    Led->init();
    Led->setMode(LED_GREEN);
    gui->init();
    gui->drawText(_("Welcome to ABFlashKit"));
    SDL_Delay(2000);


    bool loopme = true;
    while (loopme) {
        waitForJoy();
        if (selection == SEL_FLASH) {
            // validate if bleemsync of eris is on
            flasher *flash = new flasher();
            vector<string> filesToCheck;
            filesToCheck.push_back("/usr/bin/bleemsync_service");
            filesToCheck.push_back("/etc/systemd/system/project_eris.service");
            bool found = false;
            for (string file:filesToCheck)
            {
                if (flash->exists(file))
                {
                    found = true;
                    break;
                }
            }

            if (found)
            {
                gui->drawText(_("This PSC has not been restored to original condition. Unable to flash."));
                SDL_Delay(4000);
                loopme = true;
                continue;
            }

            GuiConfirm *confirm = new GuiConfirm(gui->renderer);
            confirm->label = _("Start flashing ?");
            confirm->show();
            bool result = confirm->result;
            delete (confirm);

            if (!result)
            {
                loopme = true;
                continue;
            }



            Led->setMode(BLINK_GREEN);
            gui->drawText(_("Creating backup...."));
            // zip files

            vector<FileTag> files;
            files.push_back(FileTag("/dev/disk/by-partlabel/BOOTIMG1", "boot.img"));

            // Advice from madmonkey - do not backup root as AB will not alter it
            // files.push_back(FileTag("/dev/disk/by-partlabel/ROOTFS1","rootfs.ext4"));
            files.push_back(FileTag("/dev/disk/by-partlabel/USRDATA", "userdata.ext4"));
            files.push_back(FileTag("/dev/disk/by-partlabel/TEE1", "tz.img"));

            if (!flash->exists("/media/LBOOT.EPB")) {
                flash->zipFiles(files, "/media/LBOOT.EPB");
                flash->addSignature("/media/LBOOT.EPB");
            }
            flash->execUnixCommand("sync");

            if (flash->validateSignature("/media/LBOOT.EPB") && flash->validateKernel()) {
                Led->setMode(LED_RED);
                SDL_Delay(2000);
                flash->setRecoveryMode(true);
                flash->flashKernel();
                gui->drawText(_("Updating payload"));
                flash->execUnixCommand("bash ./kernel/install_payload.sh");
                Led->setMode(LED_GREEN);
                flash->setRecoveryMode(false);
                SDL_Delay(2000);

                gui->drawText(_("All done - when the screen goes black replace power cord"));
            } else {
                gui->drawText(_("Invalid backup or invalid kernel image"));
            }

            SDL_Delay(3000);
            Led->setMode(LED_OFF);
            SDL_Delay(100);
            flash->execUnixCommand("killall -9 autobleem-gui");
            flash->execUnixCommand("systemctl stop weston");
            flash->execUnixCommand("systemctl reboot");
            delete flash;
            loopme=false;
        }

        if (selection == SEL_RESTORE) {


            flasher *flash = new flasher();
            if (flash->exists("/media/LBOOT.EPB")) {
                // check autobleem
                bool isAutobleemLBOOT = flash->isAutoBleemBackup("/media/LBOOT.EPB");

                bool canRestore = false;
                if (!isAutobleemLBOOT) {
                    gui->drawText(_("Non AutoBleem Backup found. Please use valid AB backup"));
                    SDL_Delay(3000);
                    canRestore = false;
                } else {
                    canRestore = true;
                }

                if (!flash->exists("/media/validlboot")) {
                    // Check md5 of boot.img in backup
                    gui->drawText(_("Checking LBOOT for vanilla kernel. Decompressing..."));
                    SDL_Delay(1000);
                    flash->execUnixCommand("rm -rf /tmp/lbootzip");
                    flash->execUnixCommand("mkdir -p /tmp/lbootzip");
                    flash->unzipFiles("/media/LBOOT.EPB", "/tmp/lbootzip");

                    if (flash->exists("/tmp/lbootzip/boot.img")) {
                        bool isVanillaData = true;
                        bool isRootlessBackup = false;
                        string md5boot = flash->execUnixCommand(
                                (string("md5sum ") + "/tmp/lbootzip/boot.img").c_str()).substr(0, 32);
                        string vanillaboot = "28ce5f6de4981764411393310311b517";
                        if (md5boot!=vanillaboot) isVanillaData = false;

                        if (flash->exists("/tmp/lbootzip/rootfs.ext4"))
                        {
                            string md5root = flash->execUnixCommand(
                                    (string("md5sum ") + "/tmp/lbootzip/rootfs.ext4").c_str()).substr(0, 32);

                            string vanillaroot = "ca710a128b7da4a23ace840c9d16e745";
                            cout << "MD5 of rootfs:  " << md5root << "/" << vanillaroot << endl;
                            if (vanillaroot!=md5root) isVanillaData = false;
                        } else isRootlessBackup = true;
                        cout << "MD5 of kernel:  " << md5boot << "/" << vanillaboot << endl;
                        //flash->execUnixCommand("rm -rf /tmp/lbootzip");

                        if (!isVanillaData) {
                            GuiConfirm *confirm = new GuiConfirm(gui->renderer);
                            confirm->label = _("The LBOOT does not contain CLEAN PSC firmware. You may SOFTBRICK your console. Are you sure?");
                            confirm->show();
                            bool result = confirm->result;
                            delete (confirm);

                            if (!result) {
                                canRestore = false;
                            }
                        }
                        if (isRootlessBackup){
                            GuiConfirm *confirm = new GuiConfirm(gui->renderer);
                            confirm->label = _("The LBOOT does not contain rootfs1. This may be ABFK1.0a backup. Are you sure?");
                            confirm->show();
                            bool result = confirm->result;
                            delete (confirm);

                            if (!result) {
                                canRestore = false;
                            }
                        }
                    }

                    cout << "Files Unziped" << endl;
                }
                if (canRestore) {
                    Led->setMode(LED_RED);

                    flash->setRecoveryMode(true);
                    gui->drawText(_("Recovery Mode On"));
                    SDL_Delay(3000);
                    flash->execUnixCommand("killall -9 autobleem-gui");
                    flash->execUnixCommand("systemctl stop weston");
                    flash->execUnixCommand("systemctl reboot");
                    delete flash;
                    loopme=false;
                } else {
                    gui->drawText(_("Recovery interrupted"));
                    SDL_Delay(3000);
                    loopme=true;
                }
            } else {
                gui->drawText(_("No backup found - Recovery locked"));
                SDL_Delay(3000);
                loopme=true;
            }
        }

        if (selection == SEL_QUIT) {
            loopme = false;
        }

        if (selection == SEL_BACKUP) {
            Led->setMode(BLINK_GREEN);
            gui->drawText(_("Creating backup...."));
            // zip files
            flasher *flash = new flasher();
            vector<FileTag> files;
            files.push_back(FileTag("/dev/disk/by-partlabel/BOOTIMG1", "boot.img"));

            // Advice from madmonkey - do not backup root as AB will not alter it
            files.push_back(FileTag("/dev/disk/by-partlabel/ROOTFS1", "rootfs.ext4"));
            files.push_back(FileTag("/dev/disk/by-partlabel/USRDATA", "userdata.ext4"));
            files.push_back(FileTag("/dev/disk/by-partlabel/TEE1", "tz.img"));


            flash->execUnixCommand("rm -rf /media/LBOOT.EPB");
            flash->zipFiles(files, "/media/LBOOT.EPB");
            flash->addSignature("/media/LBOOT.EPB");

            flash->execUnixCommand("sync");
            delete (flash);
            loopme=false;
        }
    }

    delete Led;
    return 0;
}