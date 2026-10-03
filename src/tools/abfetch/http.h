//
// abfetch's HTTP/1.1 side with no sockets in it: URLs, the request, the response head, redirects and the
// chunked transfer coding. fetch.* puts it on a connection; the tests (tests/tools/test_abfetch.cpp) drive
// it directly.
//
#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace abfetch {

//******************
// Url
//******************
// http:// and https:// only. target is what goes on the request line: the path and the query, "/" when
// the URL has neither; a #fragment is dropped.
struct Url {
    std::string scheme; // "http" | "https"
    std::string host;   // an IPv6 literal without its brackets
    std::string port;   // always set: the URL's, else 80 / 443
    std::string target;

    bool tls() const { return scheme == "https"; }
    bool defaultPort() const { return port == (tls() ? "443" : "80"); }
    // host[:port] as the Host header and a Location resolved against this URL spell it
    std::string authority() const;
    std::string str() const { return scheme + "://" + authority() + target; }
};

bool parseUrl(const std::string &text, Url &url);

// a Location header resolved against the URL that answered with it: absolute, scheme-relative
// ("//host/x"), absolute-path ("/x") or relative ("x", against the answering URL's directory)
std::string resolveLocation(const Url &base, const std::string &location);

// GET, the Host header, Connection: close (one request per connection), no compression asked for; with
// rangeFrom > 0 a "Range: bytes=<rangeFrom>-" as well - the rest of a file that is partly there (--continue)
std::string buildRequest(const Url &url, unsigned long long rangeFrom = 0);

//******************
// ResponseHead
//******************
struct ResponseHead {
    int status = 0;
    std::vector<std::pair<std::string, std::string>> headers; // names lower-cased, values trimmed

    // the first header of that (lower-case) name, "" when there is none
    std::string header(const std::string &name) const;
    bool redirect() const { return status == 301 || status == 302 || status == 303 || status == 307 || status == 308; }
    bool chunked() const;
    // the Content-Length header's value; false when there is none or it is not a number
    bool contentLength(unsigned long long &length) const;
};

// the end of the head in a buffer being received: the offset just past its blank line, or npos while it
// has not all arrived yet
size_t headEnd(const std::string &buffer);
// the status line and the header lines up to (not including) the blank line
bool parseHead(const std::string &head, ResponseHead &out);

//******************
// ChunkedDecoder
//******************
// the chunked transfer coding (RFC 9112, 7.1), fed as the bytes arrive, in pieces of any size
class ChunkedDecoder {
public:
    // appends the decoded body bytes to out; false once the stream is malformed
    bool feed(const char *data, size_t length, std::string &out);
    // the last chunk and the trailer section have been read
    bool done() const { return state_ == State::Done; }

private:
    enum class State { Size, Extension, SizeLf, Data, DataCr, DataLf, Trailer, TrailerLf, Done, Error };
    State state_ = State::Size;
    unsigned long long remaining_ = 0;
    int digits_ = 0;
    bool trailerLineEmpty_ = true;
};

} // namespace abfetch
