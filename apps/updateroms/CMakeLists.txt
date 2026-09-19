# UpdateRoms: the PC-side ROM scanner for a console stick (or a Pi card) in a card reader - what the
# launcher's own scan does, with the PC's network for the databases and the box art. updateroms_core is
# the job (ab_core only, tested from tests/apps), updateroms the program: a plain Win32 window on
# Windows (no SDL, no AutoBleem theme - a utility, and one self-contained exe), the --quiet console path
# elsewhere. Built for the dev hosts only: it is what runs on the PC the stick is plugged into.

add_library(updateroms_core STATIC
        src/core/update_roms_job.cpp
        src/core/update_roms_job.h
)
target_include_directories(updateroms_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/code)
target_link_libraries(updateroms_core ab_core)

add_executable(updateroms
        src/main.cpp
        src/win32_window.cpp
        src/win32_window.h
)
set_target_properties(updateroms PROPERTIES OUTPUT_NAME UpdateRoms)
target_link_libraries(updateroms updateroms_core Threads::Threads ${CMAKE_DL_LIBS})

if (WIN32)
    # a GUI-subsystem exe (no console window when double-clicked; --quiet attaches to the parent's), the
    # common-controls manifest as a resource, and everything static: the MinGW runtime, winpthread, the
    # engine's own libraries - nothing of MSYS2 is needed next to it
    target_sources(updateroms PRIVATE src/updateroms.rc)
    target_link_libraries(updateroms comctl32 user32 gdi32)
    target_link_options(updateroms PRIVATE -mwindows -static -static-libgcc -static-libstdc++)
endif()

# README.txt next to the binary: the folder for a stick is the exe and that file
add_custom_command(TARGET updateroms PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:updateroms>)
