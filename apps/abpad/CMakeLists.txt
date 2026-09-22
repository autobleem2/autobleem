# abpad: the virtual gamepad for third-party Apps - an LD_PRELOAD interposer that owns the pad and
# shows a precompiled SDL 1.2 or SDL2 program a pad it knows (docs/virtual-gamepad-plan.md).
#
# abpad_core is the logic, and it deliberately links *nothing* - not even ab_core. It ends up inside a
# preloaded shared object that is mapped into someone else's process, so every byte of it has to be
# ours and cheap: no sqlite, no miniz, no SDL. The tests link it next to ab_core, which is fine there.

add_library(abpad_core STATIC
        src/core/evdev_codes.h
        src/core/evdev_order.cpp
        src/core/evdev_order.h
        src/core/mapping.cpp
        src/core/mapping.h
        src/core/virtual_pad.cpp
        src/core/virtual_pad.h
)
target_include_directories(abpad_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(abpad_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
