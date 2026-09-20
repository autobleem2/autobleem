# The installer: AutoBleem onto a PlayStation Classic stick from a PC - the stick's file system from the
# package next to the program, the rest from the download repository. installer_core is the job (ab_core
# only, tested from tests/apps), installer the program: a plain Win32 window on Windows (no SDL, no
# AutoBleem theme - one self-contained exe, like UpdateRoms), the --quiet console path elsewhere. Built
# for the dev hosts only: it is what runs on the PC the stick is plugged into.

add_library(installer_core STATIC
        src/core/install_job_base.cpp
        src/core/install_job_base.h
        src/core/installer_job.cpp
        src/core/installer_job.h
        src/core/legacy_layout.cpp
        src/core/legacy_layout.h
        src/core/windows_install_job.cpp
        src/core/windows_install_job.h
)
target_include_directories(installer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/code)
target_link_libraries(installer_core ab_core)

add_executable(installer
        src/main.cpp
        src/win32_platform.cpp
        src/win32_platform.h
        src/win32_window.cpp
        src/win32_window.h
)
set_target_properties(installer PROPERTIES OUTPUT_NAME AutoBleemInstaller)
target_link_libraries(installer installer_core Threads::Threads ${CMAKE_DL_LIBS})

if (WIN32)
    # a GUI-subsystem exe (no console window when double-clicked; --quiet attaches to the parent's), the
    # manifest and the picture as resources, and everything static: nothing of MSYS2 is needed next to it
    target_sources(installer PRIVATE src/installer.rc)
    target_link_libraries(installer comctl32 user32 gdi32 gdiplus ole32 wininet)
    target_link_options(installer PRIVATE -mwindows -static -static-libgcc -static-libstdc++)
endif()

# README.txt next to the binary: the folder for a release is the exe, that file and the package
add_custom_command(TARGET installer PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:installer>)

# AutoBleemWinSetup: the Windows product's data tree (windows_install_job) behind the same kind of window -
# what the NSIS installer runs once the program is in place, and the Start Menu's "AutoBleem Setup"
add_executable(winsetup
        src/main_winsetup.cpp
        src/win32_platform.cpp
        src/win32_platform.h
        src/win32_setup_window.cpp
        src/win32_setup_window.h
)
set_target_properties(winsetup PROPERTIES OUTPUT_NAME AutoBleemWinSetup)
target_link_libraries(winsetup installer_core Threads::Threads ${CMAKE_DL_LIBS})
if (WIN32)
    target_sources(winsetup PRIVATE src/installer.rc)
    target_link_libraries(winsetup comctl32 user32 gdi32 gdiplus ole32 shell32 wininet)
    target_link_options(winsetup PRIVATE -mwindows -static -static-libgcc -static-libstdc++)
endif()
