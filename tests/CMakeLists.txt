# Unit tests for ab_core. Host builds only - the ARM toolchain file forces AB_BUILD_TESTS off, because
# nothing here ships to the console.
#
# doctest is vendored as a single header (tests/third_party/doctest/doctest.h). It is the one piece of
# vendored code outside lib_ableem/third_party, which is right: it ends up in no binary we ship.
#
# Run with:  ctest --test-dir build_win --output-on-failure    (make_win.sh does this after a build)

add_library(doctest_main STATIC doctest_main.cpp)
target_include_directories(doctest_main PUBLIC third_party)

# the support pieces every test links: EnvFixture (Environment's setters are global), TempDir and
# makeFakeGame (a real verifiable PS1 game on disk, for tests that exercise the scanner itself).
add_library(ab_test_support STATIC support/temp_dir.cpp support/fake_game.cpp)
target_include_directories(ab_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ab_test_support ableem_engine)

# one executable per suite; add_test per executable so a crash in one does not hide the others
function(ab_add_test name)
    add_executable(${name} ${ARGN})
    # tests include app headers as "core/services/config.h", the way the app will once src/code is a root
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src/code)
    # tests/data: checked-in fixtures (the test disc images, see tests/data/generate_test_files.sh)
    target_compile_definitions(${name} PRIVATE AB_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
    target_link_libraries(${name} doctest_main ab_test_support ab_core)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

ab_add_test(test_cd_image core/test_cd_image.cpp)
ab_add_test(test_clock core/test_clock.cpp)
ab_add_test(test_config core/test_config.cpp)
ab_add_test(test_platform_config core/test_platform_config.cpp)
ab_add_test(test_game_catalog core/test_game_catalog.cpp)
ab_add_test(test_game_database core/test_game_database.cpp)
ab_add_test(test_filesystem core/test_filesystem.cpp)
ab_add_test(test_game_library core/test_game_library.cpp)
ab_add_test(test_game_query core/test_game_query.cpp)
ab_add_test(test_games_fingerprint core/test_games_fingerprint.cpp)
ab_add_test(test_game_set core/test_game_set.cpp)
ab_add_test(test_game_settings core/test_game_settings.cpp)
ab_add_test(test_lang core/test_lang.cpp)
ab_add_test(test_launch core/test_launch.cpp)
ab_add_test(test_memcard core/test_memcard.cpp)
ab_add_test(test_metadata_lookup core/test_metadata_lookup.cpp)
ab_add_test(test_memcard_image core/test_memcard_image.cpp)
ab_add_test(test_rdb_reader core/test_rdb_reader.cpp)
ab_add_test(test_resume_point core/test_resume_point.cpp)
ab_add_test(test_retroarch core/test_retroarch.cpp)
ab_add_test(test_scan_service core/test_scan_service.cpp)
ab_add_test(test_theme core/test_theme.cpp)
ab_add_test(test_thumbnail_lookup core/test_thumbnail_lookup.cpp)
ab_add_test(test_theme_converter core/test_theme_converter.cpp)
ab_add_test(test_theme_installer core/test_theme_installer.cpp)
ab_add_test(test_theme_spec core/test_theme_spec.cpp)
ab_add_test(test_harness core/test_env_fixture.cpp)
