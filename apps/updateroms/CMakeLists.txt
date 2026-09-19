# UpdateRoms: the PC-side ROM scanner for a console stick (or a Pi card) in a card reader - what the
# launcher's own scan does, with the PC's network for the databases and the box art. updateroms_core is
# the job (ab_core only, tested from tests/apps), updateroms the program on ab_classic. Built for the dev
# hosts only: it is what runs on the PC the stick is plugged into.

add_library(updateroms_core STATIC
        src/core/update_roms_job.cpp
        src/core/update_roms_job.h
)
target_include_directories(updateroms_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/code)
target_link_libraries(updateroms_core ab_core)

add_executable(updateroms
        src/main.cpp
        src/updateroms_app.cpp
        src/updateroms_app.h
        src/screens/gui_update_roms.cpp
        src/screens/gui_update_roms.h
)
set_target_properties(updateroms PROPERTIES OUTPUT_NAME UpdateRoms)
target_link_libraries(updateroms updateroms_core ab_classic Threads::Threads ${CMAKE_DL_LIBS})

# its own translations next to the binary (the stick's Autobleem/bin/autobleem/lang carries the shared
# strings; these are the tool's)
add_custom_command(TARGET updateroms PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:updateroms>)
