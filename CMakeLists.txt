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

# AB_TARGET: which of the five things this build is for. Exactly one AB_PLATFORM_<TARGET> is defined from it,
# and core/services/environment.h derives the semantic macros the code checks (AB_DEBUG_HOST, AB_APPLIANCE,
# AB_ROOT_RELATIVE_LAYOUT, AB_HAS_INTERNAL_GAMES) - the sources never test the CPU or the OS for this.
#   psc    the PlayStation Classic (the console): the /media tree, /gaadata's internal games, offline updates
#   rpi    the Raspberry Pi appliance (payload_linux/): forks the emulators, halts for real, no internal games,
#          everything on the exFAT data partition given on the command line, the online update
#   pcusb  the 32-bit PC USB-stick appliance: rpi's semantics on x86
#   win    the Windows product (the NSIS installer): a real target on a desktop OS
#   dev    a development host (make_win.sh, make_sys.sh): the emulators are not forked, a window,
#          keyboard-as-pad, the 1/2-arg roots - what tools/win_drive.ps1 drives
# Empty, the default: psc when cross-compiling for ARM, dev otherwise. The toolchain files force theirs.
set(AB_TARGET "" CACHE STRING "psc | rpi | pcusb | win | dev (empty: psc on ARM, dev elsewhere)")
option(AB_TARGET_RPI "Deprecated: the same as -DAB_TARGET=rpi" OFF)
if (AB_TARGET_RPI AND AB_TARGET STREQUAL "")
    message(WARNING "AB_TARGET_RPI is deprecated, use -DAB_TARGET=rpi")
    set(AB_TARGET "rpi")
endif()
if (AB_TARGET STREQUAL "")
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^arm" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(AB_TARGET "psc")
    else()
        set(AB_TARGET "dev")
    endif()
endif()
set(_ab_targets psc rpi pcusb win dev)
if (NOT AB_TARGET IN_LIST _ab_targets)
    message(FATAL_ERROR "AB_TARGET must be one of: ${_ab_targets} (got '${AB_TARGET}')")
endif()
string(TOUPPER "${AB_TARGET}" _ab_target_upper)
message(STATUS "Building for target ${AB_TARGET} (AB_PLATFORM_${_ab_target_upper})")
add_compile_definitions(AB_PLATFORM_${_ab_target_upper})

# The launcher's online update (core/services/update_service.*, the update dialogs): the appliances, the
# Windows product and the dev hosts check the download repository for a newer AutoBleem/RetroArch and fetch
# it; the console updates from a stick, offline, and carries none of this code - its branch below switches
# it off.
option(AB_ONLINE_UPDATE "Build the online update check and download (everything but the console)" ON)

message(STATUS "CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}")
if (AB_TARGET STREQUAL "psc")
	message(STATUS "Building for ARM (the console)")
	set(AB_ONLINE_UPDATE OFF) # the console has no network: its updater is offline (see the option above)
	# The console is a Cortex-A35 (ARMv8-A, run in AArch32) with NEON. GCC 8+ spells that armv8-a+simd;
	# an older GCC - the Stretch gcc-6 the Docker image builds with, the newest whose libstdc++ matches the
	# console's glibc 2.24 - takes the -mfpu spelling (its old branch here was armv7ve without NEON).
	if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.0)
		message(STATUS "CXX Compiler Version > 8")
		SET(CMAKE_C_FLAGS "-mfloat-abi=hard -march=armv8-a+simd -Os -s")
		SET(CMAKE_CXX_FLAGS " -mfloat-abi=hard -march=armv8-a+simd  -Os -s")
        message(STATUS "Newer GCC installed")
	else()
		message(STATUS "CXX Compiler Version <= 8")
		SET(CMAKE_C_FLAGS "-mfloat-abi=hard -march=armv8-a -mfpu=neon-vfpv4 -Os -s")
		SET(CMAKE_CXX_FLAGS "-mfloat-abi=hard -march=armv8-a -mfpu=neon-vfpv4 -Os -s")
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

elseif (AB_TARGET STREQUAL "rpi" OR AB_TARGET STREQUAL "pcusb")

	# the CPU flags for an appliance come from its toolchain file (a Pi 2 is armv7, not the console's armv8;
	# the PC stick is i686), and there is no Sony sysroot to add to the include path.
	message(STATUS "Building for an appliance (${AB_TARGET}) CPU flags=${CMAKE_CXX_FLAGS}")
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
# touch SDL directly at all - see CLAUDE.md. The console and the two appliances are embedded targets (no
# cursor grab, keyboard-as-pad off); a dev host and the Windows product keep lib_ableem's dev-host behaviour.
if (AB_TARGET STREQUAL "psc" OR AB_TARGET STREQUAL "rpi" OR AB_TARGET STREQUAL "pcusb")
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
        src/code/core/services/environment_setup.cpp
        src/code/core/services/environment_setup.h
        src/code/core/services/windows_host.cpp
        src/code/core/services/windows_host.h
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
        src/code/core/services/online_assets.cpp
        src/code/core/services/online_assets.h
        src/code/core/services/system.cpp
        src/code/core/services/system.h
        src/code/core/services/system_info.cpp
        src/code/core/services/system_info.h
        src/code/core/services/theme.cpp
        src/code/core/services/theme.h
        src/code/core/services/theme_converter.cpp
        src/code/core/services/theme_converter.h
        src/code/core/services/theme_installer.cpp
        src/code/core/services/theme_installer.h
        )
target_link_libraries(ab_core ableem_engine)
if (AB_ONLINE_UPDATE)
    message(STATUS "Online update: on")
    add_compile_definitions(AB_ONLINE_UPDATE)
    target_sources(ab_core PRIVATE
            src/code/core/services/update_service.cpp
            src/code/core/services/update_service.h)
endif()
if (WIN32)
    # SystemInfoService: the adapters' addresses (GetAdaptersAddresses) and inet_ntop;
    # WindowsHost: the Documents folder (SHGetKnownFolderPath) and the registry
    target_link_libraries(ab_core iphlpapi ws2_32 shell32 ole32 advapi32)
endif()

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
    # folder, for converting payload/Themes or a theme being worked on ahead of the app doing it on first load. A host tool, so it rides on the
    # tests' switch; it ships nowhere.
    add_executable(theme_convert tools/theme_convert/main.cpp)
    target_include_directories(theme_convert PRIVATE src/code)
    target_link_libraries(theme_convert ab_core)
endif()

# ab_classic: the classic UI as a library any program can draw with - Gui (the window, the theme's assets,
# the text renderer), AppBase (config.ini, the language, the theme, the clock, the UI audio: the model
# every classic screen holds as `app`), the screens that need nothing but that model (splash, confirm,
# keyboard, about, hardware information) and the list-menu framework. Links ab_core and ableem. The console
# tools under apps/ link this and nothing above it; nothing in here knows a game exists.
add_library(ab_classic STATIC
        src/code/app_base.cpp
        src/code/app_base.h
        src/code/gui/app_audio.cpp
        src/code/gui/app_audio.h
        src/code/gui/gui.cpp
        src/code/gui/gui.h
        src/code/gui/gui_font.cpp
        src/code/gui/gui_font.h
        src/code/gui/gui_screen.h
        src/code/gui/menus/gui_menu_base.h
        src/code/gui/menus/gui_string_menu.h
        src/code/gui/menus/gui_two_column_string_menu.h
        src/code/gui/screens/gui_about.cpp
        src/code/gui/screens/gui_about.h
        src/code/gui/screens/gui_confirm.cpp
        src/code/gui/screens/gui_confirm.h
        src/code/gui/screens/gui_hardware_info.cpp
        src/code/gui/screens/gui_hardware_info.h
        src/code/gui/screens/gui_keyboard.cpp
        src/code/gui/screens/gui_keyboard.h
        src/code/gui/screens/gui_splash.cpp
        src/code/gui/screens/gui_splash.h
        src/code/gui/screens/gui_text_page.cpp
        src/code/gui/screens/gui_text_page.h
        src/code/gui/starfx.cpp
        src/code/gui/starfx.h
        src/code/gui/surprise_game.cpp
        src/code/gui/surprise_game.h
        src/code/gui/panel_style.cpp
        src/code/gui/panel_style.h
        src/code/gui/text_renderer.cpp
        src/code/gui/text_renderer.h
        src/code/gui/theme_assets.cpp
        src/code/gui/theme_assets.h
)
target_link_libraries(ab_classic ab_core ableem)

# ab_ui: AutoBleem's model on top of the classic UI - App (the game library, the session, every game
# service) and the classic screens that edit games, memory cards and the options. Links ab_classic. Nothing
# in here knows the EvolutionUI launcher exists.
add_library(ab_ui STATIC
        src/code/app.cpp
        src/code/app.h
        src/code/gui/screens/gui_select_memcard.cpp
        src/code/gui/screens/gui_select_memcard.h
        src/code/gui/menus/gui_game_dir_menu.h
        src/code/gui/menus/gui_game_editor_menu.cpp
        src/code/gui/menus/gui_game_editor_ra_menu.cpp
        src/code/gui/menus/gui_game_editor_menu.h
        src/code/gui/menus/gui_game_manager_menu.cpp
        src/code/gui/menus/gui_game_manager_menu.h
        src/code/gui/menus/gui_memcards_menu.cpp
        src/code/gui/menus/gui_memcards_menu.h
        src/code/gui/menus/gui_options_menu.cpp
        src/code/gui/menus/gui_options_menu.h
        src/code/gui/menus/gui_options_menu_base.cpp
        src/code/gui/menus/gui_options_menu_base.h
        src/code/gui/menus/gui_playlists_menu.h
)
target_link_libraries(ab_ui ab_classic)

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
        src/code/evoui/controls/evoui_notification_bubble.cpp
        src/code/evoui/controls/evoui_notification_bubble.h
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
if (AB_ONLINE_UPDATE)
    target_sources(ab_evoui PRIVATE
            src/code/evoui/screens/evoui_update.cpp
            src/code/evoui/screens/evoui_update.h)
endif()

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

if (AB_TARGET STREQUAL "win")
    # the Windows product: a GUI-subsystem exe (no console window behind the launcher - the log is
    # System/Logs/autobleem.log), with the icon and a version block (src/win/autobleem.rc; the numbers from
    # the last tag at configure time - core/version.h stays the truth about the build)
    execute_process(COMMAND git describe --tags --abbrev=0 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE AB_RC_TAG OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if (NOT AB_RC_TAG)
        set(AB_RC_TAG ${AB_VERSION_FALLBACK})
    endif()
    string(REGEX MATCH "([0-9]+)\.([0-9]+)\.([0-9]+)" _rc_nums "${AB_RC_TAG}")
    if (_rc_nums)
        set(AB_RC_VERSION "${CMAKE_MATCH_1},${CMAKE_MATCH_2},${CMAKE_MATCH_3},0")
    else()
        set(AB_RC_VERSION "0,0,0,0")
    endif()
    set(AB_RC_ICON ${CMAKE_SOURCE_DIR}/src/win/autobleem.ico)
    configure_file(src/win/autobleem.rc.in ${CMAKE_BINARY_DIR}/generated/autobleem.rc @ONLY)
    target_sources(autobleem-gui PRIVATE ${CMAKE_BINARY_DIR}/generated/autobleem.rc)
    target_link_options(autobleem-gui PRIVATE -mwindows)
endif()


add_custom_command(TARGET autobleem-gui PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/resources $<TARGET_FILE_DIR:autobleem-gui>)


configure_file(src/resources/default.png default.png COPYONLY)
configure_file(src/resources/pcsx.cfg pcsx.cfg COPYONLY)

# absplash: a full-screen picture between the launcher and an emulator on the console (src/tools/absplash.cpp);
# lib_ableem's ui only, no app code
add_executable(absplash src/tools/absplash.cpp)
target_link_libraries(absplash Threads::Threads ableem ${CMAKE_DL_LIBS})


# the console tools under apps/ (PSC-Bios, ABFlashKit): built for the console and the dev hosts on
# ab_classic; the appliances and the Windows product have no PSC kernel to configure or flash
if (AB_TARGET STREQUAL "psc" OR AB_TARGET STREQUAL "dev")
    add_subdirectory(apps/pscbios)
    add_subdirectory(apps/abflashkit)
endif()

# UpdateRoms: the PC-side ROM scanner for a stick in a card reader, and the installer, which makes the
# stick from a PC - PC programs (the targets have the launcher's own scan)
if (AB_TARGET STREQUAL "dev" OR AB_TARGET STREQUAL "win")
    add_subdirectory(apps/updateroms)
    add_subdirectory(apps/installer)
endif()
