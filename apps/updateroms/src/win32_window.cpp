//
// The UpdateRoms window - see the header. The job runs on a std::thread and reports into a mutex-guarded
// State; a 100 ms timer moves that into the controls. The window is the report: what it is on, how far,
// every line the job said, and a button that is Stop while it runs and Close when it is done.
//
#ifdef _WIN32

#include "win32_window.h"

#include <ableem/engine/game_scanner.h>
#include <ableem/engine/log.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std;

namespace {

const char *const WindowClass = "AutoBleemUpdateRoms";
const int IdButton = 100;
const int IdTimer = 1;
const int Margin = 12;

//******************
// State
//******************
// what the worker thread writes and the timer reads
struct State {
    mutex m;
    string stage;
    int done = 0, total = 0;
    vector<string> lines; // every report line; `shown` of them are in the log control already
    size_t shown = 0;
    atomic<bool> stop{false};
    atomic<bool> finished{false};
};

//******************
// Listener
//******************
class Listener : public ableem::ScanProgressListener {
public:
    explicit Listener(State &state) : state_(state) {}
    void onScanProgress(ableem::ScanStage stage, const string &detail, int done, int total) override {
        lock_guard<mutex> lock(state_.m);
        if (stage == ableem::ScanStage::ScanningRoms)
            state_.stage = "Scanning " + detail;
        else if (stage == ableem::ScanStage::FetchingBoxArt)
            state_.stage = "Fetching box art: " + detail;
        else
            state_.stage = detail;
        state_.done = done;
        state_.total = total;
    }

private:
    State &state_;
};

//******************
// Window
//******************
struct Window {
    HWND hwnd = nullptr, stageLabel = nullptr, bar = nullptr, log = nullptr, button = nullptr;
    HFONT font = nullptr;
    State state;
    thread worker;
    UpdateRomsJob::Setup setup;
    UpdateRomsJob::Report report;
};

// UTF-8 (every path and label in the job is UTF-8) to the wide strings the W controls take
wstring wide(const string &utf8) {
    if (utf8.empty())
        return wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &out[0], n);
    return out;
}

void layout(Window &w) {
    RECT r;
    GetClientRect(w.hwnd, &r);
    const int width = r.right - r.left, height = r.bottom - r.top;
    const int x = Margin, inner = width - 2 * Margin;
    int y = Margin;
    MoveWindow(w.stageLabel, x, y, inner, 20, TRUE);
    y += 26;
    MoveWindow(w.bar, x, y, inner, 18, TRUE);
    y += 28;
    const int buttonH = 26, buttonW = 90;
    MoveWindow(w.log, x, y, inner, height - y - buttonH - 2 * Margin, TRUE);
    MoveWindow(w.button, width - Margin - buttonW, height - Margin - buttonH, buttonW, buttonH, TRUE);
}

void startJob(Window &w) {
    w.worker = thread([&w]() {
        Listener listener(w.state);
        UpdateRomsJob::Report result = UpdateRomsJob::run(
            w.setup, &listener, [&w]() { return w.state.stop.load(); },
            [&w](const string &line) {
                lock_guard<mutex> lock(w.state.m);
                w.state.lines.push_back(line);
            });
        {
            lock_guard<mutex> lock(w.state.m);
            w.report = result;
            w.state.stage = w.state.stop.load() ? "Stopped" : "Done";
            w.state.done = w.state.total = 0;
        }
        w.state.finished.store(true);
    });
}

// the timer: whatever changed since the last tick goes into the controls
void refresh(Window &w) {
    string stage;
    int done, total;
    vector<string> fresh;
    bool finished = w.state.finished.load();
    {
        lock_guard<mutex> lock(w.state.m);
        stage = w.state.stage;
        done = w.state.done;
        total = w.state.total;
        for (size_t i = w.state.shown; i < w.state.lines.size(); i++)
            fresh.push_back(w.state.lines[i]);
        w.state.shown = w.state.lines.size();
    }
    SetWindowTextW(w.stageLabel, wide(stage.empty() ? "Starting..." : stage).c_str());
    if (total > 0) {
        SendMessage(w.bar, PBM_SETMARQUEE, FALSE, 0);
        SetWindowLongPtr(w.bar, GWL_STYLE, GetWindowLongPtr(w.bar, GWL_STYLE) & ~PBS_MARQUEE);
        SendMessage(w.bar, PBM_SETRANGE32, 0, total);
        SendMessage(w.bar, PBM_SETPOS, done, 0);
    } else if (finished) {
        SetWindowLongPtr(w.bar, GWL_STYLE, GetWindowLongPtr(w.bar, GWL_STYLE) & ~PBS_MARQUEE);
        SendMessage(w.bar, PBM_SETMARQUEE, FALSE, 0);
        SendMessage(w.bar, PBM_SETRANGE32, 0, 1);
        SendMessage(w.bar, PBM_SETPOS, 1, 0);
    }
    for (const string &line : fresh) {
        int index =
            static_cast<int>(SendMessageW(w.log, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide(line).c_str())));
        SendMessage(w.log, LB_SETTOPINDEX, index, 0);
    }
    if (finished) {
        SetWindowTextW(w.button, L"Close");
        KillTimer(w.hwnd, IdTimer);
    }
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Window *w = reinterpret_cast<Window *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        w = reinterpret_cast<Window *>(reinterpret_cast<CREATESTRUCT *>(lParam)->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(w));
        w->hwnd = hwnd;
        HINSTANCE inst = GetModuleHandle(nullptr);
        w->stageLabel = CreateWindowExW(0, L"STATIC", L"Starting...", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
                                        0, 0, 0, 0, hwnd, nullptr, inst, nullptr);
        w->bar = CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | PBS_MARQUEE, 0, 0, 0, 0, hwnd,
                                 nullptr, inst, nullptr);
        SendMessage(w->bar, PBM_SETMARQUEE, TRUE, 0);
        w->log = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOSEL, 0, 0, 0, 0,
                                 hwnd, nullptr, inst, nullptr);
        w->button = CreateWindowExW(0, L"BUTTON", L"Stop", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0,
                                    0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdButton)), inst, nullptr);
        // the system's dialog font on every control - the default is the bitmap "System" font
        NONCLIENTMETRICSW metrics = {};
        metrics.cbSize = sizeof(metrics);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
        w->font = CreateFontIndirectW(&metrics.lfMessageFont);
        for (HWND control : {w->stageLabel, w->log, w->button})
            SendMessage(control, WM_SETFONT, reinterpret_cast<WPARAM>(w->font), TRUE);
        layout(*w);
        startJob(*w);
        SetTimer(hwnd, IdTimer, 100, nullptr);
        return 0;
    }
    case WM_SIZE:
        if (w)
            layout(*w);
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *info = reinterpret_cast<MINMAXINFO *>(lParam);
        info->ptMinTrackSize.x = 480;
        info->ptMinTrackSize.y = 300;
        return 0;
    }
    case WM_TIMER:
        if (w)
            refresh(*w);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == IdButton && w) {
            if (w->state.finished.load()) {
                DestroyWindow(hwnd);
            } else if (!w->state.stop.load()) {
                // the job returns at its next checkpoint (a download in flight finishes or times out first)
                w->state.stop.store(true);
                lock_guard<mutex> lock(w->state.m);
                w->state.stage = "Stopping...";
            }
        }
        return 0;
    case WM_CLOSE:
        if (w && !w->state.finished.load()) {
            w->state.stop.store(true);
            SetWindowTextW(w->stageLabel, L"Stopping...");
            return 0; // the timer's refresh sees it finish, then Close closes
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

} // namespace

//*******************************
// attachParentConsole
//*******************************
void attachParentConsole() {
    // a redirected stdout (a pipe or a file from a script) is already ours through the inherited handle;
    // only a real console needs attaching to, and its screen buffer opening
    auto redirected = [](DWORD which) {
        HANDLE h = GetStdHandle(which);
        return h != nullptr && h != INVALID_HANDLE_VALUE && GetFileType(h) != FILE_TYPE_UNKNOWN;
    };
    const bool outRedirected = redirected(STD_OUTPUT_HANDLE), errRedirected = redirected(STD_ERROR_HANDLE);
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;
    FILE *f = nullptr;
    if (!outRedirected)
        freopen_s(&f, "CONOUT$", "w", stdout);
    if (!errRedirected)
        freopen_s(&f, "CONOUT$", "w", stderr);
}

//*******************************
// runUpdateRomsWindow
//*******************************
int runUpdateRomsWindow(const UpdateRomsJob::Setup &setup, const string &error) {
    if (!error.empty()) {
        MessageBoxW(nullptr, wide(error).c_str(), L"AutoBleem - Update ROMs", MB_OK | MB_ICONERROR);
        return 1;
    }
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    Window w;
    w.setup = setup;

    WNDCLASSA wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = WindowClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassA(&wc);

    const string title = "AutoBleem - Update ROMs (" + setup.root + ", " +
                         (setup.target == "rpi" ? "Raspberry Pi" : "PlayStation Classic") + ")";
    HWND hwnd = CreateWindowExW(0, L"AutoBleemUpdateRoms", wide(title).c_str(), WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, 640, 420, nullptr, nullptr, wc.hInstance, &w);
    if (!hwnd) {
        PLOG_ERROR << "CreateWindow failed: " << GetLastError();
        return 1;
    }
    ShowWindow(hwnd, SW_SHOW);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    w.state.stop.store(true);
    if (w.worker.joinable())
        w.worker.join();
    if (w.font)
        DeleteObject(w.font);
    return 0;
}

#endif
