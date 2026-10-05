# abdialog: the two dialogs a PE mod calls (a text screen, a Cross/Circle/Square/Triangle question) as AutoBleem screens
# in the current theme (abdialog.cpp says how it is run). abdialog_core is the pure part - the options, the command file,
# the answer codes - which the tests drive too; the program lands next to the launcher, where the package scripts pick
# it up. Not on Windows: a PE App runs on the console, and the program waits on signals the Windows build has not got.
add_library(abdialog_core STATIC dialog_cmd.cpp dialog_cmd.h)
target_include_directories(abdialog_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(abdialog abdialog.cpp)
target_link_libraries(abdialog abdialog_core ab_classic Threads::Threads ${CMAKE_DL_LIBS})
set_target_properties(abdialog PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
