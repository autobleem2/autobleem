// abfetch (src/tools/abfetch): the console's own downloader. The HTTP parts on their own - URLs, redirects,
// the response head, the chunked coding - and whole fetches over plain HTTP against a server on the
// loopback that answers with scripted bytes: bodies, redirects, errors, a short body, a stall. HTTPS is
// the same code over mbedTLS; it is tried against the real download site by hand (docs in the commit).

#include "doctest/doctest.h"
#include "fetch.h"
#include "http.h"
#include "support/temp_dir.h"

#include <mbedtls/net_sockets.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

using namespace abfetch;
using namespace std;

namespace {

//******************
// LoopbackServer
//******************
// one connection per Reply, in order: reads the request head, sends `bytes`, waits `pauseMs`, sends
// `tail`, closes. Gives up on a connection that does not come within 5 s, so a test that fails early
// does not hang in the destructor.
struct Reply {
    Reply(string bytesIn, int pause = 0, string tailIn = "") // NOLINT: a string is a reply
        : bytes(std::move(bytesIn)), pauseMs(pause), tail(std::move(tailIn)) {}
    string bytes;
    int pauseMs;
    string tail;
};

class LoopbackServer {
public:
    explicit LoopbackServer(vector<Reply> replies) : replies_(std::move(replies)) {
        mbedtls_net_init(&listen_);
        REQUIRE(mbedtls_net_bind(&listen_, "127.0.0.1", "0", MBEDTLS_NET_PROTO_TCP) == 0);
        sockaddr_in address{};
        socklen_t length = sizeof(address);
        getsockname(static_cast<decltype(socket(0, 0, 0))>(listen_.fd), reinterpret_cast<sockaddr *>(&address),
                    &length);
        port_ = ntohs(address.sin_port);
        thread_ = thread([this] { serve(); });
    }
    ~LoopbackServer() {
        if (thread_.joinable())
            thread_.join();
        mbedtls_net_free(&listen_);
    }
    LoopbackServer(const LoopbackServer &) = delete;
    LoopbackServer &operator=(const LoopbackServer &) = delete;

    string url(const string &path) const { return "http://127.0.0.1:" + to_string(port_) + path; }
    // the request heads received - waits for the server to have sent every reply
    vector<string> requests() {
        if (thread_.joinable())
            thread_.join();
        return requests_;
    }

private:
    void serve() {
        for (const Reply &reply : replies_) {
            if (mbedtls_net_poll(&listen_, MBEDTLS_NET_POLL_READ, 5000) <= 0)
                return;
            mbedtls_net_context client;
            mbedtls_net_init(&client);
            if (mbedtls_net_accept(&listen_, &client, nullptr, 0, nullptr) != 0)
                return;
            string head;
            unsigned char buffer[1024];
            while (head.find("\r\n\r\n") == string::npos) {
                int got = mbedtls_net_recv_timeout(&client, buffer, sizeof(buffer), 5000);
                if (got <= 0)
                    break;
                head.append(reinterpret_cast<char *>(buffer), static_cast<size_t>(got));
            }
            requests_.push_back(head);
            send(client, reply.bytes);
            if (reply.pauseMs > 0)
                this_thread::sleep_for(chrono::milliseconds(reply.pauseMs));
            send(client, reply.tail);
            mbedtls_net_free(&client);
        }
    }
    static void send(mbedtls_net_context &client, const string &bytes) {
        size_t done = 0;
        while (done < bytes.size()) {
            int sent = mbedtls_net_send(&client, reinterpret_cast<const unsigned char *>(bytes.data()) + done,
                                        bytes.size() - done);
            if (sent <= 0)
                return;
            done += static_cast<size_t>(sent);
        }
    }

    vector<Reply> replies_;
    vector<string> requests_;
    mbedtls_net_context listen_;
    int port_ = 0;
    thread thread_;
};

string ok(const string &body) {
    return "HTTP/1.1 200 OK\r\nContent-Length: " + to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
}

Options optionsFor(const string &url, const string &output) {
    Options options;
    options.url = url;
    options.output = output;
    options.connectTimeout = 5;
    options.stallTimeout = 5;
    return options;
}

bool exists(const string &path) {
    return ableem::DirEntry::exists(path);
}

} // namespace

TEST_CASE("parseUrl: scheme, host, port and target") {
    Url url;
    REQUIRE(parseUrl("https://autobleem.retromenele.pl/nightly/latest.json", url));
    CHECK(url.scheme == "https");
    CHECK(url.host == "autobleem.retromenele.pl");
    CHECK(url.port == "443");
    CHECK(url.target == "/nightly/latest.json");
    CHECK(url.tls());
    CHECK(url.authority() == "autobleem.retromenele.pl");

    REQUIRE(parseUrl("HTTP://212.71.244.78:9090", url));
    CHECK(url.scheme == "http");
    CHECK(url.port == "9090");
    CHECK(url.target == "/");
    CHECK(url.authority() == "212.71.244.78:9090");

    REQUIRE(parseUrl("http://example.org?a=1#frag", url));
    CHECK(url.target == "/?a=1");
    REQUIRE(parseUrl("http://example.org:80/x#y", url));
    CHECK(url.authority() == "example.org");
    CHECK(url.target == "/x");

    REQUIRE(parseUrl("https://[::1]:8443/a", url));
    CHECK(url.host == "::1");
    CHECK(url.authority() == "[::1]:8443");

    CHECK_FALSE(parseUrl("ftp://example.org/x", url));
    CHECK_FALSE(parseUrl("example.org/x", url));
    CHECK_FALSE(parseUrl("http:///x", url));
    CHECK_FALSE(parseUrl("http://user:pw@example.org/", url));
    CHECK_FALSE(parseUrl("http://example.org:0/", url));
    CHECK_FALSE(parseUrl("http://example.org:99999/", url));
    CHECK_FALSE(parseUrl("http://example.org:8a/", url));
}

TEST_CASE("resolveLocation: every form a Location header comes in") {
    Url base;
    REQUIRE(parseUrl("https://site.example/releases/v1/file.tar.gz?x=1", base));
    CHECK(resolveLocation(base, "http://other.example/y") == "http://other.example/y");
    CHECK(resolveLocation(base, "//cdn.example/z") == "https://cdn.example/z");
    CHECK(resolveLocation(base, "/nightly/latest.json") == "https://site.example/nightly/latest.json");
    CHECK(resolveLocation(base, "other.tar.gz") == "https://site.example/releases/v1/other.tar.gz");
    CHECK(resolveLocation(base, "?y=2") == "https://site.example/releases/v1/file.tar.gz?y=2");
    CHECK(resolveLocation(base, "  /spaced  ") == "https://site.example/spaced");
}

TEST_CASE("buildRequest: one GET with the Host header, the connection closed after") {
    Url url;
    REQUIRE(parseUrl("http://127.0.0.1:8080/a/b?c", url));
    string request = buildRequest(url);
    CHECK(request.compare(0, 24, "GET /a/b?c HTTP/1.1\r\nHos") == 0);
    CHECK(request.find("\r\nHost: 127.0.0.1:8080\r\n") != string::npos);
    CHECK(request.find("\r\nConnection: close\r\n") != string::npos);
    CHECK(request.find("\r\nAccept-Encoding: identity\r\n") != string::npos);
    CHECK(request.substr(request.size() - 4) == "\r\n\r\n");
}

TEST_CASE("parseHead: the status and the headers") {
    ResponseHead head;
    REQUIRE(parseHead("HTTP/1.1 302 Found\r\nLocation: /x\r\nContent-Length:  12 \r\nX-Folded: a\r\n b\r\n", head));
    CHECK(head.status == 302);
    CHECK(head.redirect());
    CHECK(head.header("location") == "/x");
    CHECK(head.header("x-folded") == "a b");
    unsigned long long length = 0;
    CHECK(head.contentLength(length));
    CHECK(length == 12);
    CHECK_FALSE(head.chunked());
    CHECK(head.header("missing").empty());

    REQUIRE(parseHead("HTTP/1.0 200\nTransfer-Encoding: gzip, Chunked\n", head));
    CHECK(head.status == 200);
    CHECK(head.chunked());
    CHECK_FALSE(head.contentLength(length));

    CHECK_FALSE(parseHead("", head));
    CHECK_FALSE(parseHead("SSH-2.0-OpenSSH\r\n", head));
    CHECK_FALSE(parseHead("HTTP/1.1 2x0 OK\r\n", head));
    CHECK_FALSE(parseHead("HTTP/1.1 200 OK\r\nno colon here\r\n", head));
}

TEST_CASE("headEnd: where the body starts, once the head is all there") {
    CHECK(headEnd("HTTP/1.1 200 OK\r\nA: b\r\n") == string::npos);
    CHECK(headEnd("HTTP/1.1 200 OK\r\nA: b\r\n\r\nbody") == 25);
    CHECK(headEnd("HTTP/1.1 200 OK\nA: b\n\nbody") == 22);
}

TEST_CASE("ChunkedDecoder: whole, byte by byte, with extensions and trailers") {
    const string stream =
        "4\r\nWiki\r\n6;name=value\r\npedia \r\nE\r\nin \r\n\r\nchunks.\r\n0\r\nExpires: never\r\n\r\n";
    const string body = "Wikipedia in \r\n\r\nchunks.";
    {
        ChunkedDecoder decoder;
        string out;
        CHECK(decoder.feed(stream.data(), stream.size(), out));
        CHECK(decoder.done());
        CHECK(out == body);
    }
    {
        ChunkedDecoder decoder;
        string out;
        for (char c : stream)
            REQUIRE(decoder.feed(&c, 1, out));
        CHECK(decoder.done());
        CHECK(out == body);
    }
    {
        ChunkedDecoder decoder; // not finished: no last chunk yet
        string out;
        string part = "3\r\nabc\r\n";
        CHECK(decoder.feed(part.data(), part.size(), out));
        CHECK_FALSE(decoder.done());
        CHECK(out == "abc");
    }
    for (const string bad : {"x\r\n", "3\r\nabcX\r\n", "\r\n", "1234567890abcdef0\r\n"}) {
        ChunkedDecoder decoder;
        string out;
        CHECK_FALSE(decoder.feed(bad.data(), bad.size(), out));
    }
}

TEST_CASE("fetch: a body with a Content-Length lands in the file") {
    TempDir tmp("abfetch");
    const string body(100000, 'x');
    LoopbackServer server({{ok(body)}});
    string error;
    CHECK(fetch(optionsFor(server.url("/pkg.tar.gz"), tmp.at("out.part")), error) == Ok);
    CHECK(tmp.readFile("out.part") == body);
    auto requests = server.requests();
    REQUIRE(requests.size() == 1);
    CHECK(requests[0].compare(0, 24, "GET /pkg.tar.gz HTTP/1.1") == 0);
}

TEST_CASE("fetch: a chunked body, in pieces") {
    TempDir tmp("abfetch");
    LoopbackServer server(
        {{"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello", 50, "\r\n6\r\n world\r\n0\r\n\r\n"}});
    string error;
    CHECK(fetch(optionsFor(server.url("/x"), tmp.at("out")), error) == Ok);
    CHECK(tmp.readFile("out") == "hello world");
}

TEST_CASE("fetch: no length at all - the close is the end") {
    TempDir tmp("abfetch");
    LoopbackServer server({{"HTTP/1.0 200 OK\r\n\r\nuntil the end"}});
    string error;
    CHECK(fetch(optionsFor(server.url("/x"), tmp.at("out")), error) == Ok);
    CHECK(tmp.readFile("out") == "until the end");
}

TEST_CASE("fetch: redirects are followed, relative ones against the answering URL") {
    TempDir tmp("abfetch");
    LoopbackServer server({{"HTTP/1.1 302 Found\r\nLocation: latest.json\r\nContent-Length: 0\r\n\r\n"},
                           {"HTTP/1.1 100 Continue\r\n\r\n" + ok("{}")}});
    string error;
    CHECK(fetch(optionsFor(server.url("/nightly/old.json"), tmp.at("out")), error) == Ok);
    CHECK(tmp.readFile("out") == "{}");
    auto requests = server.requests();
    REQUIRE(requests.size() == 2);
    CHECK(requests[1].compare(0, 30, "GET /nightly/latest.json HTTP/") == 0);
}

TEST_CASE("fetch: too many redirects") {
    TempDir tmp("abfetch");
    const Reply loop{"HTTP/1.1 301 Moved\r\nLocation: /again\r\n\r\n"};
    LoopbackServer server({loop, loop, loop});
    Options options = optionsFor(server.url("/"), tmp.at("out"));
    options.maxRedirects = 2;
    string error;
    CHECK(fetch(options, error) == TooManyRedirects);
    CHECK_FALSE(exists(tmp.at("out")));
}

TEST_CASE("fetch: an HTTP error writes nothing") {
    TempDir tmp("abfetch");
    LoopbackServer server({{"HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nnot found"}});
    string error;
    CHECK(fetch(optionsFor(server.url("/missing"), tmp.at("out")), error) == HttpError);
    CHECK(error.find("404") != string::npos);
    CHECK_FALSE(exists(tmp.at("out")));
}

TEST_CASE("fetch: a short body is a failure and its file goes") {
    TempDir tmp("abfetch");
    LoopbackServer server({{"HTTP/1.1 200 OK\r\nContent-Length: 1000\r\n\r\nonly this much"}});
    string error;
    CHECK(fetch(optionsFor(server.url("/x"), tmp.at("out.part")), error) == Incomplete);
    CHECK(error.find("14 of 1000") != string::npos);
    CHECK_FALSE(exists(tmp.at("out.part")));
}

TEST_CASE("fetch: a stall gives up after the stall timeout") {
    TempDir tmp("abfetch");
    LoopbackServer server({{"HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nabc", 3000, "defghij"}});
    Options options = optionsFor(server.url("/x"), tmp.at("out"));
    options.stallTimeout = 1;
    string error;
    auto start = chrono::steady_clock::now();
    CHECK(fetch(options, error) == NetworkError);
    CHECK(chrono::steady_clock::now() - start < chrono::milliseconds(2800));
    CHECK_FALSE(exists(tmp.at("out")));
}

TEST_CASE("fetch: nothing listening, a malformed answer, bad input") {
    TempDir tmp("abfetch");
    string error;
    int port = 0;
    {
        LoopbackServer server({}); // bound, then closed: nothing listens there any more
        port = stoi(server.url("").substr(17));
    }
    CHECK(fetch(optionsFor("http://127.0.0.1:" + to_string(port) + "/", tmp.at("out")), error) == NetworkError);

    {
        LoopbackServer server({{"this is not HTTP\r\n\r\n"}});
        CHECK(fetch(optionsFor(server.url("/"), tmp.at("out")), error) == Incomplete);
    }
    CHECK(fetch(optionsFor("gopher://x/", tmp.at("out")), error) == BadUsage);
    Options https = optionsFor("https://127.0.0.1:1/", tmp.at("out"));
    https.caFile = tmp.at("no-such-bundle.pem");
    CHECK(fetch(https, error) == TlsError);
    CHECK(error.find("CA bundle") != string::npos);
    CHECK_FALSE(exists(tmp.at("out")));
}
