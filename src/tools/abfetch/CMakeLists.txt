# abfetch: AutoBleem's own HTTP(S) downloader (main.cpp says why), over the vendored mbedTLS. abfetch_core
# is the client, which the tests drive too; the program lands next to the launcher with cacert.pem - the
# Mozilla CA bundle as curl.se publishes it (https://curl.se/ca/cacert.pem; refresh it from there).
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/mbedtls ${CMAKE_BINARY_DIR}/third_party/mbedtls)

add_library(abfetch_core STATIC http.cpp fetch.cpp)
target_include_directories(abfetch_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(abfetch_core PUBLIC abmbedtls)

add_executable(abfetch main.cpp)
target_link_libraries(abfetch abfetch_core)
# next to autobleem-gui and the other helpers, where the package scripts pick them up
set_target_properties(abfetch PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_custom_command(TARGET abfetch POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/cacert.pem
        $<TARGET_FILE_DIR:abfetch>/cacert.pem)
