//
// The tool's static texts.
//
#include "pscbios_pages.h"
#include "core/main.h"
#include "core/version.h" // generated into the build tree

using namespace std;

//*******************************
// pscbiosCredits
//*******************************
vector<string> pscbiosCredits() {
    auto heading = [](const string &text) { return ".-= " + text + " =-."; }; // the decoration is not translated
    return {"PSC BIOS " + string(Version::FULL_VERSION),
            "----------------",
            heading(_("Code C++ and shell scripts")),
            "screemer(AutoBleem), madmonkey(Hakchi)",
            " ",
            heading(_("Linux Kernel Patching")),
            "screemer, madmonkey",
            " ",
            heading(_("Testing")),
            "MagnusRC, xboxiso, Azazel, Solidius, SupaSAIAN, Kingherb, saptis",
            heading(_("Localization support")),
            "nex(German), Azazel(Polish), gadsby(Turkish), GeekAndy(Dutch), Pardubak(Slovak), SupaSAIAN(Spanish), "
            "Mate(Czech)",
            "Sasha(Italian), Jakejj(BR_Portuguese), jolny(Swedish), StepJefli(Danish), alucard73 / MagnusRC(French), "
            "Quenti(Occitan), ",
            " ",
            _("Support via Discord:") + " https://discord.gg/AHUS3RM",
            _("This is free software. It works AS IS and We take no responsibility for any issues or damage.")};
}

//*******************************
// dualshock3PairingLines
//*******************************
vector<string> dualshock3PairingLines() {
    return {_("In AutoBleem DualShock/SixAxis type controller are automatically paired if you have a compatible "
              "BlueTooth dongle and an AutoBleem Kernel installed."),
            "",
            _("To pair a controller of this kind please follow the procedure"),
            "  " + _("1. Using a micro USB charging cable connect the controller to one of available USB FRONT port "
                     "of the console"),
            "  " + _("2. When the leds on the controller will start to blink press the PlayStation Home button"),
            "  " + _("3. The controller should be recognized as normal wired PS3 compatible controller"),
            "  " + _("4. Disconnect charging cable from the console and press PlayStation Home button again"),
            "  " + _("5. After 1-5 seconds controller will auto pair to AutoBleem and show player number LED "
                     "(similar as paired to PS3)"),
            "  " + _("6. If the controller does not react for buttons or analog sticks are setup wrong - use mapping "
                     "section to reconfigure"),
            "",
            "  " + _("NOTE 1:Genuine SONY DualShock3 and Sixaxis are supported. Also SHANWAN produced clones should "
                     "work."),
            "  " + _("NOTE 2: In case you pair the controller back to PS3 or other console you have to follow this "
                     "procedure again."),
            "  " + _("NOTE 3: You do not need to do this procedure in this screen. USB pairing works on any screen as "
                     "soon console is powered on."),
            "  " + _("NOTE 4: Pairing information is saved in console, so next time just press PlayStation Home on "
                     "the controller and it will work in AutoBleem")};
}

//*******************************
// bluetoothPairingLines
//*******************************
vector<string> bluetoothPairingLines() {
    return {_("This section of hardware configuration is not available yet."),
            "",
            _("We are working on implementing Bluetooth pairing on console for a next release"),
            _("Until then please use BlueTool by DanTheMan (Included in the package)"),
            "",
            "  " + _("NOTE 1: After pairing in BlueTool controller is automatically mapped until you unpair it")};
}
