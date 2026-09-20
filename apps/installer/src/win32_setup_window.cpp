//
// The setup window - see the header. The console installer's window (win32_window.cpp) with the stick
// questions replaced by the data folder: a box with the path and a Browse... button. The job runs on a
// std::thread into a mutex-guarded State; a 100 ms timer moves it into the controls.
//
#ifdef _WIN32

#include "win32_setup_window.h"
#include "win32_platform.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/log.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlobj.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace std;

namespace {

const char *const WindowClass = "AutoBleemWinSetup";
const int IdTimer = 1;
enum Ids {
    IdFolder = 100,
    IdBrowse,
    IdCoversJ,
    IdCoversU,
    IdCoversP,
    IdRetroArch,
    IdBios,
    IdSamples,
    IdInstall,
    IdAction, // Stop / Close on the progress page
};
const int Width = 640;
const int HeroHeight = 270; // the picture: rows 90..630 of the 1280x720 splash, scaled to the width
const int Margin = 14;

//******************
// State
//******************
struct State {
    mutex m;
    string phase;
    int phaseIndex = 0, phaseTotal = 0;
    uint64_t done = 0, total = 0;
    vector<string> lines;
    size_t shown = 0;
    atomic<bool> stop{false};
    atomic<bool> finished{false};
    bool ok = false;
    string error;
};

//******************
// Listener
//******************
class Listener : public InstallListener {
public:
    explicit Listener(State &state) : state_(state) {}
    void onPhase(int index, int total, const string &title) override {
        lock_guard<mutex> lock(state_.m);
        state_.phase = title;
        state_.phaseIndex = index;
        state_.phaseTotal = total;
        state_.done = state_.total = 0;
    }
    void onProgress(uint64_t done, uint64_t total) override {
        lock_guard<mutex> lock(state_.m);
        state_.done = done;
        state_.total = total;
    }
    void onLine(const string &line) override {
        lock_guard<mutex> lock(state_.m);
        state_.lines.push_back(line);
    }

private:
    State &state_;
};

//******************
// Window
//******************
struct Window {
    HWND hwnd = nullptr;
    HWND folderLabel = nullptr, folder = nullptr, browse = nullptr, status = nullptr;
    HWND coversLabel = nullptr, coversJ = nullptr, coversU = nullptr, coversP = nullptr;
    HWND retroarch = nullptr, bios = nullptr, samples = nullptr, install = nullptr;
    HWND phaseLabel = nullptr, phaseBar = nullptr, bar = nullptr, log = nullptr, action = nullptr;
    HFONT font = nullptr, bold = nullptr;
    Gdiplus::Image *hero = nullptr;
    ULONG_PTR gdiplusToken = 0;
    WindowsInstallOptions defaults;
    WindowsInstallInfo info;
    State state;
    thread worker;
    bool progressPage = false;
    bool autoStart = false;
};

wstring wide(const string &utf8) {
    if (utf8.empty())
        return wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &out[0], n);
    return out;
}

string narrow(const wstring &w) {
    if (w.empty())
        return string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &out[0], n, nullptr, nullptr);
    return out;
}

HWND make(Window &w, const wchar_t *cls, const wchar_t *text, DWORD style, int id, DWORD ex = 0) {
    HWND h = CreateWindowExW(ex, cls, text, WS_CHILD | style, 0, 0, 0, 0, w.hwnd,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandle(nullptr), nullptr);
    SendMessage(h, WM_SETFONT, reinterpret_cast<WPARAM>(w.font), TRUE);
    return h;
}

bool checked(HWND h) {
    return SendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED;
}
void setChecked(HWND h, bool on) {
    SendMessage(h, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
}

// the hero picture from the exe's resources (autobleem.jpg as RCDATA 1)
Gdiplus::Image *loadHero() {
    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(1), reinterpret_cast<LPCWSTR>(RT_RCDATA));
    if (!res)
        return nullptr;
    HGLOBAL data = LoadResource(nullptr, res);
    DWORD size = SizeofResource(nullptr, res);
    void *bytes = LockResource(data);
    if (!bytes || !size)
        return nullptr;
    HGLOBAL copy = GlobalAlloc(GMEM_MOVEABLE, size);
    memcpy(GlobalLock(copy), bytes, size);
    GlobalUnlock(copy);
    IStream *stream = nullptr;
    if (CreateStreamOnHGlobal(copy, TRUE, &stream) != S_OK)
        return nullptr;
    Gdiplus::Image *image = new Gdiplus::Image(stream);
    stream->Release();
    if (image->GetLastStatus() != Gdiplus::Ok) {
        delete image;
        return nullptr;
    }
    return image;
}

void showPage(Window &w, bool progress) {
    w.progressPage = progress;
    for (HWND h : {w.folderLabel, w.folder, w.browse, w.status, w.coversLabel, w.coversJ, w.coversU, w.coversP,
                   w.retroarch, w.bios, w.samples, w.install})
        ShowWindow(h, progress ? SW_HIDE : SW_SHOW);
    for (HWND h : {w.phaseLabel, w.phaseBar, w.bar, w.log, w.action})
        ShowWindow(h, progress ? SW_SHOW : SW_HIDE);
}

string folderText(Window &w) {
    wchar_t buf[MAX_PATH * 4];
    GetWindowTextW(w.folder, buf, sizeof(buf) / sizeof(buf[0]));
    return InstallerJob::normalizeRoot(narrow(buf));
}

// the status line under the folder: what is there already
void describeFolder(Window &w) {
    WindowsInstallOptions o = w.defaults;
    o.dataRoot = folderText(w);
    w.info = WindowsInstallJob::inspect(o);
    string text;
    if (!w.info.exists) {
        text = w.info.error;
    } else if (w.info.installed) {
        text = "AutoBleem has run from this folder before - the games and settings are kept.";
    } else {
        text = "A new data folder: the games, settings and themes go here.";
    }
    if (w.info.hasRetroArch)
        text += " RetroArch " + (w.info.retroarchVersion.empty() ? string("is") : w.info.retroarchVersion + " is") +
                " installed.";
    SetWindowTextW(w.status, wide(text).c_str());
    EnableWindow(w.bios, checked(w.retroarch) || w.info.hasRetroArch);
    if (!checked(w.retroarch) && !w.info.hasRetroArch)
        setChecked(w.bios, false);
    EnableWindow(w.install, w.info.exists);
}

void browseFolder(Window &w) {
    wchar_t path[MAX_PATH] = {0};
    BROWSEINFOW bi = {};
    bi.hwndOwner = w.hwnd;
    bi.lpszTitle = L"Where the games, settings and themes go:";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
    LPITEMIDLIST item = SHBrowseForFolderW(&bi);
    if (item && SHGetPathFromIDListW(item, path)) {
        SetWindowTextW(w.folder, path);
        describeFolder(w);
    }
    if (item)
        CoTaskMemFree(item);
}

void layout(Window &w) {
    const int x = Margin, inner = Width - 2 * Margin;
    int y = HeroHeight + Margin;
    MoveWindow(w.folderLabel, x, y + 4, 80, 20, TRUE);
    MoveWindow(w.folder, x + 82, y, inner - 82 - 90 - 8, 24, TRUE);
    MoveWindow(w.browse, Width - Margin - 90, y, 90, 24, TRUE);
    y += 32;
    MoveWindow(w.status, x, y, inner, 34, TRUE);
    y += 40;
    MoveWindow(w.coversLabel, x, y + 2, 110, 20, TRUE);
    MoveWindow(w.coversJ, x + 112, y, 70, 22, TRUE);
    MoveWindow(w.coversU, x + 190, y, 70, 22, TRUE);
    MoveWindow(w.coversP, x + 268, y, 70, 22, TRUE);
    y += 28;
    MoveWindow(w.retroarch, x, y, inner, 22, TRUE);
    y += 26;
    MoveWindow(w.bios, x + 20, y, inner - 20, 22, TRUE);
    y += 26;
    MoveWindow(w.samples, x, y, inner, 22, TRUE);
    y += 34;
    MoveWindow(w.install, Width - Margin - 110, y, 110, 30, TRUE);
    const int questionsBottom = y + 30 + Margin;
    y = HeroHeight + Margin;
    MoveWindow(w.phaseLabel, x, y, inner, 20, TRUE);
    y += 24;
    MoveWindow(w.phaseBar, x, y, inner, 14, TRUE);
    y += 20;
    MoveWindow(w.bar, x, y, inner, 14, TRUE);
    y += 22;
    const int buttonH = 26, buttonW = 90;
    MoveWindow(w.log, x, y, inner, questionsBottom - y - buttonH - Margin - 4, TRUE);
    MoveWindow(w.action, Width - Margin - buttonW, questionsBottom - buttonH - Margin, buttonW, buttonH, TRUE);
    RECT r = {0, 0, Width, questionsBottom};
    AdjustWindowRect(&r, static_cast<DWORD>(GetWindowLongPtr(w.hwnd, GWL_STYLE)), FALSE);
    SetWindowPos(w.hwnd, nullptr, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
}

void startInstall(Window &w) {
    WindowsInstallOptions o = w.defaults;
    o.dataRoot = folderText(w);
    o.coversJapan = checked(w.coversJ);
    o.coversUsa = checked(w.coversU);
    o.coversPal = checked(w.coversP);
    o.retroarch = checked(w.retroarch);
    o.bios = checked(w.bios);
    o.samples = checked(w.samples);
    {
        lock_guard<mutex> lock(w.state.m);
        w.state.phase.clear();
        w.state.phaseIndex = w.state.phaseTotal = 0;
        w.state.done = w.state.total = 0;
        w.state.lines.clear();
        w.state.shown = 0;
        w.state.ok = false;
        w.state.error.clear();
    }
    w.state.stop.store(false);
    w.state.finished.store(false);
    showPage(w, true);
    SendMessage(w.log, LB_RESETCONTENT, 0, 0);
    SetWindowTextW(w.action, L"Stop");
    w.worker = thread([&w, o]() {
        Listener listener(w.state);
        WinInetDownloader downloader;
        string error;
        bool ok = WindowsInstallJob::run(o, downloader, listener, [&w]() { return w.state.stop.load(); }, error);
        {
            lock_guard<mutex> lock(w.state.m);
            w.state.ok = ok;
            w.state.error = error;
            if (!ok) {
                w.state.lines.push_back(error == "Stopped" ? "Stopped." : "Failed: " + error);
                w.state.phase = error == "Stopped" ? "Stopped" : "Failed";
            } else {
                w.state.phase = "Done";
            }
            w.state.done = w.state.total = 0;
        }
        w.state.finished.store(true);
    });
}

// the timer: whatever changed since the last tick goes into the controls
void refresh(Window &w) {
    string phase;
    int phaseIndex, phaseTotal;
    uint64_t done, total;
    vector<string> fresh;
    bool finished = w.state.finished.load();
    {
        lock_guard<mutex> lock(w.state.m);
        phase = w.state.phase;
        phaseIndex = w.state.phaseIndex;
        phaseTotal = w.state.phaseTotal;
        done = w.state.done;
        total = w.state.total;
        for (size_t i = w.state.shown; i < w.state.lines.size(); i++)
            fresh.push_back(w.state.lines[i]);
        w.state.shown = w.state.lines.size();
    }
    string title = phase.empty() ? "Starting..." : phase;
    if (phaseTotal > 0 && !finished)
        title = "Step " + to_string(phaseIndex) + " of " + to_string(phaseTotal) + ": " + phase;
    SetWindowTextW(w.phaseLabel, wide(title).c_str());
    SendMessage(w.phaseBar, PBM_SETRANGE32, 0, phaseTotal > 0 ? phaseTotal : 1);
    SendMessage(w.phaseBar, PBM_SETPOS,
                finished ? (phaseTotal > 0 ? phaseTotal : 1) : (phaseIndex > 0 ? phaseIndex - 1 : 0), 0);
    if (total > 0) {
        SetWindowLongPtr(w.bar, GWL_STYLE, GetWindowLongPtr(w.bar, GWL_STYLE) & ~PBS_MARQUEE);
        SendMessage(w.bar, PBM_SETMARQUEE, FALSE, 0);
        SendMessage(w.bar, PBM_SETRANGE32, 0, 1000);
        SendMessage(w.bar, PBM_SETPOS, static_cast<int>(done * 1000 / total), 0);
    } else if (finished) {
        SetWindowLongPtr(w.bar, GWL_STYLE, GetWindowLongPtr(w.bar, GWL_STYLE) & ~PBS_MARQUEE);
        SendMessage(w.bar, PBM_SETMARQUEE, FALSE, 0);
        SendMessage(w.bar, PBM_SETRANGE32, 0, 1);
        SendMessage(w.bar, PBM_SETPOS, 1, 0);
    } else if (!(GetWindowLongPtr(w.bar, GWL_STYLE) & PBS_MARQUEE)) {
        SetWindowLongPtr(w.bar, GWL_STYLE, GetWindowLongPtr(w.bar, GWL_STYLE) | PBS_MARQUEE);
        SendMessage(w.bar, PBM_SETMARQUEE, TRUE, 0);
    }
    for (const string &line : fresh) {
        int index =
            static_cast<int>(SendMessageW(w.log, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide(line).c_str())));
        SendMessage(w.log, LB_SETTOPINDEX, index, 0);
    }
    if (finished) {
        if (w.worker.joinable())
            w.worker.join();
        SetWindowTextW(w.action, w.state.ok ? L"Close" : L"Back");
        EnableWindow(w.action, TRUE);
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
        NONCLIENTMETRICSW metrics = {};
        metrics.cbSize = sizeof(metrics);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
        w->font = CreateFontIndirectW(&metrics.lfMessageFont);
        LOGFONTW boldFont = metrics.lfMessageFont;
        boldFont.lfWeight = FW_SEMIBOLD;
        w->bold = CreateFontIndirectW(&boldFont);

        w->folderLabel = make(*w, L"STATIC", L"Data folder:", SS_LEFT, 0);
        w->folder = make(*w, L"EDIT", wide(w->defaults.dataRoot).c_str(), WS_TABSTOP | ES_AUTOHSCROLL, IdFolder,
                         WS_EX_CLIENTEDGE);
        w->browse = make(*w, L"BUTTON", L"Browse...", WS_TABSTOP | BS_PUSHBUTTON, IdBrowse);
        w->status = make(*w, L"STATIC", L"", SS_LEFT, 0);
        SendMessage(w->status, WM_SETFONT, reinterpret_cast<WPARAM>(w->bold), TRUE);
        w->coversLabel = make(*w, L"STATIC", L"Cover databases:", SS_LEFT, 0);
        w->coversJ = make(*w, L"BUTTON", L"Japan", WS_TABSTOP | BS_AUTOCHECKBOX, IdCoversJ);
        w->coversU = make(*w, L"BUTTON", L"USA", WS_TABSTOP | BS_AUTOCHECKBOX, IdCoversU);
        w->coversP = make(*w, L"BUTTON", L"PAL", WS_TABSTOP | BS_AUTOCHECKBOX, IdCoversP);
        w->retroarch =
            make(*w, L"BUTTON", L"Install RetroArch (emulators for the other systems, with every core - about 1.5 GB)",
                 WS_TABSTOP | BS_AUTOCHECKBOX, IdRetroArch);
        w->bios = make(*w, L"BUTTON", L"Download the BIOS files the cores need (about 300 MB, from RetroBIOS)",
                       WS_TABSTOP | BS_AUTOCHECKBOX, IdBios);
        w->samples = make(*w, L"BUTTON", L"Add the sample games (free homebrew, so the shelf is not empty)",
                          WS_TABSTOP | BS_AUTOCHECKBOX, IdSamples);
        w->install = make(*w, L"BUTTON", L"Install", WS_TABSTOP | BS_DEFPUSHBUTTON, IdInstall);
        setChecked(w->coversJ, w->defaults.coversJapan);
        setChecked(w->coversU, w->defaults.coversUsa);
        setChecked(w->coversP, w->defaults.coversPal);
        setChecked(w->retroarch, w->defaults.retroarch);
        setChecked(w->bios, w->defaults.bios);
        setChecked(w->samples, w->defaults.samples);

        w->phaseLabel = make(*w, L"STATIC", L"", SS_LEFT | SS_ENDELLIPSIS, 0);
        SendMessage(w->phaseLabel, WM_SETFONT, reinterpret_cast<WPARAM>(w->bold), TRUE);
        w->phaseBar = make(*w, PROGRESS_CLASSW, nullptr, 0, 0);
        w->bar = make(*w, PROGRESS_CLASSW, nullptr, PBS_MARQUEE, 0);
        w->log = make(*w, L"LISTBOX", nullptr, WS_VSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOSEL, 0, WS_EX_CLIENTEDGE);
        w->action = make(*w, L"BUTTON", L"Stop", WS_TABSTOP | BS_PUSHBUTTON, IdAction);

        layout(*w);
        showPage(*w, false);
        describeFolder(*w);
        if (w->autoStart && w->info.exists) {
            startInstall(*w);
            SetTimer(hwnd, IdTimer, 100, nullptr);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT hero = {0, 0, Width, HeroHeight};
        if (w && w->hero) {
            Gdiplus::Graphics g(dc);
            g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            Gdiplus::Rect dest(0, 0, Width, HeroHeight);
            g.DrawImage(w->hero, dest, 0, 90, 1280, 540, Gdiplus::UnitPixel);
        } else {
            HBRUSH navy = CreateSolidBrush(RGB(6, 26, 58));
            FillRect(dc, &hero, navy);
            DeleteObject(navy);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(232, 242, 255));
            if (w)
                SelectObject(dc, w->bold);
            DrawTextW(dc, L"AutoBleem 2", -1, &hero, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
    case WM_TIMER:
        if (w)
            refresh(*w);
        return 0;
    case WM_COMMAND:
        if (!w)
            return 0;
        switch (LOWORD(wParam)) {
        case IdFolder:
            if (HIWORD(wParam) == EN_KILLFOCUS)
                describeFolder(*w);
            break;
        case IdBrowse:
            browseFolder(*w);
            break;
        case IdRetroArch:
            describeFolder(*w);
            break;
        case IdInstall:
            if (!w->progressPage) {
                describeFolder(*w);
                if (w->info.exists) {
                    startInstall(*w);
                    SetTimer(hwnd, IdTimer, 100, nullptr);
                }
            }
            break;
        case IdAction:
            if (w->state.finished.load()) {
                if (w->state.ok)
                    DestroyWindow(hwnd);
                else
                    showPage(*w, false);
            } else if (!w->state.stop.load()) {
                w->state.stop.store(true);
                lock_guard<mutex> lock(w->state.m);
                w->state.phase = "Stopping...";
            }
            break;
        default:
            break;
        }
        return 0;
    case WM_CLOSE:
        if (w && w->progressPage && !w->state.finished.load()) {
            w->state.stop.store(true);
            return 0;
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
// runSetupWindow
//*******************************
int runSetupWindow(const WindowsInstallOptions &defaults, bool autoStart) {
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    CoInitialize(nullptr);
    Window w;
    w.defaults = defaults;
    w.autoStart = autoStart;
    Gdiplus::GdiplusStartupInput gdiplusInput;
    Gdiplus::GdiplusStartup(&w.gdiplusToken, &gdiplusInput, nullptr);
    w.hero = loadHero();

    WNDCLASSA wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = WindowClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassA(&wc);

    HWND hwnd =
        CreateWindowExW(0, L"AutoBleemWinSetup", autoStart ? L"AutoBleem 2 - setting up" : L"AutoBleem 2 - setup",
                        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, Width,
                        600, nullptr, nullptr, wc.hInstance, &w);
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
    delete w.hero;
    Gdiplus::GdiplusShutdown(w.gdiplusToken);
    if (w.font)
        DeleteObject(w.font);
    if (w.bold)
        DeleteObject(w.bold);
    CoUninitialize();
    return 0;
}

#endif
