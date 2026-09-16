# Unit tests for ab_core. Host builds only - the ARM toolchain file forces AB_BUILD_TESTS off, because
# nothing here ships to the console.
#
# doctest is vendored as a single header (tests/third_party/doctest/doctest.h). It is the one piece of
# vendored code outside lib_ableem/third_party, which is right: it ends up in no binary we ship.
#
# Run with:  ctest --test-dir build_win --output-on-failure    (make_win.sh does this after a build)

add_library(doctest_main STATIC doctest_main.cpp)
target_include_directories(doctest_main PUBLIC third_party)

# the support pieces every test links: EnvFixture (Environment's setters are global) and TempDir.
add_library(ab_test_support STATIC support/temp_dir.cpp)
target_include_directories(ab_test_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ab_test_support ableem_engine)

# one executable per suite; add_test per executable so a crash in one does not hide the others
function(ab_add_test name)
    add_executable(${name} ${ARGN})
    # tests include app headers as "core/services/config.h", the way the app will once src/code is a root
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src/code)
    target_link_libraries(${name} doctest_main ab_test_support ab_core)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

ab_add_test(test_config core/test_config.cpp)
ab_add_test(test_game_catalog core/test_game_catalog.cpp)
ab_add_test(test_game_query core/test_game_query.cpp)
ab_add_test(test_game_set core/test_game_set.cpp)
ab_add_test(test_harness core/test_env_fixture.cpp)
