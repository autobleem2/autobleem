#ifdef _WIN32

#include "win32_platform.h"

#include <ableem/engine/log.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wininet.h>

#include <cstdio>
#include <fstream>

using namespace std;

namespace {

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

string gb(uint64_t bytes) {
    char buf[32];
    if (bytes >= 1024ull * 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0 * 1024 * 1024));
    else
        snprintf(buf, sizeof(buf), "%.0f MB", bytes / (1024.0 * 1024));
    return buf;
}

string lastErrorText(DWORD code) {
    wchar_t *msg = nullptr;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE |
                                 FORMAT_MESSAGE_IGNORE_INSERTS,
                             GetModuleHandleW(L"wininet.dll"), code, 0, reinterpret_cast<LPWSTR>(&msg), 0, nullptr);
    string text = n ? narrow(wstring(msg, n)) : "error " + to_string(code);
    if (msg)
        LocalFree(msg);
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n' || text.back() == ' '))
        text.pop_back();
    return text;
}

// runs a command line, every line of its output to `say`; the exit code, -1 when it could not start
int runCapturing(const wstring &commandLine, const function<void(const string &)> &say) {
    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE readEnd = nullptr, writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, &sa, 0))
        return -1;
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writeEnd;
    si.hStdError = writeEnd;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi = {};
    wstring cmd = commandLine; // CreateProcess may write into it
    if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(readEnd);
        CloseHandle(writeEnd);
        return -1;
    }
    CloseHandle(writeEnd);
    string pending;
    char buf[512];
    DWORD got = 0;
    while (ReadFile(readEnd, buf, sizeof(buf), &got, nullptr) && got > 0) {
        pending.append(buf, got);
        size_t nl;
        while ((nl = pending.find_first_of("\r\n")) != string::npos) {
            string line = pending.substr(0, nl);
            pending.erase(0, nl + 1);
            if (!line.empty())
                say("  " + line);
        }
    }
    if (!pending.empty())
        say("  " + pending);
    CloseHandle(readEnd);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(code);
}

} // namespace

//*******************************
// RemovableDrive::describe
//*******************************
string RemovableDrive::describe() const {
    if (!ready)
        return letter + "  (no readable volume - format it first)";
    string text = letter + "  " + (label.empty() ? "(no label)" : label) + "  (" +
                  (fileSystem.empty() ? "?" : fileSystem) + ", " + gb(sizeBytes) + ", " + gb(freeBytes) + " free)";
    return text;
}

//*******************************
// listRemovableDrives
//*******************************
vector<RemovableDrive> listRemovableDrives() {
    vector<RemovableDrive> drives;
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (!(mask & (1u << i)))
            continue;
        wchar_t root[] = {static_cast<wchar_t>(L'A' + i), L':', L'\\', 0};
        if (GetDriveTypeW(root) != DRIVE_REMOVABLE)
            continue;
        RemovableDrive d;
        d.letter = string(1, static_cast<char>('A' + i)) + ":";
        d.root = d.letter + "/";
        wchar_t label[MAX_PATH + 1] = {0}, fs[MAX_PATH + 1] = {0};
        DWORD serial = 0, maxLen = 0, flags = 0;
        UINT oldMode = SetErrorMode(SEM_FAILCRITICALERRORS); // no "insert a disk" box for an empty reader
        if (GetVolumeInformationW(root, label, MAX_PATH, &serial, &maxLen, &flags, fs, MAX_PATH)) {
            d.label = narrow(label);
            d.fileSystem = narrow(fs);
            ULARGE_INTEGER freeToCaller, total, freeTotal;
            if (GetDiskFreeSpaceExW(root, &freeToCaller, &total, &freeTotal)) {
                d.sizeBytes = total.QuadPart;
                d.freeBytes = freeToCaller.QuadPart;
            }
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_NOT_READY)
                continue;    // a card reader with nothing in it
            d.ready = false; // there, but unformatted (or RAW)
        }
        SetErrorMode(oldMode);
        drives.push_back(d);
    }
    return drives;
}

//*******************************
// ensureVolumeLabel
//*******************************
bool ensureVolumeLabel(const string &root, const string &label, string &error) {
    if (root.size() < 2 || root[1] != ':') {
        error = "not a drive: " + root;
        return false;
    }
    wchar_t r[] = {static_cast<wchar_t>(root[0]), L':', L'\\', 0};
    wchar_t current[MAX_PATH + 1] = {0};
    if (GetVolumeInformationW(r, current, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0) &&
        _wcsicmp(current, wide(label).c_str()) == 0)
        return true;
    if (!SetVolumeLabelW(r, wide(label).c_str())) {
        error = "cannot name " + root.substr(0, 2) + " " + label + ": " + lastErrorText(GetLastError());
        return false;
    }
    return true;
}

//*******************************
// programDirectory
//*******************************
string programDirectory() {
    wchar_t path[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH * 4);
    string dir = narrow(wstring(path, n));
    for (char &c : dir)
        if (c == '\\')
            c = '/';
    size_t slash = dir.find_last_of('/');
    return slash == string::npos ? "." : dir.substr(0, slash);
}

//*******************************
// formatDrive
//*******************************
bool formatDrive(const string &letter, const string &fileSystem, const string &label,
                 const function<void(const string &)> &say, string &error) {
    if (letter.size() != 2 || letter[1] != ':') {
        error = "not a drive letter: " + letter;
        return false;
    }
    wchar_t root[] = {static_cast<wchar_t>(letter[0]), L':', L'\\', 0};
    if (GetDriveTypeW(root) != DRIVE_REMOVABLE) {
        error = letter + " is not a removable drive - only those are formatted from here";
        return false;
    }
    const bool fat32 = fileSystem == "FAT32";
    ULARGE_INTEGER total = {};
    ULARGE_INTEGER dummy = {};
    GetDiskFreeSpaceExW(root, &dummy, &total, &dummy);
    const bool bigForFat32 = fat32 && total.QuadPart > 32ull * 1024 * 1024 * 1024;

    wstring command;
    const string helper = programDirectory() + "/fat32format.exe";
    if (bigForFat32 && GetFileAttributesW(wide(helper).c_str()) != INVALID_FILE_ATTRIBUTES) {
        // Ridgecrop's fat32format: `fat32format -c<sectors per cluster> X:` - it answers a prompt itself
        // with -y in the newer builds; the older takes the drive only and asks, so the answer is piped
        say("Formatting " + letter + " as FAT32 with fat32format.exe (" + gb(total.QuadPart) +
            " is more than format.com will do)");
        command = L"cmd.exe /c echo y| \"" + wide(helper) + L"\" " + wide(letter);
    } else {
        if (bigForFat32)
            say("Note: " + letter + " is " + gb(total.QuadPart) +
                " - Windows formats FAT32 up to 32 GB only; put "
                "fat32format.exe next to the installer for a bigger stick, or use exFAT");
        say("Formatting " + letter + " as " + fileSystem + " (quick), label " + label);
        command =
            L"cmd.exe /c format " + wide(letter) + L" /FS:" + wide(fileSystem) + L" /Q /V:" + wide(label) + L" /Y";
    }
    int code = runCapturing(command, say);
    if (code != 0) {
        error =
            code < 0 ? "could not start the format tool" : "the format tool failed (exit code " + to_string(code) + ")";
        return false;
    }
    // the label: format.com sets it; fat32format does not
    SetVolumeLabelW(root, wide(label).c_str());
    return true;
}

//*******************************
// WinInetDownloader::fetch
//*******************************
bool WinInetDownloader::fetch(const string &url, const string &destFile, const Progress &progress, string &error) {
    HINTERNET session = InternetOpenW(L"AutoBleem-Installer/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!session) {
        error = "WinINet: " + lastErrorText(GetLastError());
        return false;
    }
    DWORD timeout = 30000;
    InternetSetOptionW(session, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionW(session, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    HINTERNET file = InternetOpenUrlW(session, wide(url).c_str(), nullptr, 0,
                                      INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI, 0);
    if (!file) {
        error = url + ": " + lastErrorText(GetLastError());
        InternetCloseHandle(session);
        return false;
    }
    bool ok = true;
    DWORD status = 0, size = sizeof(status), index = 0;
    if (HttpQueryInfoW(file, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &size, &index) &&
        status != 200) {
        error = url + ": HTTP " + to_string(status);
        ok = false;
    }
    uint64_t total = 0;
    wchar_t lengthText[64] = {0};
    size = sizeof(lengthText);
    index = 0;
    if (ok && HttpQueryInfoW(file, HTTP_QUERY_CONTENT_LENGTH, lengthText, &size, &index))
        total = wcstoull(lengthText, nullptr, 10);
    if (ok) {
        ofstream out(destFile, ios::binary | ios::trunc);
        if (!out) {
            error = "cannot write " + destFile;
            ok = false;
        }
        uint64_t done = 0;
        char buf[65536];
        DWORD got = 0;
        while (ok && InternetReadFile(file, buf, sizeof(buf), &got) && got > 0) {
            out.write(buf, got);
            done += got;
            if (progress && !progress(done, total)) {
                error = "Stopped";
                ok = false;
            }
        }
        if (ok && !out) {
            error = "cannot write " + destFile;
            ok = false;
        }
        if (ok && total > 0 && done != total) {
            error = url + ": the download stopped short (" + to_string(done) + " of " + to_string(total) + " bytes)";
            ok = false;
        }
        out.close();
        if (!ok)
            DeleteFileW(wide(destFile).c_str());
    }
    InternetCloseHandle(file);
    InternetCloseHandle(session);
    return ok;
}

//*******************************
// attachParentConsole
//*******************************
void attachParentConsole() {
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

#endif
