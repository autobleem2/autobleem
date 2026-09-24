# The launcher's own suites. autobleem-core's (ab_core, ab_classic's model, ab_installer) build from its
# tests/ through the same AB_BUILD_TESTS switch; both use its harness (doctest, EnvFixture, TempDir).
include(${CMAKE_SOURCE_DIR}/autobleem-core/tests/harness.cmake)

# abpad, the virtual gamepad for Apps (apps/abpad)
ab_add_test(test_abpad_core apps/test_abpad_core.cpp LIBS abpad_core)

# rc/app_resolve.sh, the shell copy of AppManifest's rule, held to the same answers
ab_add_test(test_app_resolve rc/test_app_resolve.cpp)
target_compile_definitions(test_app_resolve PRIVATE AB_RC_DIR="${CMAKE_SOURCE_DIR}/payload/Autobleem/rc"
                                                     AB_LINUX_RC_DIR="${CMAKE_SOURCE_DIR}/payload_linux/Autobleem/rc")

# abfetch, the console's own downloader (src/tools/abfetch - built for psc and the dev hosts)
if (AB_TARGET STREQUAL "psc" OR AB_TARGET STREQUAL "dev")
    ab_add_test(test_abfetch tools/test_abfetch.cpp LIBS abfetch_core)
endif()
