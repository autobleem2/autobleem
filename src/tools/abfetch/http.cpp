#include "http.h"

#include <cctype>
#include <cstdlib>

using namespace std;

namespace abfetch {

namespace {

string lower(string text) {
    for (char &c : text)
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return text;
}

string trimmed(const string &text) {
    size_t from = text.find_first_not_of(" \t\r\n");
    if (from == string::npos)
        return "";
    size_t to = text.find_last_not_of(" \t\r\n");
    return text.substr(from, to - from + 1);
}

bool allDigits(const string &text) {
    if (text.empty())
        return false;
    for (char c : text)
        if (!isdigit(static_cast<unsigned char>(c)))
            return false;
    return true;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

} // namespace

//*******************************
// Url
//*******************************
string Url::authority() const {
    string name = host.find(':') != string::npos ? "[" + host + "]" : host;
    return defaultPort() ? name : name + ":" + port;
}

//*******************************
// parseUrl
//*******************************
bool parseUrl(const string &text, Url &url) {
    string source = trimmed(text);
    size_t schemeEnd = source.find("://");
    if (schemeEnd == string::npos)
        return false;
    Url result;
    result.scheme = lower(source.substr(0, schemeEnd));
    if (result.scheme != "http" && result.scheme != "https")
        return false;
    string rest = source.substr(schemeEnd + 3);
    size_t authorityEnd = rest.find_first_of("/?#");
    string authority = rest.substr(0, authorityEnd);
    string remainder = authorityEnd == string::npos ? "" : rest.substr(authorityEnd);
    if (authority.find('@') != string::npos)
        return false; // no credentials in a URL

    string port;
    if (!authority.empty() && authority[0] == '[') {
        size_t close = authority.find(']');
        if (close == string::npos)
            return false;
        result.host = authority.substr(1, close - 1);
        string after = authority.substr(close + 1);
        if (!after.empty()) {
            if (after[0] != ':')
                return false;
            port = after.substr(1);
        }
    } else {
        size_t colon = authority.rfind(':');
        result.host = authority.substr(0, colon);
        if (colon != string::npos)
            port = authority.substr(colon + 1);
    }
    if (result.host.empty())
        return false;
    if (port.empty()) {
        result.port = result.tls() ? "443" : "80";
    } else {
        if (!allDigits(port) || port.size() > 5 || atoi(port.c_str()) < 1 || atoi(port.c_str()) > 65535)
            return false;
        result.port = to_string(atoi(port.c_str()));
    }

    size_t fragment = remainder.find('#');
    if (fragment != string::npos)
        remainder = remainder.substr(0, fragment);
    result.target = remainder.empty() || remainder[0] != '/' ? "/" + remainder : remainder;
    url = result;
    return true;
}

//*******************************
// resolveLocation
//*******************************
string resolveLocation(const Url &base, const string &location) {
    string where = trimmed(location);
    size_t schemeEnd = where.find("://");
    if (schemeEnd != string::npos && where.find_first_of("/?#") > schemeEnd)
        return where;
    if (where.compare(0, 2, "//") == 0)
        return base.scheme + ":" + where;
    string origin = base.scheme + "://" + base.authority();
    if (!where.empty() && where[0] == '/')
        return origin + where;
    string path = base.target.substr(0, base.target.find('?'));
    if (!where.empty() && where[0] == '?')
        return origin + path + where;
    return origin + path.substr(0, path.rfind('/') + 1) + where;
}

//*******************************
// buildRequest
//*******************************
string buildRequest(const Url &url, unsigned long long rangeFrom) {
    return "GET " + url.target + " HTTP/1.1\r\n" + "Host: " + url.authority() + "\r\n" +
           "User-Agent: abfetch/1 (AutoBleem)\r\n" + "Accept: */*\r\n" + "Accept-Encoding: identity\r\n" +
           (rangeFrom > 0 ? "Range: bytes=" + to_string(rangeFrom) + "-\r\n" : string()) + "Connection: close\r\n\r\n";
}

//*******************************
// ResponseHead
//*******************************
string ResponseHead::header(const string &name) const {
    for (const auto &h : headers)
        if (h.first == name)
            return h.second;
    return "";
}

bool ResponseHead::chunked() const {
    return lower(header("transfer-encoding")).find("chunked") != string::npos;
}

bool ResponseHead::contentLength(unsigned long long &length) const {
    string value = header("content-length");
    if (!allDigits(value) || value.size() > 19)
        return false;
    length = strtoull(value.c_str(), nullptr, 10);
    return true;
}

//*******************************
// headEnd
//*******************************
size_t headEnd(const string &buffer) {
    size_t crlf = buffer.find("\r\n\r\n");
    size_t lf = buffer.find("\n\n");
    if (crlf != string::npos && (lf == string::npos || crlf + 1 < lf))
        return crlf + 4;
    if (lf != string::npos)
        return lf + 2;
    return string::npos;
}

//*******************************
// parseHead
//*******************************
bool parseHead(const string &head, ResponseHead &out) {
    ResponseHead result;
    size_t pos = 0;
    bool first = true;
    while (pos < head.size()) {
        size_t end = head.find('\n', pos);
        string line = head.substr(pos, end == string::npos ? string::npos : end - pos);
        pos = end == string::npos ? head.size() : end + 1;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (first) {
            first = false;
            // HTTP/1.1 200 OK
            if (line.compare(0, 5, "HTTP/") != 0)
                return false;
            size_t space = line.find(' ');
            if (space == string::npos)
                return false;
            string code = line.substr(space + 1, 3);
            if (code.size() != 3 || !allDigits(code) || (line.size() > space + 4 && line[space + 4] != ' '))
                return false;
            result.status = atoi(code.c_str());
            continue;
        }
        if (line.empty())
            break;
        if (line[0] == ' ' || line[0] == '\t') { // an obsolete folded continuation
            if (result.headers.empty())
                return false;
            result.headers.back().second += " " + trimmed(line);
            continue;
        }
        size_t colon = line.find(':');
        if (colon == string::npos || colon == 0)
            return false;
        result.headers.emplace_back(lower(trimmed(line.substr(0, colon))), trimmed(line.substr(colon + 1)));
    }
    if (first)
        return false;
    out = result;
    return true;
}

//*******************************
// ChunkedDecoder::feed
//*******************************
bool ChunkedDecoder::feed(const char *data, size_t length, string &out) {
    size_t i = 0;
    auto sizeLineDone = [this]() {
        if (remaining_ == 0) {
            state_ = State::Trailer;
            trailerLineEmpty_ = true;
        } else {
            state_ = State::Data;
        }
    };
    auto trailerLineDone = [this]() {
        if (trailerLineEmpty_)
            state_ = State::Done;
        else {
            state_ = State::Trailer;
            trailerLineEmpty_ = true;
        }
    };
    while (i < length) {
        char c = data[i];
        switch (state_) {
        case State::Size: {
            int v = hexValue(c);
            if (v >= 0) {
                if (++digits_ > 15) {
                    state_ = State::Error;
                    return false;
                }
                remaining_ = remaining_ * 16 + static_cast<unsigned long long>(v);
            } else if (digits_ == 0) {
                state_ = State::Error;
                return false;
            } else if (c == ';' || c == ' ' || c == '\t') {
                state_ = State::Extension;
            } else if (c == '\r') {
                state_ = State::SizeLf;
            } else if (c == '\n') {
                sizeLineDone();
            } else {
                state_ = State::Error;
                return false;
            }
            i++;
            break;
        }
        case State::Extension:
            if (c == '\r')
                state_ = State::SizeLf;
            else if (c == '\n')
                sizeLineDone();
            i++;
            break;
        case State::SizeLf:
            if (c != '\n') {
                state_ = State::Error;
                return false;
            }
            sizeLineDone();
            i++;
            break;
        case State::Data: {
            size_t take = length - i;
            if (static_cast<unsigned long long>(take) > remaining_)
                take = static_cast<size_t>(remaining_);
            out.append(data + i, take);
            i += take;
            remaining_ -= take;
            if (remaining_ == 0)
                state_ = State::DataCr;
            break;
        }
        case State::DataCr:
        case State::DataLf:
            if (c == '\r' && state_ == State::DataCr) {
                state_ = State::DataLf;
            } else if (c == '\n') {
                state_ = State::Size;
                remaining_ = 0;
                digits_ = 0;
            } else {
                state_ = State::Error;
                return false;
            }
            i++;
            break;
        case State::Trailer:
            if (c == '\r')
                state_ = State::TrailerLf;
            else if (c == '\n')
                trailerLineDone();
            else
                trailerLineEmpty_ = false;
            i++;
            break;
        case State::TrailerLf:
            if (c != '\n') {
                state_ = State::Error;
                return false;
            }
            trailerLineDone();
            i++;
            break;
        case State::Done:
            return true; // anything after the last chunk is not the body's
        case State::Error:
            return false;
        }
    }
    return state_ != State::Error;
}

} // namespace abfetch
