#include "gui_options_menu_base.h"

using namespace std;

//*******************************
// void GuiOptionsMenuBase::init()
//*******************************
void GuiOptionsMenuBase::init() {
    GuiMenuBase<OptionsInfo>::init();
}

//*******************************
// driverRowName(OptionsInfo)
//*******************************
std::string driverRowName(const OptionsInfo &info) {
    return app.lang().translate(info.descriptionToTranslate);
}

//*******************************
// void GuiOptionsMenuBase::getBooleanSymbolText
//*******************************
std::string GuiOptionsMenuBase::getBooleanSymbolText(const OptionsInfo &info, const std::string &value) {
    if (info.choices[0] == "true") {
        // the boolean is reversed
        if (value == "true")
            return "|@Uncheck|";
        else
            return "|@Check|";
    } else {
        // boolean is normal
        if (value == "true")
            return "|@Check|";
        else
            return "|@Uncheck|";
    }
}

//*******************************
// void GuiOptionsMenuBase::getLineText
//*******************************
std::string GuiOptionsMenuBase::getLineText(const OptionsInfo &info) {
    std::string temp = app.lang().translate(info.descriptionToTranslate) + " ";
    auto value = app.config().inifile.values[info.iniKey];
    if (info.keyIsBoolean) {
        temp += getBooleanSymbolText(info, value);
    } else {
        temp += value; // append the current text value in the options list
    }
    return temp;
}

//*******************************
// void GuiOptionsMenuBase::renderOptionRow()
//*******************************
// the label at the row's left, the value at its right edge: a boolean's check switch (where
// renderTextLineOptions puts it), any other value as text right-aligned to the same edge
void GuiOptionsMenuBase::renderOptionRow(const OptionsInfo &info, int y) {
    const string label = app.lang().translate(info.descriptionToTranslate);
    const string value = app.config().inifile.values[info.iniKey];
    if (info.keyIsBoolean) {
        gui->text().renderTextLineOptions(label + " " + getBooleanSymbolText(info, value), -y, 0, XALIGN_LEFT);
        return;
    }
    gui->text().renderTextLine(label, -y, 0, XALIGN_LEFT, 0, font);
    // at the panel's right edge, in the row role's value colour (the caller sets the role)
    gui->text().renderRowValue(valueText(info, value), -y, 0, 0, font);
}

//*******************************
// void GuiOptionsMenuBase::valueText()
//*******************************
string GuiOptionsMenuBase::valueText(const OptionsInfo &, const string &value) {
    return value;
}

//*******************************
// void GuiOptionsMenuBase::renderLineIndexOnRow()
//*******************************
void GuiOptionsMenuBase::renderLineIndexOnRow(int index, int row) {
    renderOptionRow(lines[index], yoffset + font.lineHeight() * row);
}

//*******************************
// void GuiOptionsMenuBase::validSelectedIndex()
//*******************************
bool GuiOptionsMenuBase::validSelectedIndex() {
    return (lines.size() > 0 && selected >= 0 && selected < lines.size());
}

//*******************************
// void GuiOptionsMenuBase::getChoicesSize()
//*******************************
unsigned int GuiOptionsMenuBase::getChoicesSize() {
    if (validSelectedIndex())
        return lines[selected].choices.size();
    else
        return 0;
}

//*******************************
// void GuiOptionsMenuBase::getCurrentOptionIndex()
//*******************************
unsigned int GuiOptionsMenuBase::getCurrentOptionIndex(OptionsInfo &info, const std::string &current) {
    const vector<string> &list = info.choices;
    // find current position
    int pos = 0;
    for (int i = 0; i < list.size(); i++) {
        if (list[i] == current) {
            pos = i;
            break;
        }
    }

    return pos;
}

//*******************************
// void GuiOptionsMenuBase::getPrevNextOption()
//*******************************
std::string GuiOptionsMenuBase::getPrevNextOption(OptionsInfo &info, const std::string &current, bool next) {
    const vector<string> &list = info.choices;
    // find current position
    int pos = 0;
    for (int i = 0; i < list.size(); i++) {
        if (list[i] == current) {
            pos = i;
            break;
        }
    }
    if (next) {
        pos++;
        if (pos >= list.size()) {
            pos = list.size() - 1;
        }
    } else {
        pos--;
        if (pos < 0)
            pos = 0;
    }

    return list[pos];
}

//*******************************
// void GuiOptionsMenuBase::doPrevNextOption()
//*******************************
string GuiOptionsMenuBase::doPrevNextOption(OptionsInfo &info, bool next) {
    string nextValue = getPrevNextOption(info, app.config().inifile.values[info.iniKey], next);
    app.config().inifile.values[info.iniKey] = nextValue;
    return nextValue;
}

//*******************************
// void GuiOptionsMenuBase::doPrevNextOption()
//*******************************
std::string GuiOptionsMenuBase::doPrevNextOption(bool next) {
    if (lines.size() > 0 && selected >= 0 && selected < lines.size())
        return doPrevNextOption(lines[selected], next);
    else
        return "";
}

//*******************************
// void GuiOptionsMenuBase::doOptionIndex()
//*******************************
string GuiOptionsMenuBase::doOptionIndex(unsigned int index) {
    if (validSelectedIndex()) {
        auto &choices = lines[selected].choices;
        if (choices.size() > 0 && index < choices.size()) {
            string nextValue = choices[index];
            app.config().inifile.values[lines[selected].iniKey] = nextValue;
            return nextValue;
        } else
            return ""; // index is not within range
    } else
        return "";
}

//*******************************
// void GuiOptionsMenuBase::doFirstOption()
//*******************************
string GuiOptionsMenuBase::doFirstOption() {
    return doOptionIndex(0);
}

//*******************************
// void GuiOptionsMenuBase::doLastOption()
//*******************************
string GuiOptionsMenuBase::doLastOption() {
    if (validSelectedIndex()) {
        auto &choices = lines[selected].choices;
        if (choices.size() > 0) {
            return doOptionIndex(choices.size() - 1);
        } else
            return ""; // index is not within range
    } else
        return "";
}

//*******************************
// void GuiOptionsMenuBase::doStart_Pressed()
//*******************************
void GuiOptionsMenuBase::doStart_Pressed() {
    doRandomOption();
    render();
}
