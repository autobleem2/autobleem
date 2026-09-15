//
// Created by screemer on 2018-12-15.
//

#include "scanner.h"
#include "../gui/gui.h"
#include "../lang.h"
#include <unistd.h>

using namespace std;

//*******************************
// Scanner::onScanProgress
//*******************************
void Scanner::onScanProgress(ScanStage stage, const string &detail) {
    switch (stage) {
        case ScanStage::Scanning:
            Gui::splash(_("Scanning..."));
            break;
        case ScanStage::Game:
            Gui::splash(_("Game:") + " " + detail);
            break;
        case ScanStage::DecompressingEcm:
            Gui::splash(detail.empty() ? _("Decompressing ecm:") : detail);
            break;
        case ScanStage::UpdatingDatabase:
            Gui::splash(_("Updating regional.db..."));
            break;
        case ScanStage::GameFailedVerify:
            Gui::splash(_("Game failed to verify:") + " " + detail);
            sleep(3);   // long enough to read it
            break;
    }
}
