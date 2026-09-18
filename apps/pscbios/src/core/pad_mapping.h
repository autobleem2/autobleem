//
// PadMapping: the mapping wizard's logic without the screen - the 25 things a standard game controller
// has, in the order the user is asked for them; which raw input just moved; the merge of the two halves
// of each stick axis into one; the gamecontrollerdb line that comes out.
//
// A raw input is written the way SDL's mapping strings do: "b<n>" a button, "h<n>.<mask>" a hat
// direction, "+a<n>"/"-a<n>" half an axis that rests in the middle (a stick), "a<n>" a whole axis that
// rests at one end (a trigger), and "a<n>~" a stick axis the pad reports inverted.
//
#pragma once

#include <ableem/ui/joystick.h> // JoystickState, a plain struct - no SDL behind it

#include <string>
#include <vector>

//******************
// PadMapping
//******************
class PadMapping {
public:
    enum class Scan { Digital, Analog, Trigger };

    struct Element {
        std::string apiName; // "a", "dpup", "-leftx", "lefttrigger", ...
        Scan scan = Scan::Digital;
        int button = -1;       // the standard button (0..14) it lights up on the pad picture while asked for
        int axis = -1;         // or the standard axis (0..5)
        bool negative = false; // ... and towards which end
        std::string value;     // the raw input found for it, "" until then
    };

    // the 25 elements the wizard asks for, in order; every value empty
    static std::vector<Element> standardElements();

    // the raw input that moved between `initial` and `now`, a button first, then a hat, then an axis (a
    // change of more than AxisThreshold); "" when nothing did, or when the input is already the value of
    // one of `taken` (an element cannot be mapped twice)
    static std::string detectChange(const ableem::JoystickState &initial, const ableem::JoystickState &now,
                                    const std::vector<Element> &taken);
    // the raw inputs still away from their rest position - what the wizard waits to clear before asking
    // for the next element
    static bool anythingHeld(const ableem::JoystickState &initial, const ableem::JoystickState &now);

    // the list to write: each stick axis' two halves merged into one "a<n>" (with "~" when the halves
    // came out inverted) when they are the two halves of one raw axis, both dropped when one is missing,
    // kept as two half-axis keys otherwise; every unmapped element dropped; "platform:<platform>" last
    static std::vector<Element> finalElements(const std::vector<Element> &scanned, const std::string &platform);

    // "<guid>,<name>,<apiName>:<value>,...," as SDL reads it
    static std::string mappingLine(const std::string &guid, const std::string &name,
                                   const std::vector<Element> &finals);
    // a pad name SDL will take inside the line: letters, digits and spaces only
    static std::string cleanName(const std::string &name);

    static const int AxisThreshold = 32000; // how far an axis must travel to count as "moved"
    static const int RestTolerance = 600;   // an axis within this of 0 rested in the middle (a stick)
    static const int HeldThreshold = 20000; // an axis still this far from its rest is still held

private:
    static void mergeAxis(std::vector<Element> &elements, const std::string &apiName, size_t minus, size_t plus);
};
