#include "ableem/engine/ecm_decoder.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/game_types.h"

#include <cstdio>
#include <iostream>

extern "C" {
void eccedc_init(void);
int unecmify(FILE *in, FILE *out);
void unecm_set_progress(void (*cb)(const char *message));
}

using namespace std;

namespace ableem {

namespace {
    function<void(const string &)> progressHandler;

    void forwardProgress(const char *message) {
        if (progressHandler)
            progressHandler(message);
    }
}

//*******************************
// EcmDecoder::setProgressHandler
//*******************************
void EcmDecoder::setProgressHandler(function<void(const string &)> handler) {
    progressHandler = handler;
    unecm_set_progress(handler ? forwardProgress : nullptr);
}

//*******************************
// EcmDecoder::decode
//*******************************
bool EcmDecoder::decode(const string &input, string output) {
    cout << "Unpacking: " << input << " to " << output << endl;
    if (!DirEntry::matchExtension(output, EXT_BIN)) {
        output = output + ".bin";
    }
    eccedc_init();
    FILE *fin = fopen(input.c_str(), "rb");
    if (!fin) {
        return false;
    }
    FILE *fout = fopen(output.c_str(), "wb");
    if (!fout) {
        fclose(fin);
        return false;
    }
    int result = unecmify(fin, fout);
    fclose(fout);
    fclose(fin);
    return result == 0;
}

} // namespace ableem
