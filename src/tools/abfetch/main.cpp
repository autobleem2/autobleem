// abfetch - AutoBleem's own downloader: one URL into one file over HTTP or HTTPS.
//
//   abfetch [-o FILE] [--continue] [--cacert FILE] [--connect-timeout S] [--stall-timeout S] [-q] URL
//
// The console fetches its online update with it (psc.ini's update_download_command, abupdate's UpdateRoms
// download), from the stick - so the update does not depend on what the console's kernel payload ships: a
// console on the AutoBleem 1.x kernel, whose overlay has no curl, updates the same as one on the current
// payload. TLS is mbedTLS (third_party/mbedtls, autobleem_config.h): the server's chain must end in a
// certificate of the CA bundle, cacert.pem next to this program unless --cacert names another, and must
// name the host; its validity dates are not checked, because the console has no battery-backed clock.
//
// Like curl -sfL: redirects are followed, an HTTP error status is a failure, nothing is printed unless it
// fails. A download that does not complete removes its output file - unless --continue: then what arrived
// stays, and the next --continue asks only for the rest (a Range request; the Store's big downloads over a
// console's WiFi). Exit codes: fetch.h's Result.
#include "fetch.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace std;

namespace {

// the directory this program is in, with a trailing separator; "" when it cannot be told
string ownDir(const char *argv0) {
    string path;
#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD length = GetModuleFileNameA(nullptr, buffer, sizeof(buffer));
    if (length > 0 && length < sizeof(buffer))
        path.assign(buffer, length);
#else
    char buffer[4096];
    ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length > 0)
        path.assign(buffer, static_cast<size_t>(length));
#endif
    if (path.empty() && argv0 != nullptr)
        path = argv0;
    size_t slash = path.find_last_of("/\\");
    return slash == string::npos ? "" : path.substr(0, slash + 1);
}

int usage() {
    fprintf(stderr,
            "usage: abfetch [-o FILE] [--continue] [--cacert FILE] [--connect-timeout S] [--stall-timeout S] [-q] URL\n");
    return abfetch::BadUsage;
}

bool seconds(const char *text, int &out) {
    char *end = nullptr;
    long value = strtol(text, &end, 10);
    if (end == text || *end != 0 || value < 1 || value > 3600)
        return false;
    out = static_cast<int>(value);
    return true;
}

} // namespace

int main(int argc, char **argv) {
    abfetch::Options options;
    options.output = "-";
    bool quiet = false;
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        bool hasValue = i + 1 < argc;
        if (arg == "-o" && hasValue)
            options.output = argv[++i];
        else if (arg == "--cacert" && hasValue)
            options.caFile = argv[++i];
        else if (arg == "--connect-timeout" && hasValue) {
            if (!seconds(argv[++i], options.connectTimeout))
                return usage();
        } else if (arg == "--stall-timeout" && hasValue) {
            if (!seconds(argv[++i], options.stallTimeout))
                return usage();
        } else if (arg == "-q")
            quiet = true;
        else if (arg == "--continue" || arg == "-C")
            options.resume = true;
        else if (!arg.empty() && arg[0] != '-' && options.url.empty())
            options.url = arg;
        else
            return usage();
    }
    if (options.url.empty())
        return usage();
    if (options.caFile.empty())
        options.caFile = ownDir(argv[0]) + "cacert.pem";

    string error;
    abfetch::Result result = abfetch::fetch(options, error);
    if (result != abfetch::Ok && !quiet)
        fprintf(stderr, "abfetch: %s\n", error.c_str());
    return result;
}
