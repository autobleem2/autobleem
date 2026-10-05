// abdialog's pure part: the command line the PE dialog scripts use, the command file they write, and the answer codes.
// No SDL and no UI in here - the tests drive it (tests/tools/test_abdialog.cpp), abdialog.cpp draws.
//
// The 2020 environment's dialog tools were three shell scripts and two programs; a mod's launch.sh calls
//
//   sdl_text_display TEXT [X Y SIZE FONT R G B BGIMAGE BGCOLOR]               shows TEXT, does not wait
//   sdl_input_text_display TEXT [X Y SIZE FONT R G B BGIMAGE ONLY BGCOLOR]    asks, waits, exits with the answer
//
// Our rc/pe/ scripts of those names keep that interface. They write the arguments as records into a command file
// and start the programs below (abdialog, started under the names the originals had):
//
//   sdl_display       -file FILE                                  the text screen; stays until it is killed
//   sdl_choicedisplay -controller-db FILE -only LETTERS -file FILE [-img PNG]
//                                                                 the question; exits with the answer
//
// The command file, one record per line, fields separated by a tab: "IMAGE x y path" (the picture behind the text),
// "FTEXT x y size font r g b text" and "FTEXTWBG x y size font r g b 0xAARRGGBB text" (a text and where/how to draw
// it). abdialog shows the texts as AutoBleem's own dialog in the current theme: the position, size, font, colour and
// background of a record are the 2020 screen's, not ours, and are not used.
#pragma once

#include <string>
#include <vector>

namespace abdialog {

//********************
// the mode
//********************
enum class Mode { Display, Choice };

// the mode the program was started as: from the name it was run under (sdl_display, sdl_choicedisplay - a link to
// the program), else from its first argument (`text`, `choice`), which is then dropped from `args`
bool detectMode(const std::string &argv0, std::vector<std::string> &args, Mode &mode);

//********************
// the command line
//********************
struct Args {
    std::string file;         // -file: the command file
    std::string controllerDb; // -controller-db: read and ignored (the launcher's own pad table is used)
    std::string only;         // -only: the letters of the buttons that may answer
    std::string image;        // -img: a picture (not used)
};

// the options after the mode; false with `error` set for an unknown option, a missing value or no -file
bool parseArgs(const std::vector<std::string> &args, Args &out, std::string &error);

//********************
// the command file
//********************
struct Command {
    std::vector<std::string> lines; // the texts of the FTEXT / FTEXTWBG records, one entry per line shown
    std::string image;              // the path the IMAGE record names; "" when there is none
};

// `content` of a command file. A text holding the two characters \n, or running on over line breaks, is several lines
// (the 2020 script's echo made newlines of them); blank lines at the start and the end are dropped, so a question
// whose text is " " (the mods pass that when the picture says everything) has no lines at all.
Command parseCommandFile(const std::string &content);

//********************
// the answer
//********************
// the exit codes: the answers 100 (Cross) .. 103 (Triangle) in the order of the letters X O S T, as the mods test
constexpr int AnswerBase = 100;
constexpr int ExitUsage = 2;     // wrong arguments
constexpr int ExitNoDisplay = 3; // no display to draw on: the script answers by its fixed rule instead

// "XOST" in the order of the answer codes
const char *answerLetters();

// the buttons that may answer, from -only: the letters X O S T (any case) in the order of the answer codes, each
// once; others are dropped. Empty or without one of them: all four.
std::string allowedLetters(const std::string &only);

// the exit code of the button `letter` (X O S T, any case); -1 for any other
int answerCode(char letter);

// the letter of the answer code (100..103); 0 for any other
char answerLetter(int code);

//********************
// what a question shows
//********************
// The 2020 questions were a picture, not text (the mods pass " " as the text): which picture they name tells what
// the buttons mean. The pictures are third-party art and not used; the names pick the words instead.
enum class Preset {
    None,       // any other question: the text as given, the buttons without words
    Controller, // *_controller_select.png: X one d-pad, O analog sticks
    Windows,    // win311_select.png: X run, O install, S DOS, T uninstall
};
Preset presetFor(const std::string &imagePath);

} // namespace abdialog
