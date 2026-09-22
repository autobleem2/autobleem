# abpad: the virtual gamepad for third-party Apps - an LD_PRELOAD interposer that owns the pad and
# shows a precompiled SDL 1.2 or SDL2 program a pad it knows (docs/virtual-gamepad-plan.md).
#
# abpad_core is the logic, and it deliberately links *nothing* - not even ab_core. It ends up inside a
# preloaded shared object that is mapped into someone else's process, so every byte of it has to be
# ours and cheap: no sqlite, no miniz, no SDL. The tests link it next to ab_core, which is fine there.

add_library(abpad_core STATIC
        src/core/key_names.cpp
        src/core/key_names.h
        src/core/mapping.cpp
        src/core/mapping.h
        src/core/profile.cpp
        src/core/profile.h
        src/core/shared_state.cpp
        src/core/shared_state.h
        src/core/virtual_pad.cpp
        src/core/virtual_pad.h
)
target_include_directories(abpad_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(abpad_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# abpadd - the virtual gamepad itself: the one process that reads the pads, through SDL2's
# GameController API with our gamecontrollerdb.txt, publishing what it finds for the shim to serve.
# Its own SDL2 discovery rather than lib_ableem's: an imported target from a find_package in another
# directory is not ours to rely on, and this one wants SDL2 alone (no image/mixer/ttf).
if (NOT AB_TARGET STREQUAL "win")
    add_executable(abpadd
            src/daemon/abpadd.cpp
            src/daemon/shm_block.cpp
            src/daemon/shm_block.h
    )
    target_link_libraries(abpadd abpad_core)
    if (MINGW)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(ABPAD_SDL REQUIRED IMPORTED_TARGET sdl2)
        target_link_libraries(abpadd PkgConfig::ABPAD_SDL)
    else()
        find_package(SDL2 REQUIRED)
        target_link_libraries(abpadd SDL2)
    endif()
endif()
