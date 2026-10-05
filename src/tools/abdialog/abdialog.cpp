// abdialog - the two dialogs a PE mod's launch.sh calls, drawn as AutoBleem's own screens in the current theme.
//
//   sdl_display       -file FILE
//       the text screen (sdl_text_display's): stays until it is killed
//   sdl_choicedisplay -controller-db FILE -only LETTERS -file FILE
//       the question (sdl_input_text_display's): waits for a button of LETTERS and exits with 100 Cross, 101 Circle,
//       102 Square, 103 Triangle
//
// The program is one binary run under the names the 2020 tools had (rc/pe_env.sh links them in the mod's bin/); the
// interface, the command file and the exit codes are in dialog_cmd.h. It is an AppBase program like the console
// tools: the launcher's config.ini (theme, language), the launcher's pad table, and the pad the App got - the
// environment the mod runs in (LD_PRELOAD of abpad, or the kernel pad abpadd made) is the one it inherits.
//
// Exit: 100..103 an answer, 1 closed without one, 2 wrong arguments, 3 no display (the script then answers by its
// fixed rule; the text screen does not exit but waits to be killed, as the process the mod stops with killall).
#include "dialog_cmd.h"

#include "app_base.h"
#include "core/services/environment_setup.h"

#include <ab_gui/confirm.h>
#include <ab_gui/hold_repeat.h>
#include <ab_gui/panel.h>
#include <ab_gui/screen_transition.h>
#include <ab_gui/text_page.h>
#include <ableem/engine/log.h>
#include <ableem/ui/platform.h>

#include <csignal>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;
using ableem::Event;

namespace {

//*******************************
// the command file
//*******************************
bool readFile(const string &path, string &content) {
    ifstream in(path, ios::binary);
    if (!in)
        return false;
    ostringstream all;
    all << in.rdbuf();
    content = all.str();
    return true;
}

// a file's change stamp: size and modification time, "" when it is not there
string stamp(const string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
        return string();
    return to_string(static_cast<long long>(st.st_size)) + ":" + to_string(static_cast<long long>(st.st_mtime));
}

//*******************************
// TextDisplay
//*******************************
// The text screen: the command file's texts as a page in the theme's panel. It follows the file - a mod that calls
// sdl_text_display again gets the new text - and scrolls with the d-pad when the text is longer than the panel (a press
// steps and wraps nowhere, a held direction repeats at the pad's pace and stops at the ends). Circle closes it, which
// the mod's own `killall sdl_display` does as well.
class TextDisplay : public abgui::TextPage {
public:
    TextDisplay(ableem::GuiBase &gui, abgui::Context &context, string commandFile)
        : abgui::TextPage(gui, context), file_(std::move(commandFile)) {
        // the App's name (rc/pe_env.sh exports it); without one, the word
        const char *app = getenv("PE_APP_TITLE");
        title = app && *app ? string(app) : _("Message");
        reload();
    }

    bool prepareFrame() override {
        if (stamp(file_) != stamp_)
            reload();
        hold_.tick(gui.input(), ctx.ticks(), [this](int dir, bool repeat) {
            if (repeat)
                scrollBy(dir);
        });
        return true;
    }

    void onAction(const abgui::ActionEvent &action) override {
        abgui::TextPage::onAction(action);
        const Event::Type type = action.event.type;
        if (type == Event::Type::DpadDown || type == Event::Type::DpadUp)
            hold_.track(gui.input(), ctx.ticks());
    }

private:
    void reload() {
        stamp_ = stamp(file_);
        string content;
        readFile(file_, content);
        lines = abdialog::parseCommandFile(content).lines;
        firstLine_ = 0;
    }

    string file_, stamp_;
    abgui::DpadHold hold_;
};

//*******************************
// ChoiceDialog
//*******************************
// The question: a compact dialog over the theme's background - the text wrapped to the panel, the buttons that may
// answer as footer hints. Only the buttons of `allowed` answer, and not in the first moments (a press that started the
// App may still be arriving).
class ChoiceDialog : public abgui::Screen {
public:
    ChoiceDialog(ableem::GuiBase &gui, abgui::Context &context) : abgui::Screen(gui, context) {
        declareTransitions(abgui::ScreenTransitions(abgui::Transition::pop()));
    }

    string title;
    vector<string> text;                        // the lines of the question
    string allowed = abdialog::answerLetters(); // the letters that answer
    string labels[4];                           // the words of X O S T in the footer (empty: the button alone)
    int answer = -1;                            // the exit code once pressed

    void draw() override {
        ctx.drawBackdrop();
        const abgui::Style style = ctx.style();
        const ableem::Font &font = ctx.font(abgui::FontRole::Row);
        vector<string> rows;
        for (const string &line : text) {
            const vector<string> wrapped = abgui::wrapText(line, abgui::Confirm::textWidth(style),
                                                           [&font](const string &s) { return font.width(s); });
            rows.insert(rows.end(), wrapped.begin(), wrapped.end());
        }
        const int textHeight = static_cast<int>(rows.size()) * font.lineHeight();
        vector<abgui::HintItem> hints;
        for (int i = 0; i < 4; i++) {
            const char letter = abdialog::answerLetters()[i];
            if (allowed.find(letter) != string::npos)
                hints.push_back({{string(1, letter)}, labels[i]});
        }
        const abgui::Panel panel(abgui::Confirm::panelRect(style, textHeight, ctx.renderer().width(),
                                                           ctx.renderer().height(),
                                                           abgui::Panel::compactWidth(ctx, hints, "")),
                                 style);
        panel.sheet(ctx);
        const bool programShadow = ctx.setTextShadow(style.textShadow);
        int y = panel.header(ctx, title) + abgui::Confirm::TextGapTop;
        const int x = panel.rect().x + style.rowInset + 8;
        for (const string &row : rows) {
            if (!row.empty())
                ctx.drawText(font, row, x, y, style.text);
            y += font.lineHeight();
        }
        style.footer(ctx, panel.footer(), hints, "", false);
        ctx.setTextShadow(programShadow);
    }

    // rests between presses, a frame four times a second meanwhile (the DebugDriver's shots)
    void loop() override {
        menuVisible = true;
        openedMs_ = ctx.ticks();
        while (menuVisible) {
            if (!gui.input().waitForEvent(250))
                render();
            Event e;
            while (gui.input().poll(e)) {
                if (handleQuit(e))
                    continue;
                handle(e);
            }
        }
    }

    void onAction(const abgui::ActionEvent &action) override { press(action.event); }
    void onUnmapped(const Event &event) override { press(event); }

private:
    // the button itself, not the action it is mapped to: a mod's Cross is the pad's Cross, whatever Options swap
    void press(const Event &event) {
        if (event.type != Event::Type::ButtonDown || ctx.ticks() - openedMs_ < GraceMs)
            return;
        char letter = 0;
        switch (event.button) {
        case ableem::Button::Cross:
            letter = 'X';
            break;
        case ableem::Button::Circle:
            letter = 'O';
            break;
        case ableem::Button::Square:
            letter = 'S';
            break;
        case ableem::Button::Triangle:
            letter = 'T';
            break;
        default:
            return;
        }
        if (allowed.find(letter) == string::npos)
            return;
        ctx.play(abgui::UiSound::Cursor);
        answer = abdialog::answerCode(letter);
        menuVisible = false;
    }

    static constexpr unsigned int GraceMs = 400;
    unsigned int openedMs_ = 0;
};

//*******************************
// the question's words
//*******************************
// what the 2020 picture said, in words: the Preset says which question it was. Every string is _() in the lang files.
void describe(ChoiceDialog &dialog, abdialog::Preset preset, const vector<string> &given) {
    dialog.title = _("Choose an option");
    string defaultText = _("Press one of the buttons.");
    switch (preset) {
    case abdialog::Preset::Controller:
        dialog.title = _("Controller");
        defaultText = _("Which controls does your pad have?");
        dialog.labels[0] = _("One D-pad");
        dialog.labels[1] = _("Analog sticks");
        break;
    case abdialog::Preset::Windows:
        defaultText = _("What do you want to do?");
        dialog.labels[0] = _("Run Windows");
        dialog.labels[1] = _("Install Windows");
        dialog.labels[2] = _("Run DOS");
        dialog.labels[3] = _("Uninstall Windows");
        break;
    case abdialog::Preset::None:
        break;
    }
    dialog.text = given.empty() ? vector<string>{defaultText} : given;
}

//*******************************
// the program
//*******************************
class DialogApp : public AppBase {
public:
    DialogApp() : AppBase("PE dialog") {}

    int runDisplay(const abdialog::Args &args) {
        gui_->loadAssets(false);
        gui_->hideMouseCursor();
        TextDisplay page(*gui_, gui_->uiContext(), args.file);
        page.show();
        gui_->finish();
        return 0;
    }

    int runChoice(const abdialog::Args &args) {
        string content;
        readFile(args.file, content);
        const abdialog::Command command = abdialog::parseCommandFile(content);
        gui_->loadAssets(false);
        gui_->hideMouseCursor();
        ChoiceDialog dialog(*gui_, gui_->uiContext());
        dialog.allowed = abdialog::allowedLetters(args.only);
        describe(dialog, abdialog::presetFor(command.image.empty() ? args.image : command.image), command.lines);
        dialog.show();
        gui_->finish();
        return dialog.answer < 0 ? 1 : dialog.answer; // closed without an answer: 1
    }
};

int usage(const string &why) {
    PLOG_ERROR << "abdialog: " << why;
    PLOG_ERROR << "usage: sdl_display -file FILE | sdl_choicedisplay [-controller-db FILE] [-only LETTERS] -file FILE"
                  " | abdialog text|choice (the same options)";
    return abdialog::ExitUsage;
}

// no display: the text screen has to be a process the mod can still stop, the question gives up
int noDisplay(abdialog::Mode mode, const string &why) {
    PLOG_ERROR << "abdialog: no display (" << why << ")";
    if (mode == abdialog::Mode::Display) {
        for (;;)
            sleep(3600); // SIGTERM, SIGKILL or killall ends it
    }
    return abdialog::ExitNoDisplay;
}

} // namespace

//*******************************
// main
//*******************************
int main(int argc, char *argv[]) {
    ableem::Log::initConsoleOnly();
    atexit(ableem::Platform::shutdownSDL);

    vector<string> args(argv + 1, argv + argc);
    abdialog::Mode mode = abdialog::Mode::Display;
    if (!abdialog::detectMode(argc > 0 ? argv[0] : "", args, mode))
        return usage("run as sdl_display or sdl_choicedisplay, or give text or choice first");
    abdialog::Args options;
    string error;
    if (!abdialog::parseArgs(args, options, error))
        return usage(error);

    // the launcher's data tree: AB_ROOT is the stick (the launcher exports it to every App), /media on the console
    const char *root = getenv("AB_ROOT");
    EnvironmentSetup::fromRoot(root && *root ? root : "/media");
    try {
        DialogApp app;
        return mode == abdialog::Mode::Display ? app.runDisplay(options) : app.runChoice(options);
    } catch (const exception &e) {
        return noDisplay(mode, e.what());
    }
}
