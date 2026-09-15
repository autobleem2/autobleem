// lib_ableem - engine: decodes Error Code Modeler (.ecm) compressed disc images back into plain .bin files.
// https://www.lifewire.com/ecm-file-2620956 - the decoder itself is Neill Corlett's unecm (src/engine/unecm.c).
#pragma once

#include <functional>
#include <string>

namespace ableem {

//******************
// EcmDecoder
//******************
class EcmDecoder {
public:
    // called every ~1 MB with a message like "Decoding ECMed bin (42%)"; nothing is reported without one
    static void setProgressHandler(std::function<void(const std::string &)> handler);

    // output gets ".bin" appended when it has no .bin extension. false when either file could not be opened
    // or the input is not a valid ECM stream.
    static bool decode(const std::string &input, std::string output);
};

} // namespace ableem
