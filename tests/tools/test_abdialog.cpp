// abdialog (src/tools/abdialog): the PE dialogs' pure part - how the program was started, its options, the command
// file the rc/pe scripts write, the buttons that may answer and the exit code of each. The screens need a display
// and are looked at on the VM.
#include "doctest/doctest.h"
#include "dialog_cmd.h"

#include <string>
#include <vector>

using namespace abdialog;
using namespace std;

namespace {

const string Tab = "\t";

// "FTEXT x y size font r g b text" as the script writes it
string ftext(const string &text) {
    return "FTEXT" + Tab + "640" + Tab + "120" + Tab + "12" + Tab + "/f.ttf" + Tab + "255" + Tab + "255" + Tab + "255" +
           Tab + text + "\n";
}

string ftextWbg(const string &text) {
    return "FTEXTWBG" + Tab + "640" + Tab + "120" + Tab + "12" + Tab + "/f.ttf" + Tab + "0" + Tab + "0" + Tab + "0" +
           Tab + "0x80000000" + Tab + text + "\n";
}

string image(const string &path) {
    return "IMAGE" + Tab + "640" + Tab + "360" + Tab + path + "\n";
}

} // namespace

TEST_CASE("the mode comes from the name the program runs under, else from the first argument") {
    Mode mode = Mode::Choice;
    vector<string> args = {"-file", "x"};
    CHECK(detectMode("/tmp/pe/project_eris/bin/sdl_display", args, mode));
    CHECK(mode == Mode::Display);
    CHECK(args.size() == 2);

    mode = Mode::Display;
    CHECK(detectMode("sdl_choicedisplay", args, mode));
    CHECK(mode == Mode::Choice);

    args = {"choice", "-only", "XO", "-file", "x"};
    CHECK(detectMode("/media/Autobleem/bin/autobleem/abdialog", args, mode));
    CHECK(mode == Mode::Choice);
    CHECK(args.size() == 4); // the word is dropped
    args = {"text", "-file", "x"};
    CHECK(detectMode("abdialog", args, mode));
    CHECK(mode == Mode::Display);

    args = {"-file", "x"};
    CHECK_FALSE(detectMode("abdialog", args, mode));
}

TEST_CASE("the options of the two programs") {
    Args args;
    string error;
    CHECK(parseArgs({"-file", "/tmp/cmd"}, args, error));
    CHECK(args.file == "/tmp/cmd");
    CHECK(args.only.empty());

    CHECK(parseArgs({"-controller-db", "/db.txt", "-only", "XOST", "-file", "/tmp/cmd", "-img", "a.png"}, args, error));
    CHECK(args.controllerDb == "/db.txt");
    CHECK(args.only == "XOST");
    CHECK(args.file == "/tmp/cmd");
    CHECK(args.image == "a.png");

    CHECK_FALSE(parseArgs({"-only", "XO"}, args, error)); // no -file
    CHECK(error.find("-file") != string::npos);
    CHECK_FALSE(parseArgs({"-file"}, args, error)); // no value
    CHECK(error.find("-file") != string::npos);
    CHECK_FALSE(parseArgs({"-file", "x", "-colour", "red"}, args, error));
    CHECK(error.find("-colour") != string::npos);
    CHECK_FALSE(parseArgs({}, args, error));
}

TEST_CASE("the command file: the texts of the records, the picture") {
    Command command = parseCommandFile(image("/app/doom_controller_select.png") + ftext("Loading..."));
    CHECK(command.image == "/app/doom_controller_select.png");
    REQUIRE(command.lines.size() == 1);
    CHECK(command.lines[0] == "Loading...");

    // the version with a background colour holds the text one field later
    command = parseCommandFile(image("/b.png") + ftextWbg("No install found! Exiting in 5 seconds..."));
    REQUIRE(command.lines.size() == 1);
    CHECK(command.lines[0] == "No install found! Exiting in 5 seconds...");

    // two records are two lines, in order
    command = parseCommandFile(image("/b.png") + ftext("first") + ftext("second"));
    REQUIRE(command.lines.size() == 2);
    CHECK(command.lines[0] == "first");
    CHECK(command.lines[1] == "second");

    // a tab inside the text stays in it
    command = parseCommandFile(ftext("a\tb"));
    REQUIRE(command.lines.size() == 1);
    CHECK(command.lines[0] == "a\tb");
}

TEST_CASE("the command file: line breaks inside a text") {
    // the two characters \n (bash's echo), and a real break the next line goes on after (dash's echo)
    Command command = parseCommandFile(ftext("one\\ntwo"));
    REQUIRE(command.lines.size() == 2);
    CHECK(command.lines[0] == "one");
    CHECK(command.lines[1] == "two");

    command = parseCommandFile(image("/b.png") + "FTEXT\t640\t120\t12\t/f.ttf\t255\t255\t255\tone\ntwo\nthree\n");
    REQUIRE(command.lines.size() == 3);
    CHECK(command.lines[1] == "two");
    CHECK(command.lines[2] == "three");

    // CRLF files are read the same
    command = parseCommandFile("FTEXT\t1\t2\t3\tf\t4\t5\t6\thello\r\n");
    REQUIRE(command.lines.size() == 1);
    CHECK(command.lines[0] == "hello");
}

TEST_CASE("the command file: a blank text is no text") {
    // the mods ask with " " when the picture says it all; the file may be empty or missing its records too
    CHECK(parseCommandFile(image("/q.png") + ftext(" ")).lines.empty());
    CHECK(parseCommandFile("").lines.empty());
    CHECK(parseCommandFile("garbage\nmore garbage\n").lines.empty());
    CHECK(parseCommandFile("garbage\nmore garbage\n").image.empty());
    // blank ends are cut, a blank line between two texts stays
    Command command = parseCommandFile(ftext(" ") + ftext("a") + ftext(" ") + ftext("b") + ftext(" "));
    REQUIRE(command.lines.size() == 3);
    CHECK(command.lines[0] == "a");
    CHECK(command.lines[1].empty());
    CHECK(command.lines[2] == "b");
}

TEST_CASE("the buttons that may answer, from -only") {
    CHECK(allowedLetters("XO") == "XO");
    CHECK(allowedLetters("XOST") == "XOST");
    CHECK(allowedLetters("TS") == "ST"); // the order of the answer codes
    CHECK(allowedLetters("xo") == "XO");
    CHECK(allowedLetters("OOX") == "XO");
    CHECK(allowedLetters("X1?") == "X");
    // none named: all four
    CHECK(allowedLetters("") == "XOST");
    CHECK(allowedLetters("??") == "XOST");
}

TEST_CASE("the answer is the exit code the mods test") {
    CHECK(answerCode('X') == 100); // Cross
    CHECK(answerCode('O') == 101); // Circle
    CHECK(answerCode('S') == 102); // Square
    CHECK(answerCode('T') == 103); // Triangle
    CHECK(answerCode('x') == 100);
    CHECK(answerCode('Q') == -1);
    for (char letter : string("XOST"))
        CHECK(answerLetter(answerCode(letter)) == letter);
    CHECK(answerLetter(99) == 0);
    CHECK(answerLetter(104) == 0);
    CHECK(AnswerBase == 100);
    CHECK(ExitUsage != ExitNoDisplay);
}

TEST_CASE("the picture a question names says what it asks") {
    CHECK(presetFor("/media/project_eris/etc/project_eris/SUP/launchers/tyrquake/quake_controller_select.png") ==
          Preset::Controller);
    CHECK(presetFor("/app/doom_controller_select.png") == Preset::Controller);
    CHECK(presetFor("doom_CONTROLLER_select.PNG") == Preset::Controller);
    CHECK(presetFor("/app/win311_select.png") == Preset::Windows);
    CHECK(presetFor("/app/win311splash.png") == Preset::None);
    CHECK(presetFor("") == Preset::None);
    CHECK(presetFor("/media/splashscreen.png") == Preset::None);
}
