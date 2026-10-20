# The launcher's own suites. autobleem-core's (ab_core, ab_classic's model, ab_installer) build from its
# tests/ through the same AB_BUILD_TESTS switch; both use its harness (doctest, EnvFixture, TempDir).
include(${CMAKE_SOURCE_DIR}/autobleem-core/tests/harness.cmake)

# every language file through the Rust text engine: no empty boxes, the lines that got wider listed
# (autobleem-core lib_ableem/rust/ab_text). Native and pcusb run it; the MinGW build (its SDL through pkg-config,
# its tests never run) leaves it out
if (ABLEEM_TEXT_RUST AND NOT WIN32)
    ab_add_test(test_text_languages screens/test_text_languages.cpp LIBS ableem ab_text SDL2_ttf SDL2)
    ab_test_path_define(test_text_languages AB_LAUNCHER_RESOURCES "${CMAKE_SOURCE_DIR}/src/resources")

    # BUG-11: the glyph atlas outlives a Renderer that a static destroys at exit (the abort at quit)
    ab_add_test(test_font_atlas_exit screens/test_font_atlas_exit.cpp LIBS ableem ab_text)
    ab_test_path_define(test_font_atlas_exit AB_LAUNCHER_RESOURCES "${CMAKE_SOURCE_DIR}/src/resources")
    set_tests_properties(test_font_atlas_exit PROPERTIES SKIP_RETURN_CODE 77) # no renderer: skipped, not passed
    # the abort was one run in a few: twenty runs in a row (a failure prints "failures K/20")
    add_test(NAME test_font_atlas_exit_x20
             COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/screens/repeat_test.sh 20 $<TARGET_FILE:test_font_atlas_exit>)
    set_tests_properties(test_font_atlas_exit_x20 PROPERTIES SKIP_RETURN_CODE 77)
endif()

# abpad, the virtual gamepad for Apps (apps/abpad)
ab_add_test(test_abpad_core apps/test_abpad_core.cpp LIBS abpad_core)

# GuiSetPicker's row/panel layout formulas (evoui_set_picker.cpp) - UIREV-4, report.md P1/P2. A local model,
# the same way autobleem-core's test_menu_base_navigation.cpp/test_panel_style_footer.cpp are: the screen
# itself needs a live Gui/ThemeAssets to render, which no test host here builds.
ab_add_test(test_set_picker_layout screens/test_set_picker_layout.cpp)

# The carousel's timing on tweens (autobleem-core docs/ab-gui-plan.md, G5o5): evoui/carousel_motion.h on a real
# abgui::Tweens against a copy of the hand-written timer it replaced - a tap, a queued tap, a held-stick chain, a
# direction change mid-move, the main cover's raise/lower/snap - every cover's place compared exactly at every frame
# (every ms, 60 and 30 fps, a jittery schedule), and the DebugDriver busy while the covers move. A local model of the
# carousel's bookkeeping, as above (the Carousel itself needs a live Gui).
ab_add_test(test_carousel_motion screens/test_carousel_motion.cpp LIBS ab_gui)
target_include_directories(test_carousel_motion PRIVATE ${CMAKE_SOURCE_DIR}/src/code)

# rc/app_resolve.sh, the shell copy of AppManifest's rule, held to the same answers
ab_add_test(test_app_resolve rc/test_app_resolve.cpp)
ab_test_path_define(test_app_resolve AB_RC_DIR "${CMAKE_SOURCE_DIR}/payload/Autobleem/rc")
ab_test_path_define(test_app_resolve AB_LINUX_RC_DIR "${CMAKE_SOURCE_DIR}/payload_linux/Autobleem/rc")

# abfetch, the console's own downloader (src/tools/abfetch - built for psc and the dev hosts)
if (AB_TARGET STREQUAL "psc" OR AB_TARGET STREQUAL "dev")
    ab_add_test(test_abfetch tools/test_abfetch.cpp LIBS abfetch_core)
endif()

# abupdate (src/tools/abupdate.cpp), the built program: the --retroarch mode's arguments and progress file, the old mode
if (NOT AB_TARGET STREQUAL "win" AND NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    ab_add_test(test_abupdate tools/test_abupdate.cpp)
    add_dependencies(test_abupdate abupdate)
    ab_test_path_define(test_abupdate AB_ABUPDATE_PATH "${CMAKE_BINARY_DIR}" "$<TARGET_FILE_NAME:abupdate>")
endif()

# ab_gui G5o3: the launcher's state change on tweens - the menu row, its option move and zoom, the meta panel and the
# settings band at every millisecond against a frozen copy of the old timers (the controls need a live Gui, see the
# file's opening comment); evoui_motion.h is pure
if (AB_CORE_UI)
    ab_add_test(test_launcher_state_tweens screens/test_launcher_state_tweens.cpp LIBS ab_gui)
    target_include_directories(test_launcher_state_tweens PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui/controls)
endif()

# UIREV-40: the channel watermark's text (evoui/channel_watermark.h, pure) - which version strings are a nightly, a
# testing build or a release, the short version, and the tag's place under the pad plate
ab_add_test(test_channel_watermark screens/test_channel_watermark.cpp)
target_include_directories(test_channel_watermark PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui)

# UIREV-35: the game details section's rows and badge places (evoui/controls/evoui_meta_layout.h, pure)
# the launcher's layout profile: the 1280x720 numbers and the 4:3 canvas's from layout4x3 (evoui/evoui_layout.h, pure)
ab_add_test(test_evoui_layout screens/test_evoui_layout.cpp)
target_include_directories(test_evoui_layout PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui)

ab_add_test(test_meta_layout screens/test_meta_layout.cpp LIBS ab_gui)
target_include_directories(test_meta_layout PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui/controls)

# UIREV-36: the hint bar's fixed grid of role slots (evoui/controls/hint_slots.h, pure) - which item sits in which
# slot in each state, the dimmed line 2 items
ab_add_test(test_hint_slots screens/test_hint_slots.cpp)
target_include_directories(test_hint_slots PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui/controls)

# UIREV-37: the resume-slot screen's layout and rules (evoui/controls/resume_layout.h, pure) - the cards' places, the
# NEWEST slot, the picture's cut corners, the date format
ab_add_test(test_resume_layout screens/test_resume_layout.cpp)
target_include_directories(test_resume_layout PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui/controls)

# BUG-30: the set banner's hold time (evoui/set_banner.h, pure) - the notification timeout, the default when it is Off
ab_add_test(test_set_banner screens/test_set_banner.cpp)
target_include_directories(test_set_banner PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui)

# UIREV-54: the hold of a notification set while the launcher loads (evoui/startup_toast.h, pure)
ab_add_test(test_startup_toast screens/test_startup_toast.cpp)
target_include_directories(test_startup_toast PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui)

# BUG-41: the Memory Cards counter's text (gui/menus/memcard_counter.h, pure) - "Card n/N", "0/0" with no cards
ab_add_test(test_memcard_counter screens/test_memcard_counter.cpp)
target_include_directories(test_memcard_counter PRIVATE ${CMAKE_SOURCE_DIR}/src/code/gui)

# The set picker's tab list with and without the RetroArch tab (evoui/screens/set_picker_tabs.h, pure)
ab_add_test(test_set_picker_tabs screens/test_set_picker_tabs.cpp)
target_include_directories(test_set_picker_tabs PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui/screens)

# The CD player screen's pure rules (evoui/screens/cd_player_logic.h): times, grid cursor, bar segments, repeat cycle
ab_add_test(test_cd_player_logic screens/test_cd_player_logic.cpp)
target_include_directories(test_cd_player_logic PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui/screens)
# The Repeat and Shuffle glyphs' per-row geometry (evoui/screens/cd_glyphs.h)
ab_add_test(test_cd_glyphs screens/test_cd_glyphs.cpp)
target_include_directories(test_cd_glyphs PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui/screens)

# The disc view's pure rules (evoui/screens/disc_view_logic.h): the drive chip, R1 footer rule, crossfade
ab_add_test(test_disc_view_logic screens/test_disc_view_logic.cpp)
target_include_directories(test_disc_view_logic PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui/screens)

# The RetroArch install / remove job (evoui/ra_job.h, pure, and ra_job_service.cpp with the fake runner in tools/)
ab_add_test(test_ra_job screens/test_ra_job.cpp ${CMAKE_SOURCE_DIR}/src/code/evoui/ra_job_service.cpp)
target_include_directories(test_ra_job PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui)
ab_test_path_define(test_ra_job AB_TOOLS_DIR "${CMAKE_SOURCE_DIR}/tools")

# Copying the disc in the drive to Games as a CHD (evoui/screens/disc_rip_logic.h): who is offered the copy, the names
# of the CHD / .sbi / .m3u, the LibCrypt .sbi choice, the refusals before the rip, the popup's numbers
ab_add_test(test_disc_rip_logic screens/test_disc_rip_logic.cpp)
target_include_directories(test_disc_rip_logic PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui/screens)

# The RetroArch-installed gates (evoui/screens/ra_gates.h, pure): a PS1 game's emulator, the Lightgun set
ab_add_test(test_ra_gates screens/test_ra_gates.cpp)
target_include_directories(test_ra_gates PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui/screens)

# The PSC's core picks and ROM folders (src/resources/platform/psc.cores.cfg, roms_skip.cfg) against the console's real
# core set: autobleem-core's tests/data/psc-info, the info/ folder of the cores tarball the PSC installs
ab_add_test(test_psc_cores platform/test_psc_cores.cpp)
ab_test_path_define(test_psc_cores AB_PLATFORM_DIR "${CMAKE_SOURCE_DIR}/src/resources/platform")

# rc/pe_run.sh + rc/pe_env.sh, the start of a PE App: the real scripts on a fake mod (the bind mount needs root and
# is only checked there), and the trimmed pad table the mods read (tools/make_pe_gamecontrollerdb.py)
ab_add_test(test_pe_run rc/test_pe_run.cpp)
ab_test_path_define(test_pe_run AB_RC_DIR "${CMAKE_SOURCE_DIR}/payload/Autobleem/rc")

# rc/ab_memory.sh, overcommit 1 and the zram swap at boot: the real script on a fake /proc, /sys and /dev, run twice
ab_add_test(test_ab_memory rc/test_ab_memory.cpp)
ab_test_path_define(test_ab_memory AB_RC_DIR "${CMAKE_SOURCE_DIR}/payload/Autobleem/rc")
find_package(Python3 COMPONENTS Interpreter QUIET)
if (Python3_Interpreter_FOUND)
    add_test(NAME test_make_pe_gamecontrollerdb COMMAND ${Python3_EXECUTABLE}
             ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_make_pe_gamecontrollerdb.py)
    # tools/unshipped_themes.txt: the theme folders no package ships (ab2) and the scripts that apply the list
    add_test(NAME test_unshipped_themes COMMAND ${Python3_EXECUTABLE}
             ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_unshipped_themes.py)
    # the Sony-hash guard: no Sony BIOS image is tracked, the biospack manifests hold hashes only
    add_test(NAME test_no_sony_bios COMMAND ${Python3_EXECUTABLE}
             ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_no_sony_bios.py)
    # abvm's last-use bookkeeping write: parallel callers, and a failed write never fails `lock take`
    add_test(NAME test_abvm_last_use COMMAND ${Python3_EXECUTABLE}
             ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_abvm_last_use.py)
    # the Windows product's emunxt/ carries fonts/ beside pcsx-ab.exe (Chinese)
    add_test(NAME test_win_emulator_fonts COMMAND ${Python3_EXECUTABLE}
             ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_win_emulator_fonts.py)
    # the PE runner in the Raspberry Pi 32-bit package (one copy in payload/, copied by publish-launcher.yml's rpi branch)
    add_test(NAME test_pe_linux_package COMMAND ${Python3_EXECUTABLE}
             ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_pe_linux_package.py)
endif()

# abdialog (src/tools/abdialog), the PE dialogs: the options, the command file the scripts write, the answer codes
if (NOT AB_TARGET STREQUAL "win")
    ab_add_test(test_abdialog tools/test_abdialog.cpp LIBS abdialog_core)
endif()

# The Packages screens' decisions (evoui/screens/package_picker_logic.h, pure): no game data / one entry / the picker,
# the picker's rows and where its cursor starts, the second line's fitting at 16:9 and 4:3, "Runs with"
ab_add_test(test_package_picker screens/test_package_picker.cpp)
target_include_directories(test_package_picker PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui/screens)

# The picture that stands on the screen while a program starts (launch_picture.h, pure): RetroArch's for a RetroArch
# game or another emulator, AutoBleem's for a PS1 game and for an App (which is a foreign entry too)
ab_add_test(test_launch_picture screens/test_launch_picture.cpp)
target_include_directories(test_launch_picture PRIVATE ${CMAKE_SOURCE_DIR}/src/code)

# Whether the RetroArch program needs a newer glibc than the console's (ra_glibc_check.h, pure): the GLIBC_x.y names
# in its binary against the running libc
ab_add_test(test_ra_glibc_check screens/test_ra_glibc_check.cpp)
target_include_directories(test_ra_glibc_check PRIVATE ${CMAKE_SOURCE_DIR}/src/code)

# AUTOBLEEM-22: the game editor's rows greyed for a disc in the drive (gui/menus/game_editor_options.h, pure)
ab_add_test(test_game_editor_options screens/test_game_editor_options.cpp)
target_include_directories(test_game_editor_options PRIVATE ${CMAKE_SOURCE_DIR}/src/code/gui/menus)
