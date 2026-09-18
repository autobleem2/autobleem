cmake_minimum_required(VERSION 3.6)
project(autobleem-gui)

set(CMAKE_CXX_STANDARD 11)
#set(CMAKE_VERBOSE_MAKEFILE ON)

message(STATUS "CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}")
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
    message(STATUS "Building for ARM")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.0)
        message(STATUS "CXX Compiler Version > 8")
        SET(CMAKE_C_FLAGS "-mfloat-abi=hard -march=armv8-a+simd -O2 -s")
        SET(CMAKE_CXX_FLAGS " -mfloat-abi=hard -march=armv8-a+simd  -O2 -s")
        message(STATUS "Newer GCC installed")
    else ()
        message(STATUS "CXX Compiler Version <= 8")
        SET(CMAKE_C_FLAGS "-mfloat-abi=hard -march=armv7ve -s -O2")
        SET(CMAKE_CXX_FLAGS "-mfloat-abi=hard -march=armv7ve -s  -O2")
    endif ()

    include_directories(src/include "/opt/toolchain/armv8-sony-linux-gnueabihf/sysroot/usr/include")
    link_directories(/opt/toolchain/armv8-sony-linux-gnueabihf/sysroot/usr/lib)

else ()

    message(STATUS "Building for PC CPU type=${CMAKE_BUILD_TYPE}")
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        #		if (MEM_DEBUG)	# -DMEM_DEBUG=Extra in Clion does not work!  cmake doesn't see it

        #			message(STATUS "Enabling all warnings")
        #			# all compiler warnings
        #			SET(CMAKE_C_FLAGS "-Wall -W")
        #			SET(CMAKE_CXX_FLAGS "-Wall -W")

        message(STATUS "Enabling some warnings")
        # some compiler warnings
        SET(CMAKE_C_FLAGS "-Wreturn-type -Wuninitialized -Wall -g -o0 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer")
        SET(CMAKE_CXX_FLAGS "-Wreturn-type -Wuninitialized -Wall -g -O0 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer")

        #			message(STATUS "Enabling additional memory debugging")
        # memory sanitizer with all compiler warnings
        #			SET(CMAKE_C_FLAGS "-fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -ggdb -Wall -W")
        #			SET(CMAKE_CXX_FLAGS "-fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -ggdb -Wall -W")

        # memory sanitizer with compiler warning for not returning a value from a function and uninitialized vars
        #			SET(CMAKE_C_FLAGS "-fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -ggdb -Wreturn-type -Wuninitialized")
        #			SET(CMAKE_CXX_FLAGS "-fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -ggdb -Wreturn-type -Wuninitialized")

        # memory sanitizer with no additonal compiler warnings
        #			SET(CMAKE_C_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
        #			SET(CMAKE_CXX_FLAGS "-fsanitize=address -fno-omit-frame-pointer")

        #			SET(LDFLAGS "-fsanitize=address -fno-omit-frame-pointer")

        #		endif()
    else ()
        message(STATUS "Building Release")
    endif ()
endif ()

find_package(Threads REQUIRED
        SDL2 REQUIRED)


add_executable(
        pscbios
        src/code/DebugTimer.cpp
        src/code/DebugTimer.h
        src/code/DirEntry.cpp
        src/code/DirEntry.h
        src/code/gui/SDL_FontCache.c
        src/code/engine/cfgprocessor.cpp
        src/code/engine/cfgprocessor.h
        src/code/engine/config.cpp
        src/code/engine/config.h
        src/code/engine/inifile.cpp
        src/code/engine/inifile.h
        src/code/engine/padmapper.cpp
        src/code/engine/padmapper.h
        src/code/environment.cpp
        src/code/environment.h
        src/code/gui/gui.cpp
        src/code/gui/gui.h
        src/code/gui/gui_about.cpp
        src/code/gui/gui_about.h
        src/code/gui/gui_confirm.cpp
        src/code/gui/gui_confirm.h
        src/code/gui/gui_font.cpp
        src/code/gui/gui_font.h
        src/code/gui/gui_font_wrapper.h
        src/code/gui/gui_keyboard.cpp
        src/code/gui/gui_keyboard.h
        src/code/gui/gui_screen.cpp
        src/code/gui/gui_screen.h
        src/code/gui/gui_sdl_wrapper.cpp
        src/code/gui/gui_sdl_wrapper.h

        src/code/gui/menus/gui_menuBase.h
        src/code/gui/menus/gui_networkMenu.cpp
        src/code/gui/menus/gui_networkMenu.h
        src/code/gui/menus/gui_stringMenu.h
        src/code/gui/menus/gui_twoColumnStringMenu.h
        src/code/gui/starfx.cpp
        src/code/gui/starfx.h
        src/code/lang.cpp
        src/code/lang.h
        src/code/main.cpp
        src/code/main.h
        src/code/util.cpp
        src/code/util.h
        src/code/gui/abl.c
        src/code/engine/network_controller.cpp src/code/engine/network_controller.h src/code/gui/menus/gui_ssidScanMenu.cpp
        src/code/gui/menus/gui_ssidScanMenu.h src/code/gui/menus/gui_timezoneSelect.cpp src/code/gui/menus/gui_timezoneSelect.h
        src/code/bluetooth/bluetool.cpp src/code/bluetooth/bluetool.h src/code/gui/menus/gui_btMenu.cpp src/code/gui/menus/gui_btMenu.h
        src/code/gui/gamepad/gui_padconfig.cpp src/code/gui/gamepad/gui_padconfig.h src/code/gui/gamepad/joy_info.cpp src/code/gui/gamepad/joy_info.h
        src/code/gui/gamepad/gui_gamepadMenu.cpp src/code/gui/gamepad/gui_gamepadMenu.h src/code/gui/gamepad/dualshock3_info.cpp
        src/code/gui/gamepad/dualshock3_info.h src/code/engine/gcdbprocessor.cpp src/code/engine/gcdbprocessor.h src/code/gui/gamepad/bluetooth_info.cpp src/code/gui/gamepad/bluetooth_info.h)


target_link_libraries(pscbios
        Threads::Threads
        SDL2
        SDL2_image
        SDL2_mixer
        SDL2_ttf

        ${CMAKE_DL_LIBS}
        )


add_custom_command(TARGET pscbios PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/resources $<TARGET_FILE_DIR:pscbios>)




