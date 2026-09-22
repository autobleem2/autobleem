# PSC-Bios: the console's hardware configuration tool (WiFi, timezone, gamepad mapping), an App the
# launcher starts from Apps/pscbios. Two targets: pscbios_core is everything that is not a screen and
# links ab_core only (so the tests can link it), pscbios is the program on ab_classic.

add_library(pscbios_core STATIC
        src/core/console_backend.cpp
        src/core/console_backend.h
        src/core/game_controller_db.cpp
        src/core/game_controller_db.h
        src/core/network_status.cpp
        src/core/network_status.h
        src/core/pad_mapping.cpp
        src/core/pad_mapping.h
        src/core/ssid_config.cpp
        src/core/ssid_config.h
)
target_include_directories(pscbios_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/code)
target_link_libraries(pscbios_core ab_core)

add_executable(pscbios
        src/main.cpp
        src/pscbios_app.cpp
        src/pscbios_app.h
        src/screens/gui_bt_pairing.cpp
        src/screens/gui_bt_pairing.h
        src/screens/gui_gamepad_menu.cpp
        src/screens/gui_gamepad_menu.h
        src/screens/gui_network_menu.cpp
        src/screens/gui_network_menu.h
        src/screens/gui_pad_config.cpp
        src/screens/gui_pad_config.h
        src/screens/gui_pscbios_main.cpp
        src/screens/gui_pscbios_main.h
        src/screens/gui_ssid_scan_menu.cpp
        src/screens/gui_ssid_scan_menu.h
        src/screens/pscbios_pages.cpp
        src/screens/pscbios_pages.h
)
target_link_libraries(pscbios pscbios_core ab_classic Threads::Threads ${CMAKE_DL_LIBS})

# the tool's own files next to the binary, the way src/resources goes next to autobleem-gui: what the
# package ships in Apps/pscbios (make_usb.py and make_psc.sh copy them from here)
add_custom_command(TARGET pscbios PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:pscbios>/pscbios_resources)
