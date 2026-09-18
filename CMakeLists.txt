# 3.12 is what add_compile_definitions() below needs anyway; older minimums draw a deprecation warning
# from current CMake on every configure.
cmake_minimum_required(VERSION 3.12)
project(autobleem-gui)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_VERBOSE_MAKEFILE ON)

# CHD image support (libchdr) is vendored under lib_ableem/third_party/libchdr and built from source for
# every host, so it needs no toolchain sysroot or system install. -DAB_ENABLE_CHD=OFF still compiles it
# out (.chd games then scan as "no serial") if it's ever unwanted.
option(AB_ENABLE_CHD "Enable CHD image support (builds the vendored libchdr)" ON)

# The Raspberry Pi port (payload_rpi/). A Pi is a real target like the console - it forks the emulators - but
# its files live on the exFAT data partition it is given on the command line and it has no built-in games.
# toolchains/rpi/RPitoolchain.cmake forces this on; see AB_PLATFORM_RPI in core/services/environment.h.
option(AB_TARGET_RPI "Build for the Raspberry Pi instead of the PlayStation Classic" OFF)
if (AB_TARGET_RPI)
    message(STATUS "Building for the Raspberry Pi")
    add_compile_definitions(AB_PLATFORM_RPI)
endif()

message(STATUS "CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}")
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^arm" AND NOT AB_TARGET_RPI)
	message(STATUS "Building for ARM")
	if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.0)
		message(STATUS "CXX Compiler Version > 8")
		SET(CMAKE_C_FLAGS "-mfloat-abi=hard -march=armv8-a+simd -Os -s")
		SET(CMAKE_CXX_FLAGS " -mfloat-abi=hard -march=armv8-a+simd  -Os -s")
        message(STATUS "Newer GCC installed")
	else()
		message(STATUS "CXX Compiler Version <= 8")
		SET(CMAKE_C_FLAGS "-mfloat-abi=hard -march=armv7ve -s -Os")
		SET(CMAKE_CXX_FLAGS "-mfloat-abi=hard -march=armv7ve -s -Os")
	endif()

	# the Sony sysroot is wherever toolchains/psc/PSCtoolchainV8.cmake found the toolchain (AB_PSC_TOOLCHAIN);
	# the old hard-coded /opt/toolchain/... is only the fallback for a toolchain that is merely on PATH
	if (CMAKE_SYSROOT)
		set(_ab_psc_sysroot "${CMAKE_SYSROOT}")
	else()
		set(_ab_psc_sysroot "/opt/toolchain/armv8-sony-linux-gnueabihf/sysroot")
	endif()
	include_directories(src/include "${_ab_psc_sysroot}/usr/include")
	link_directories("${_ab_psc_sysroot}/usr/lib")

elseif (AB_TARGET_RPI)

	# the -march/-mfpu for a Pi come from the toolchain file (a Pi 2 is armv7, not the console's armv8), and
	# there is no Sony sysroot to add to the include path.
	message(STATUS "Building for ARM (Raspberry Pi) CPU flags=${CMAKE_CXX_FLAGS}")
	include_directories(src/include)

else()

    message(STATUS "Building for PC CPU type=${CMAKE_BUILD_TYPE}")
	if( "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" )
#		if (MEM_DEBUG)	# -DMEM_DEBUG=Extra in Clion does not work!  cmake doesn't see it

#			message(STATUS "Enabling all warnings")
#			# all compiler warnings
#			SET(CMAKE_C_FLAGS "-Wall -W")
#			SET(CMAKE_CXX_FLAGS "-Wall -W")

			message(STATUS "Enabling warnings")
			# the noisy ones are switched off; everything that is left should be fixed, not ignored
			SET(CMAKE_C_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function")
			SET(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function")

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
	else()
		message(STATUS "Building Release")
	endif()
endif()

find_package(Threads REQUIRED)

# lib_ableem: the portable SDL2 wrapper (own SDL discovery, own demo). autobleem-gui links it and does not
# touch SDL directly at all - see CLAUDE.md.
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
    set(ABLEEM_EMBEDDED_TARGET ON CACHE BOOL "" FORCE)
endif()

# clang-tidy (.clang-tidy at the root) on every C++ file as it compiles. Off by default: tools/lint.sh runs
# the same checks over the tree in one go, which is the usual way; this is for a build that must be clean.
# C++ only, so the vendored C (sqlite, miniz, libchdr and its codecs) is never analysed.
option(AB_ENABLE_CLANG_TIDY "Run clang-tidy on every C++ file during the build" OFF)
if (AB_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if (CLANG_TIDY_EXE)
        message(STATUS "clang-tidy: ${CLANG_TIDY_EXE}")
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    else()
        message(WARNING "AB_ENABLE_CLANG_TIDY is on but clang-tidy was not found")
    endif()
endif()
set(ABLEEM_ENABLE_CHD ${AB_ENABLE_CHD} CACHE BOOL "" FORCE)
add_subdirectory(lib_ableem)

if (NOT AB_ENABLE_CHD)
    message(STATUS "CHD support disabled")
    add_compile_definitions(AB_NO_CHD)
endif()

# ab_core: the app's model and services layer, the first of the three targets the 2026-09 refactor split src/code into.
# It links ableem_engine and nothing else, so the linker - not a convention - is what keeps it free of SDL and
# of screens. That is what makes it testable without a window.
#
# It starts small on purpose: only files that already have no Gui and no App::get() in them could move here
# without a content change. theme.*, scanner.* and util_time.* are core-shaped but each still reaches the UI
# or the composition root, so they stay in the app target until a later step gives them a seam. ab_ui and ab_evoui are not split out yet either - Gui::menuSelection()
# constructs GuiLauncher while ~20 launcher files use Gui, so the two would be a link cycle rather than a
# layering. Phase C step 14 (menuSelection -> ClassicMenuScreen) is what removes that cycle.
add_library(ab_core STATIC
        src/code/core/main.h
        src/code/core/model/game_set.h
        src/code/core/model/ps_game.cpp
        src/code/core/model/ps_game.h
        src/code/core/model/session.h
        src/code/core/model/timing.h
        src/code/core/services/config.cpp
        src/code/core/services/environment.cpp
        src/code/core/services/environment.h
        src/code/core/services/clock.cpp
        src/code/core/services/clock.h
        src/code/core/services/config.h
        src/code/core/services/game_catalog.cpp
        src/code/core/services/game_catalog.h
        src/code/core/services/game_query.cpp
        src/code/core/services/game_query.h
        src/code/core/services/game_settings.cpp
        src/code/core/services/lightgun.cpp
        src/code/core/services/game_settings.h
        src/code/core/services/launch.cpp
        src/code/core/services/launch.h
        src/code/core/services/memcard.cpp
        src/code/core/services/memcard.h
        src/code/core/services/platform_config.cpp
        src/code/core/services/platform_config.h
        src/code/core/services/process_runner.cpp
        src/code/core/services/process_runner.h
        src/code/core/services/resume_point.cpp
        src/code/core/services/resume_point.h
        src/code/core/services/retroarch.cpp
        src/code/core/services/retroarch.h
        src/code/core/services/scan_service.cpp
        src/code/core/services/scan_service.h
        src/code/core/services/system.cpp
        src/code/core/services/system.h
        src/code/core/services/theme.cpp
        src/code/core/services/theme.h
        src/code/core/services/theme_converter.cpp
        src/code/core/services/theme_converter.h
        src/code/core/services/theme_installer.cpp
        src/code/core/services/theme_installer.h
        )
target_link_libraries(ab_core ableem_engine)

# core/version.h: the version, commit and build time, regenerated on every build (see the script). The
# fallback is what a tree without git tags reports; it used to be config.ini's Version= key.
set(AB_VERSION_FALLBACK "v2.0.0-pre0")
add_custom_target(ab_version
        COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -DBINARY_DIR=${CMAKE_BINARY_DIR}
                -DVERSION_FALLBACK=${AB_VERSION_FALLBACK} -P ${CMAKE_SOURCE_DIR}/cmake/generate_version.cmake
        BYPRODUCTS ${CMAKE_BINARY_DIR}/generated/core/version.h
        COMMENT "core/version.h from git")
add_dependencies(ab_core ab_version)
target_include_directories(ab_core PUBLIC ${CMAKE_BINARY_DIR}/generated)

# ab_core's tests. Host builds only - there is no reason to cross-compile them, and both cross toolchain files
# (toolchains/psc, toolchains/rpi) switch AB_BUILD_TESTS off. Every service extracted from the screens from here on arrives with its tests
# in the same commit (the refactor's rule, see CLAUDE.md "Tests").
option(AB_BUILD_TESTS "Build the ab_core unit tests" ON)
if (AB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)

    # theme_convert <themesDir>: the ThemeInstaller over every zip and the ThemeConverter over every theme
    # folder, for converting payload/themes or a theme being worked on ahead of the app doing it on first load. A host tool, so it rides on the
    # tests' switch; it ships nowhere.
    add_executable(theme_convert tools/theme_convert/main.cpp)
    target_include_directories(theme_convert PRIVATE src/code)
    target_link_libraries(theme_convert ab_core)
endif()

# ab_ui: the classic UI and everything a screen needs - Gui (the window, the theme's assets, the text
# renderer), the classic screens and menus, Theme, AppAudio, the Scanner's splash listener, the memory-card
# editor, and App itself, which is the model every screen holds as `app`. Links ab_core and ableem. Nothing
# in here knows the EvolutionUI launcher exists.
add_library(ab_ui STATIC
        src/code/app.cpp
        src/code/app.h
        src/code/gui/app_audio.cpp
        src/code/gui/app_audio.h
        src/code/gui/gui.cpp
        src/code/gui/gui.h
        src/code/gui/screens/gui_about.cpp
        src/code/gui/screens/gui_about.h
        src/code/gui/screens/gui_confirm.cpp
        src/code/gui/screens/gui_confirm.h
        src/code/gui/gui_font.cpp
        src/code/gui/gui_font.h
        src/code/gui/screens/gui_keyboard.cpp
        src/code/gui/screens/gui_keyboard.h
        src/code/gui/screens/gui_pad_test.cpp
        src/code/gui/screens/gui_pad_test.h
        src/code/gui/gui_screen.h
        src/code/gui/screens/gui_scroll_win.cpp
        src/code/gui/screens/gui_scroll_win.h
        src/code/gui/screens/gui_select_memcard.cpp
        src/code/gui/screens/gui_select_memcard.h
        src/code/gui/screens/gui_splash.cpp
        src/code/gui/screens/gui_splash.h
        src/code/gui/menus/gui_game_dir_menu.h
        src/code/gui/menus/gui_game_editor_menu.cpp
        src/code/gui/menus/gui_game_editor_ra_menu.cpp
        src/code/gui/menus/gui_game_editor_menu.h
        src/code/gui/menus/gui_game_manager_menu.cpp
        src/code/gui/menus/gui_game_manager_menu.h
        src/code/gui/menus/gui_memcards_menu.cpp
        src/code/gui/menus/gui_memcards_menu.h
        src/code/gui/menus/gui_menu_base.h
        src/code/gui/menus/gui_options_menu.cpp
        src/code/gui/menus/gui_options_menu.h
        src/code/gui/menus/gui_options_menu_base.cpp
        src/code/gui/menus/gui_options_menu_base.h
        src/code/gui/menus/gui_playlists_menu.h
        src/code/gui/menus/gui_string_menu.h
        src/code/gui/menus/gui_two_column_string_menu.h
        src/code/gui/starfx.cpp
        src/code/gui/starfx.h
        src/code/gui/surprise_game.cpp
        src/code/gui/surprise_game.h
        src/code/gui/text_renderer.cpp
        src/code/gui/text_renderer.h
        src/code/gui/theme_assets.cpp
        src/code/gui/theme_assets.h
)
target_link_libraries(ab_ui ab_core ableem)

# ab_evoui: the EvolutionUI launcher (src/code/evoui/) - the screen, the carousel, the launcher's own
# sub-screens, and under controls/ the PsObj family of animated elements it is built from.
# Links ab_ui. Gui::menuSelection() used to construct GuiLauncher, which made these two a link cycle; the
# classic menu is a screen in the executable now (phase C step 14), which is what let the targets exist.
add_library(ab_evoui STATIC
        src/code/evoui/card_edit.cpp
        src/code/evoui/card_edit.h
        src/code/evoui/carousel.cpp
        src/code/evoui/carousel.h
        src/code/evoui/carousel_game.cpp
        src/code/evoui/carousel_game.h
        src/code/evoui/controls/evoui_centerlabel.cpp
        src/code/evoui/controls/evoui_centerlabel.h
        src/code/evoui/controls/evoui_menu.cpp
        src/code/evoui/controls/evoui_menu.h
        src/code/evoui/controls/evoui_meta.cpp
        src/code/evoui/controls/evoui_meta.h
        src/code/evoui/controls/evoui_move_btn.cpp
        src/code/evoui/controls/evoui_move_btn.h
        src/code/evoui/controls/evoui_obj.cpp
        src/code/evoui/controls/evoui_obj.h
        src/code/evoui/controls/evoui_settings_back.cpp
        src/code/evoui/controls/evoui_settings_back.h
        src/code/evoui/controls/evoui_stateselector.cpp
        src/code/evoui/controls/evoui_stateselector.h
        src/code/evoui/controls/evoui_static.cpp
        src/code/evoui/controls/evoui_static.h
        src/code/evoui/controls/evoui_zoom_btn.cpp
        src/code/evoui/controls/evoui_zoom_btn.h
        src/code/evoui/controls/evoui_notification_line.cpp
        src/code/evoui/controls/evoui_notification_line.h
        src/code/evoui/screens/evoui_app_start.cpp
        src/code/evoui/screens/evoui_app_start.h
        src/code/evoui/screens/evoui_btn_guide.cpp
        src/code/evoui/screens/evoui_btn_guide.h
        src/code/evoui/screens/evoui_launcher.h
        src/code/evoui/screens/evoui_mc_manager.cpp
        src/code/evoui/screens/evoui_mc_manager.h
        src/code/evoui/screens/evoui_system_menu.cpp
        src/code/evoui/screens/evoui_system_menu.h
        src/code/evoui/screens/evoui_launcher_actions.cpp
        src/code/evoui/screens/evoui_launcher_input.cpp
        src/code/evoui/screens/evoui_launcher_screen.cpp
)
target_link_libraries(ab_evoui ab_ui)

# the program: main() and AutoBleem (App plus the outer loop, which shows the launcher directly).
add_executable(
        autobleem-gui
        src/code/autobleem.cpp
        src/code/autobleem.h
        src/code/main.cpp
)

target_link_libraries(autobleem-gui
         Threads::Threads
         ab_evoui
         ${CMAKE_DL_LIBS}
)


#add_custom_command(TARGET autobleem-gui PRE_BUILD
#	   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
#	   VERBATIM
#       COMMAND /bin/bash make_english.txt.sh
#	   )

add_custom_command(TARGET autobleem-gui PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/resources $<TARGET_FILE_DIR:autobleem-gui>)


configure_file(src/resources/default.png default.png COPYONLY)
configure_file(src/resources/pcsx.cfg pcsx.cfg COPYONLY)

