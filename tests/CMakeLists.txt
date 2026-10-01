# The launcher's own suites. autobleem-core's (ab_core, ab_classic's model, ab_installer) build from its
# tests/ through the same AB_BUILD_TESTS switch; both use its harness (doctest, EnvFixture, TempDir).
include(${CMAKE_SOURCE_DIR}/autobleem-core/tests/harness.cmake)

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
target_compile_definitions(test_app_resolve PRIVATE AB_RC_DIR="${CMAKE_SOURCE_DIR}/payload/Autobleem/rc"
                                                     AB_LINUX_RC_DIR="${CMAKE_SOURCE_DIR}/payload_linux/Autobleem/rc")

# abfetch, the console's own downloader (src/tools/abfetch - built for psc and the dev hosts)
if (AB_TARGET STREQUAL "psc" OR AB_TARGET STREQUAL "dev")
    ab_add_test(test_abfetch tools/test_abfetch.cpp LIBS abfetch_core)
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
ab_add_test(test_meta_layout screens/test_meta_layout.cpp)
target_include_directories(test_meta_layout PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui/controls)

# UIREV-37: the resume-slot screen's layout and rules (evoui/controls/resume_layout.h, pure) - the cards' places, the
# NEWEST slot, the picture's cut corners, the date format
ab_add_test(test_resume_layout screens/test_resume_layout.cpp)
target_include_directories(test_resume_layout PRIVATE ${CMAKE_SOURCE_DIR}/src/code/evoui/controls)
