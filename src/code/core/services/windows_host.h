//
// WindowsHost: the facts EnvironmentSetup::fromWindowsInstall decides the Windows product's layout from -
// where the exe is, what the installer wrote to the registry, the pointer file next to the exe, and the
// user's Documents folder. The only Windows API in ab_core besides SystemInfoService's; built on Windows
// only (the dev build too, so the tests can exercise the readers).
//
#pragma once

#include "environment_setup.h"

#include <string>

class WindowsHost {
public:
    static HostFacts facts();

    static std::string programDir();       // the folder the running exe is in
    static std::string registryDataRoot(); // HKCU\Software\AutoBleem\DataRoot, "" when unset
    static std::string pointerFileDataRoot(const std::string &programDir); // first line of dataroot.txt
    static std::string documentsDir(); // the user's Documents folder, "" when unknown

    static const char *const RegistryKey;   // "Software\AutoBleem"
    static const char *const RegistryValue; // "DataRoot"
    static const char *const PointerFile;   // "dataroot.txt"
};
