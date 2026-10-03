//
// abfetch's download: one URL into one file over HTTP or HTTPS (mbedTLS, third_party/mbedtls), redirects
// followed. The output file is written as the body arrives - the launcher's update shows the growing file's
// size as its progress - and removed again when the download does not complete.
//
#pragma once

#include <string>

namespace abfetch {

// the exit codes of the program; fetch() returns one
enum Result {
    Ok = 0,
    BadUsage = 1,
    NetworkError = 2, // no address, no connection, a stall, the connection dropped
    TlsError = 3,     // the handshake failed or the server's certificate is not trusted
    HttpError = 4,    // a status that is not 2xx after the redirects (curl -f's 22)
    WriteError = 5,   // the output file could not be written
    Incomplete = 6,   // fewer bytes than Content-Length, a broken chunked stream, a malformed response
    TooManyRedirects = 7,
};

struct Options {
    std::string url;
    std::string output;      // the file to write; "-" = stdout
    std::string caFile;      // PEM bundle the HTTPS server's chain must end in
    int connectTimeout = 20; // seconds, for each address tried
    int stallTimeout = 60;   // seconds without a byte before giving up (TLS handshake included)
    int maxRedirects = 5;
    // --continue: what is already in `output` is kept and only the rest is asked for (a Range request);
    // a server that answers 206 has it appended, one that answers 200 (no ranges) has the file started over,
    // and 416 (nothing left to send) is taken as complete. A download that stops keeps what arrived, so
    // the next --continue picks it up. Not for stdout.
    bool resume = false;
};

Result fetch(const Options &options, std::string &error);

} // namespace abfetch
