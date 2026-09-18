#pragma once

#include "gui_menu_base.h"

//*******************************
// struct OptionsInfo
//*******************************
struct OptionsInfo {
    int id;                             // ex: CFG_THEME
    std::string descriptionToTranslate; // ex: _("AutoBleem Theme:")
    std::string iniKey;                 // ex: "theme"
    bool keyIsBoolean;                  // if it's false/true we substitute the switch icons
    std::vector<std::string> choices;

    explicit OptionsInfo(int _id = 0, std::string _descriptionToTranslate = std::string(),
                         std::string _iniKey = std::string(), bool _keyIsBoolean = false,
                         const std::vector<std::string> &_choices = std::vector<std::string>())
        : id(_id), descriptionToTranslate(_descriptionToTranslate), iniKey(_iniKey), keyIsBoolean(_keyIsBoolean),
          choices(_choices) {}
};

//*******************************
// class GuiOptionsMenuBase
//*******************************
class GuiOptionsMenuBase : public GuiMenuBase<OptionsInfo> {
public:
    explicit GuiOptionsMenuBase(ableem::GuiBase &_gui) : GuiMenuBase(_gui) {}
    void init() override;

    virtual std::string getBooleanSymbolText(const OptionsInfo &info, const std::string &value);
    virtual std::string getLineText(const OptionsInfo &info);

    void renderLineIndexOnRow(int index, int row) override;

    bool validSelectedIndex(); // returns true if the selected line index is a valid index
    unsigned int
    getChoicesSize(); // returns the number of choices on the selected line (0 if the selected index is invalid).

    virtual unsigned int getCurrentOptionIndex(OptionsInfo &info, const std::string &current);
    virtual std::string getPrevNextOption(OptionsInfo &info, const std::string &current, bool next);
    virtual std::string doPrevNextOption(OptionsInfo &info, bool next);
    virtual std::string doPrevNextOption(bool next);
    virtual std::string doRandomOption() { return ""; } // only a few lines will use this.  most will just return.

    virtual std::string doOptionIndex(unsigned int index);
    virtual std::string doFirstOption();
    virtual std::string doLastOption();

    int computeAmountTomoveBy(unsigned int totalSize);
    void doL1_Pressed() override;
    void doR1_Pressed() override;
    void doStart_Pressed() override;

    void doL2_Pressed() override { doFirstOption(); }
    void doR2_Pressed() override { doLastOption(); }
    void doHome() override { doFirstOption(); }
    void doEnd() override { doLastOption(); }
};
