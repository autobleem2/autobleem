# The launcher's own suites. autobleem-core's (ab_core, ab_classic's model, ab_installer) build from its
# tests/ through the same AB_BUILD_TESTS switch; both use its harness (doctest, EnvFixture, TempDir).
include(${CMAKE_SOURCE_DIR}/autobleem-core/tests/harness.cmake)

# abpad, the virtual gamepad for Apps (apps/abpad)
ab_add_test(test_abpad_core apps/test_abpad_core.cpp LIBS abpad_core)
