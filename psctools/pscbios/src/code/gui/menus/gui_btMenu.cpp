//
// Created by screemer on 24.03.2020.
//

#include "gui_btMenu.h"

void gui_btMenu::init()
{
    GuiMenuBase::init();
    NetworkController *controller=new NetworkController();
    if (controller->isBluetoothOn())
    {
        active = true;
        controllerName = controller->getBluetoothInfo();
    }

    if (active)
    {
        bt->init();
    }
}
void gui_btMenu::fill()
{
    lines.clear();
    lines.emplace_back(_("Controller:"));
    if (active)
    {
        lines.emplace_back("    "+controllerName);
    } else
    {
        lines.emplace_back("    "+_("Not Found"));
    }
    lines.emplace_back(_("Operations:"));
    lines.emplace_back("    "+_("Set controller discoverable"));
    lines.emplace_back("    "+_("Scan controller for new devices"));

    lines.emplace_back(_("New devices:"));
    lines.emplace_back(_( "Paired devices:"));

}
void gui_btMenu::render() {
    if (active)
    {
        bt->processSocket();
    }
    fill();
    GuiStringMenu::render();
}
void gui_btMenu::doCircle_Pressed() {
    bt->runCommand("devices");
}
void gui_btMenu::doCross_Pressed() {
bt->runCommand("scan on");
}