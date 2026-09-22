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
        src/core/shm_block.cpp
        src/core/shm_block.h
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
    add_executable(abpadd src/daemon/abpadd.cpp)
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

# libabpad.so - the preload. POSIX only: it exists to be LD_PRELOADed, and it needs dlsym(RTLD_NEXT)
# to reach the SDL it is standing in front of.
if (UNIX AND NOT AB_TARGET STREQUAL "win")
    add_library(abpad SHARED
            src/shim/abpad_shim.cpp
            src/shim/sdl_abi.h
            src/shim/shim_state.cpp
            src/shim/shim_state.h
    )
    target_link_libraries(abpad abpad_core ${CMAKE_DL_LIBS})
    # no SDL is linked on purpose - every SDL symbol it names is resolved at run time, in the app
    set_target_properties(abpad PROPERTIES PREFIX "lib")
endif()

# padtest - the subject the shim is tested against: run it plain and preloaded, and the difference
# between the two printouts is what the shim does
if (NOT AB_TARGET STREQUAL "win")
    add_executable(padtest src/padtest/padtest.cpp)
    if (MINGW)
        target_link_libraries(padtest PkgConfig::ABPAD_SDL)
    else()
        target_link_libraries(padtest SDL2)
    endif()
endif()
