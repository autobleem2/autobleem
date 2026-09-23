#include "fetch.h"

#include "http.h"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fcntl.h>
#include <io.h>
#else
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

using namespace std;

namespace abfetch {

namespace {

#ifdef _WIN32
using Socket = SOCKET;
const Socket NoSocket = INVALID_SOCKET;
void closeSocket(Socket s) {
    closesocket(s);
}
bool socketsReady() {
    static const bool ready = [] {
        WSADATA data;
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    return ready;
}
void setBlocking(Socket s, bool blocking) {
    u_long mode = blocking ? 0 : 1;
    ioctlsocket(s, FIONBIO, &mode);
}
bool connectPending() {
    return WSAGetLastError() == WSAEWOULDBLOCK;
}
void setSendTimeout(Socket s, int seconds) {
    DWORD ms = static_cast<DWORD>(seconds) * 1000;
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&ms), sizeof(ms));
}
#else
using Socket = int;
const Socket NoSocket = -1;
void closeSocket(Socket s) {
    close(s);
}
bool socketsReady() {
    return true;
}
void setBlocking(Socket s, bool blocking) {
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK));
}
bool connectPending() {
    return errno == EINPROGRESS;
}
void setSendTimeout(Socket s, int seconds) {
    timeval tv{};
    tv.tv_sec = seconds;
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}
#endif

string tlsMessage(int code) {
    char text[200];
    mbedtls_strerror(code, text, sizeof(text));
    return text;
}

//*******************************
// connectTo
//*******************************
// every address the name has, each given timeoutSeconds, the first that answers
Socket connectTo(const Url &url, int timeoutSeconds, string &error) {
    if (!socketsReady()) {
        error = "the network could not be initialised";
        return NoSocket;
    }
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo *addresses = nullptr;
    if (getaddrinfo(url.host.c_str(), url.port.c_str(), &hints, &addresses) != 0 || addresses == nullptr) {
        error = "cannot resolve " + url.host;
        return NoSocket;
    }
    Socket result = NoSocket;
    error = "cannot connect to " + url.authority();
    for (addrinfo *a = addresses; a != nullptr && result == NoSocket; a = a->ai_next) {
        Socket s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s == NoSocket)
            continue;
        setBlocking(s, false);
        bool connected = connect(s, a->ai_addr, static_cast<int>(a->ai_addrlen)) == 0;
        if (!connected && connectPending()) {
            fd_set writable, failed;
            FD_ZERO(&writable);
            FD_ZERO(&failed);
            FD_SET(s, &writable);
            FD_SET(s, &failed);
            timeval tv{};
            tv.tv_sec = timeoutSeconds;
            int ready = select(static_cast<int>(s) + 1, nullptr, &writable, &failed, &tv);
            if (ready > 0 && FD_ISSET(s, &writable)) {
                int code = 0;
                socklen_t length = sizeof(code);
                connected =
                    getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&code), &length) == 0 && code == 0;
            } else if (ready == 0) {
                error = "no answer from " + url.authority() + " in " + to_string(timeoutSeconds) + " s";
            }
        }
        if (connected) {
            setBlocking(s, true);
            result = s;
        } else {
            closeSocket(s);
        }
    }
    freeaddrinfo(addresses);
    return result;
}

//*******************************
// Trust
//*******************************
// the random generator and the CA bundle, made once per fetch and only when an https URL comes up
struct Trust {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_x509_crt ca;
    Trust() {
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);
        mbedtls_x509_crt_init(&ca);
    }
    ~Trust() {
        mbedtls_x509_crt_free(&ca);
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
    }
    Trust(const Trust &) = delete;
    Trust &operator=(const Trust &) = delete;

    bool load(const string &caFile, string &error) {
        static const char personal[] = "abfetch";
        int code = mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy,
                                         reinterpret_cast<const unsigned char *>(personal), sizeof(personal));
        if (code != 0) {
            error = "no random numbers: " + tlsMessage(code);
            return false;
        }
        if (caFile.empty()) {
            error = "no CA bundle given for an https URL";
            return false;
        }
        FILE *file = fopen(caFile.c_str(), "rb");
        if (file == nullptr) {
            error = "cannot read the CA bundle " + caFile + ": " + strerror(errno);
            return false;
        }
        string pem;
        char buffer[8192];
        size_t got;
        while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0)
            pem.append(buffer, got);
        fclose(file);
        // PEM is parsed with its terminating NUL counted; a positive return is the number of certificates
        // that did not parse - the rest are in
        code = mbedtls_x509_crt_parse(&ca, reinterpret_cast<const unsigned char *>(pem.c_str()), pem.size() + 1);
        if (code < 0) {
            error = "cannot read the CA bundle " + caFile + ": " + tlsMessage(code);
            return false;
        }
        return true;
    }
};

//*******************************
// Connection
//*******************************
class Connection {
public:
    Connection() {
        mbedtls_net_init(&net_);
        mbedtls_ssl_init(&ssl_);
        mbedtls_ssl_config_init(&conf_);
    }
    ~Connection() {
        if (tls_)
            mbedtls_ssl_close_notify(&ssl_);
        mbedtls_ssl_free(&ssl_);
        mbedtls_ssl_config_free(&conf_);
        mbedtls_net_free(&net_);
    }
    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    Result open(const Url &url, const Options &options, Trust *trust, string &error) {
        stallMs_ = static_cast<uint32_t>(options.stallTimeout) * 1000;
        Socket s = connectTo(url, options.connectTimeout, error);
        if (s == NoSocket)
            return NetworkError;
        setSendTimeout(s, options.stallTimeout);
        net_.fd = static_cast<int>(s);
        if (!url.tls())
            return Ok;

        int code = mbedtls_ssl_config_defaults(&conf_, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                               MBEDTLS_SSL_PRESET_DEFAULT);
        if (code != 0) {
            error = "TLS set-up: " + tlsMessage(code);
            return TlsError;
        }
        mbedtls_ssl_conf_authmode(&conf_, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&conf_, &trust->ca, nullptr);
        mbedtls_ssl_conf_rng(&conf_, mbedtls_ctr_drbg_random, &trust->drbg);
        mbedtls_ssl_conf_read_timeout(&conf_, stallMs_);
        if ((code = mbedtls_ssl_setup(&ssl_, &conf_)) != 0 ||
            (code = mbedtls_ssl_set_hostname(&ssl_, url.host.c_str())) != 0) {
            error = "TLS set-up: " + tlsMessage(code);
            return TlsError;
        }
        mbedtls_ssl_set_bio(&ssl_, &net_, mbedtls_net_send, nullptr, mbedtls_net_recv_timeout);
        while ((code = mbedtls_ssl_handshake(&ssl_)) != 0) {
            if (code == MBEDTLS_ERR_SSL_WANT_READ || code == MBEDTLS_ERR_SSL_WANT_WRITE)
                continue;
            if (code == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
                char why[512];
                mbedtls_x509_crt_verify_info(why, sizeof(why), "", mbedtls_ssl_get_verify_result(&ssl_));
                string reason = why;
                while (!reason.empty() && (reason.back() == '\n' || reason.back() == ' '))
                    reason.pop_back();
                error = url.host + "'s certificate is not trusted: " + reason;
            } else if (code == MBEDTLS_ERR_SSL_TIMEOUT) {
                error = "the TLS handshake with " + url.host + " stalled";
                return NetworkError;
            } else {
                error = "TLS handshake with " + url.host + ": " + tlsMessage(code);
            }
            return TlsError;
        }
        tls_ = true;
        return Ok;
    }

    Result sendAll(const string &data, string &error) {
        size_t done = 0;
        while (done < data.size()) {
            const auto *bytes = reinterpret_cast<const unsigned char *>(data.data()) + done;
            int code = tls_ ? mbedtls_ssl_write(&ssl_, bytes, data.size() - done)
                            : mbedtls_net_send(&net_, bytes, data.size() - done);
            if (code == MBEDTLS_ERR_SSL_WANT_READ || code == MBEDTLS_ERR_SSL_WANT_WRITE)
                continue;
            if (code <= 0) {
                error = "sending the request: " + tlsMessage(code);
                return NetworkError;
            }
            done += static_cast<size_t>(code);
        }
        return Ok;
    }

    // > 0 bytes read, 0 the server closed the connection, < 0 failed (error says why)
    int receive(unsigned char *buffer, size_t size, string &error) {
        for (;;) {
            int code =
                tls_ ? mbedtls_ssl_read(&ssl_, buffer, size) : mbedtls_net_recv_timeout(&net_, buffer, size, stallMs_);
            if (code > 0)
                return code;
            if (code == MBEDTLS_ERR_SSL_WANT_READ || code == MBEDTLS_ERR_SSL_WANT_WRITE)
                continue;
            if (code == 0 || code == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || code == MBEDTLS_ERR_NET_CONN_RESET)
                return 0; // whether that was the whole body is the caller's to say
            error = code == MBEDTLS_ERR_SSL_TIMEOUT ? "nothing received for " + to_string(stallMs_ / 1000) + " s"
                                                    : "receiving: " + tlsMessage(code);
            return -1;
        }
    }

private:
    mbedtls_net_context net_;
    mbedtls_ssl_context ssl_;
    mbedtls_ssl_config conf_;
    bool tls_ = false;
    uint32_t stallMs_ = 60000;
};

//*******************************
// Sink
//*******************************
// the output, opened when a body starts; removed unless close() says it was all written
class Sink {
public:
    explicit Sink(string path) : path_(std::move(path)) {}
    ~Sink() { discard(); }
    Sink(const Sink &) = delete;
    Sink &operator=(const Sink &) = delete;

    bool open() {
        if (path_ == "-") {
#ifdef _WIN32
            _setmode(_fileno(stdout), _O_BINARY);
#endif
            file_ = stdout;
            return true;
        }
        file_ = fopen(path_.c_str(), "wb");
        opened_ = file_ != nullptr;
        return opened_;
    }
    bool write(const char *data, size_t size) { return size == 0 || fwrite(data, 1, size, file_) == size; }
    bool close() {
        bool ok = fflush(file_) == 0;
        if (file_ != stdout)
            ok = fclose(file_) == 0 && ok;
        file_ = nullptr;
        kept_ = ok;
        return ok;
    }

private:
    void discard() {
        if (file_ != nullptr && file_ != stdout)
            fclose(file_);
        file_ = nullptr;
        if (opened_ && !kept_)
            remove(path_.c_str());
    }
    string path_;
    FILE *file_ = nullptr;
    bool opened_ = false; // a file of ours exists (never stdout)
    bool kept_ = false;
};

const size_t MaxHead = 64 * 1024;

} // namespace

//*******************************
// fetch
//*******************************
Result fetch(const Options &options, string &error) {
    Url url;
    if (!parseUrl(options.url, url)) {
        error = "not an http(s) URL: " + options.url;
        return BadUsage;
    }
    unique_ptr<Trust> trust;
    for (int hop = 0; hop <= options.maxRedirects; hop++) {
        if (url.tls() && !trust) {
            trust.reset(new Trust());
            if (!trust->load(options.caFile, error))
                return TlsError;
        }
        Connection connection;
        Result result = connection.open(url, options, trust.get(), error);
        if (result != Ok)
            return result;
        if ((result = connection.sendAll(buildRequest(url), error)) != Ok)
            return result;

        // the head (skipping any 1xx interim response)
        string buffer;
        ResponseHead head;
        unsigned char chunk[16384];
        for (;;) {
            size_t end = headEnd(buffer);
            if (end != string::npos) {
                if (!parseHead(buffer.substr(0, end), head)) {
                    error = "a malformed response from " + url.host;
                    return Incomplete;
                }
                buffer.erase(0, end);
                if (head.status >= 200 || head.status < 100)
                    break;
                continue;
            }
            if (buffer.size() > MaxHead) {
                error = "a response head of more than 64 KB from " + url.host;
                return Incomplete;
            }
            int got = connection.receive(chunk, sizeof(chunk), error);
            if (got < 0)
                return NetworkError;
            if (got == 0) {
                error = url.host + " closed the connection without an answer";
                return NetworkError;
            }
            buffer.append(reinterpret_cast<const char *>(chunk), static_cast<size_t>(got));
        }

        if (head.redirect() && !head.header("location").empty()) {
            string next = resolveLocation(url, head.header("location"));
            if (!parseUrl(next, url)) {
                error = "a redirect to something that is not an http(s) URL: " + next;
                return HttpError;
            }
            continue;
        }
        if (head.status < 200 || head.status >= 300) {
            error = "the server answered " + to_string(head.status) + " for " + url.str();
            return HttpError;
        }

        // the body
        Sink sink(options.output);
        if (!sink.open()) {
            error = "cannot write " + options.output + ": " + strerror(errno);
            return WriteError;
        }
        unsigned long long length = 0;
        const bool chunked = head.chunked();
        const bool sized = !chunked && head.contentLength(length);
        unsigned long long written = 0;
        ChunkedDecoder decoder;
        string decoded;
        // one piece of what arrived: through the decoder, or up to Content-Length, into the file
        auto take = [&](const char *data, size_t size) -> Result {
            if (chunked) {
                decoded.clear();
                if (!decoder.feed(data, size, decoded)) {
                    error = "a broken chunked response from " + url.host;
                    return Incomplete;
                }
                data = decoded.data();
                size = decoded.size();
            } else if (sized && written + size > length) {
                size = static_cast<size_t>(length - written);
            }
            if (!sink.write(data, size)) {
                error = "cannot write " + options.output + ": " + strerror(errno);
                return WriteError;
            }
            written += size;
            return Ok;
        };
        auto complete = [&]() { return chunked ? decoder.done() : sized && written >= length; };

        if ((result = take(buffer.data(), buffer.size())) != Ok)
            return result;
        while (!complete()) {
            int got = connection.receive(chunk, sizeof(chunk), error);
            if (got < 0)
                return NetworkError;
            if (got == 0) {
                if (chunked || sized) {
                    error = url.host + " closed the connection after " + to_string(written) + " of " +
                            (sized ? to_string(length) : string("a chunked")) + " bytes";
                    return Incomplete;
                }
                break; // no length given: the close is the end
            }
            if ((result = take(reinterpret_cast<const char *>(chunk), static_cast<size_t>(got))) != Ok)
                return result;
        }
        if (!sink.close()) {
            error = "cannot write " + options.output + ": " + strerror(errno);
            return WriteError;
        }
        return Ok;
    }
    error = "more than " + to_string(options.maxRedirects) + " redirects";
    return TooManyRedirects;
}

} // namespace abfetch
