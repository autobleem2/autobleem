#include "windows_host.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/log.h>
#include <ableem/engine/strings.h>

#include <fstream>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#endif

using namespace std;

const char *const WindowsHost::RegistryKey = "Software\\AutoBleem";
const char *const WindowsHost::RegistryValue = "DataRoot";
const char *const WindowsHost::PointerFile = "dataroot.txt";

namespace {
#ifdef _WIN32
string utf8(const wstring &w) {
    if (w.empty())
        return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1)
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

wstring wide(const string &s) {
    if (s.empty())
        return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1)
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}
#endif

// the paths the app builds compare as strings with '/' - a path from Windows comes with '\'
string slashes(string p) {
    for (char &c : p)
        if (c == '\\')
            c = '/';
    while (p.size() > 1 && p.back() == '/')
        p.pop_back();
    return p;
}
} // namespace

//*******************************
// WindowsHost::facts
//*******************************
HostFacts WindowsHost::facts() {
    HostFacts f;
    f.programDir = programDir();
    f.registryDataRoot = registryDataRoot();
    f.pointerFileDataRoot = pointerFileDataRoot(f.programDir);
    f.documentsDir = documentsDir();
    return f;
}

//*******************************
// WindowsHost::programDir
//*******************************
string WindowsHost::programDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buf, sizeof(buf) / sizeof(buf[0]));
    if (n == 0 || n >= sizeof(buf) / sizeof(buf[0]))
        return "";
    string exe = slashes(utf8(wstring(buf, n)));
    size_t slash = exe.rfind('/');
    return slash == string::npos ? "" : exe.substr(0, slash);
#else
    return "";
#endif
}

//*******************************
// WindowsHost::registryDataRoot
//*******************************
string WindowsHost::registryDataRoot() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH * 4];
    DWORD size = sizeof(buf);
    LSTATUS st = RegGetValueW(HKEY_CURRENT_USER, wide(RegistryKey).c_str(), wide(RegistryValue).c_str(), RRF_RT_REG_SZ,
                              nullptr, buf, &size);
    if (st != ERROR_SUCCESS)
        return "";
    return slashes(utf8(buf));
#else
    return "";
#endif
}

//*******************************
// WindowsHost::pointerFileDataRoot
//*******************************
string WindowsHost::pointerFileDataRoot(const string &programDir) {
    if (programDir.empty())
        return "";
    ifstream in(programDir + ableem::sep + PointerFile);
    if (!in)
        return "";
    string line;
    getline(in, line);
    if (!line.empty() && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
        line.erase(0, 3); // a BOM from Notepad
    ableem::trim(line);
    return line.empty() ? "" : slashes(line);
}

//*******************************
// WindowsHost::documentsDir
//*******************************
string WindowsHost::documentsDir() {
#ifdef _WIN32
    PWSTR path = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_CREATE, nullptr, &path) != S_OK) {
        if (path)
            CoTaskMemFree(path);
        return "";
    }
    string s = slashes(utf8(path));
    CoTaskMemFree(path);
    return s;
#else
    return "";
#endif
}
