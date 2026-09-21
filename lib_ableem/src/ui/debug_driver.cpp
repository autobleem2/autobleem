//
// DebugDriver: the TCP line server that drives the program for automated UI tests. See the header.
//
#include "ableem/ui/debug_driver.h"
#include "ableem/ui/input.h"
#include "ableem/ui/platform.h"
#include "ableem/ui/renderer.h"

#include <ableem/engine/log.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define CLOSESOCK closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define INVALID_SOCKET (-1)
#define CLOSESOCK close
#endif

using namespace std;

namespace ableem {

namespace {
std::mutex screenMutex;
std::vector<std::string> screenStack;

// "11GuiLauncher" (gcc) / "class GuiLauncher" (msvc) -> "GuiLauncher"
std::string plainName(const char *typeName) {
    std::string s = typeName;
    if (s.compare(0, 6, "class ") == 0)
        s.erase(0, 6);
    size_t i = 0;
    while (i < s.size() && isdigit(static_cast<unsigned char>(s[i])))
        i++;
    return s.substr(i);
}

// the button names the client uses
bool buttonFor(const string &name, Button &button, bool &dpad) {
    dpad = false;
    if (name == "x" || name == "cross")
        button = Button::Cross;
    else if (name == "o" || name == "circle")
        button = Button::Circle;
    else if (name == "s" || name == "square")
        button = Button::Square;
    else if (name == "t" || name == "triangle")
        button = Button::Triangle;
    else if (name == "start")
        button = Button::Start;
    else if (name == "select")
        button = Button::Select;
    else if (name == "l1")
        button = Button::L1;
    else if (name == "r1")
        button = Button::R1;
    else if (name == "l2")
        button = Button::L2;
    else if (name == "r2")
        button = Button::R2;
    else {
        dpad = true;
        if (name == "up")
            button = Button::DpadUp;
        else if (name == "down")
            button = Button::DpadDown;
        else if (name == "left")
            button = Button::DpadLeft;
        else if (name == "right")
            button = Button::DpadRight;
        else
            return false;
    }
    return true;
}

bool keyFor(const string &name, Key &key) {
    static const struct {
        const char *name;
        Key key;
    } keys[] = {{"escape", Key::Escape},       {"return", Key::Return}, {"enter", Key::Return}, {"up", Key::Up},
                {"down", Key::Down},           {"left", Key::Left},     {"right", Key::Right},  {"pageup", Key::PageUp},
                {"pagedown", Key::PageDown},   {"home", Key::Home},     {"end", Key::End},      {"tab", Key::Tab},
                {"backspace", Key::Backspace}, {"delete", Key::Delete}};
    for (const auto &k : keys) {
        if (name == k.name) {
            key = k.key;
            return true;
        }
    }
    return false;
}

void sleepMs(int ms) {
    this_thread::sleep_for(chrono::milliseconds(ms));
}

class Server {
public:
    Server(GuiBase &gui, int port) : gui_(gui), port_(port) {}

    bool listen() {
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            return false;
#endif
        listener_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listener_ == INVALID_SOCKET)
            return false;
        int one = 1;
        setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&one), sizeof(one));
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<unsigned short>(port_));
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(listener_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
            CLOSESOCK(listener_);
            return false;
        }
        if (::listen(listener_, 1) != 0) {
            CLOSESOCK(listener_);
            return false;
        }
        return true;
    }

    void run() {
        gui_.renderer().setFrameCache(true);
        for (;;) {
            sock_t client = accept(listener_, nullptr, nullptr);
            if (client == INVALID_SOCKET)
                continue;
            serve(client);
            CLOSESOCK(client);
        }
    }

private:
    void serve(sock_t client) {
        string buffer;
        char chunk[512];
        for (;;) {
            size_t nl = buffer.find('\n');
            while (nl == string::npos) {
                int n = recv(client, chunk, sizeof(chunk), 0);
                if (n <= 0)
                    return;
                buffer.append(chunk, static_cast<size_t>(n));
                nl = buffer.find('\n');
            }
            string line = buffer.substr(0, nl);
            buffer.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            string reply = handle(line) + "\n";
            send(client, reply.c_str(), static_cast<int>(reply.size()), 0);
        }
    }

    void injectButton(Button button, bool dpad, bool down) {
        Event e;
        e.button = button;
        if (dpad)
            e.type = down ? Event::Type::DpadDown : Event::Type::DpadUp;
        else
            e.type = down ? Event::Type::ButtonDown : Event::Type::ButtonUp;
        gui_.input().inject(e);
        lastInputFrame_ = gui_.renderer().frameCount();
    }

    string handle(const string &line) {
        istringstream in(line);
        string cmd;
        in >> cmd;
        if (cmd.empty() || cmd == "ping")
            return "ok";
        if (cmd == "press" || cmd == "down" || cmd == "up") {
            string name;
            in >> name;
            Button button;
            bool dpad;
            if (!buttonFor(name, button, dpad))
                return "err unknown button " + name;
            if (cmd == "press") {
                int ms = 60;
                in >> ms;
                injectButton(button, dpad, true);
                sleepMs(ms);
                injectButton(button, dpad, false);
            } else {
                injectButton(button, dpad, cmd == "down");
            }
            return "ok";
        }
        if (cmd == "key") {
            string name;
            in >> name;
            Key key;
            if (!keyFor(name, key))
                return "err unknown key " + name;
            Event e;
            e.type = Event::Type::KeyDown;
            e.key = key;
            gui_.input().inject(e);
            e.type = Event::Type::KeyUp;
            gui_.input().inject(e);
            lastInputFrame_ = gui_.renderer().frameCount();
            return "ok";
        }
        if (cmd == "text") {
            string text;
            getline(in, text);
            if (!text.empty() && text[0] == ' ')
                text.erase(0, 1);
            Event e;
            e.type = Event::Type::TextInput;
            e.text = text;
            gui_.input().inject(e);
            lastInputFrame_ = gui_.renderer().frameCount();
            return "ok";
        }
        if (cmd == "wait") {
            int ms = 0;
            in >> ms;
            sleepMs(ms);
            return "ok";
        }
        if (cmd == "frames")
            return "ok " + to_string(gui_.renderer().frameCount());
        if (cmd == "screen")
            return "ok " + DebugDriver::currentScreen();
        if (cmd == "shot") {
            string path;
            getline(in, path);
            if (!path.empty() && path[0] == ' ')
                path.erase(0, 1);
            if (path.empty())
                return "err no path";
            // a frame newer than the last input, if one comes in time
            for (int i = 0; i < 40 && gui_.renderer().frameCount() <= lastInputFrame_; i++)
                sleepMs(10);
            return gui_.renderer().saveLastFrame(path) ? "ok " + path : "err no frame";
        }
        if (cmd == "window") {
            string what;
            in >> what;
            if (what == "hide")
                gui_.platform().hideWindow();
            else if (what == "show")
                gui_.platform().showWindow();
            else if (what == "min")
                gui_.platform().minimizeWindow();
            else if (what == "restore")
                gui_.platform().restoreWindow();
            else
                return "err window hide|show|min|restore";
            return "ok";
        }
        if (cmd == "quit") {
            Event e;
            e.type = Event::Type::Quit;
            gui_.input().inject(e);
            return "ok";
        }
        return "err unknown command " + cmd;
    }

    GuiBase &gui_;
    int port_;
    sock_t listener_ = INVALID_SOCKET;
    unsigned long lastInputFrame_ = 0;
};

} // namespace

//*******************************
// DebugDriver::pushScreen / popScreen / currentScreen
//*******************************
void DebugDriver::pushScreen(const char *typeName) {
    lock_guard<mutex> lock(screenMutex);
    screenStack.push_back(plainName(typeName));
}

void DebugDriver::popScreen() {
    lock_guard<mutex> lock(screenMutex);
    if (!screenStack.empty())
        screenStack.pop_back();
}

string DebugDriver::currentScreen() {
    lock_guard<mutex> lock(screenMutex);
    return screenStack.empty() ? string("none") : screenStack.back();
}

//*******************************
// DebugDriver::start
//*******************************
bool DebugDriver::start(GuiBase &gui, int port) {
    static unique_ptr<Server> server; // lives as long as the process: the thread never ends
    server = make_unique<Server>(gui, port);
    if (!server->listen()) {
        PLOG_ERROR << "DebugDriver: cannot listen on 127.0.0.1:" << port;
        server.reset();
        return false;
    }
    PLOG_INFO << "DebugDriver listening on 127.0.0.1:" << port;
    Server *s = server.get();
    thread([s]() { s->run(); }).detach();
    return true;
}

} // namespace ableem
