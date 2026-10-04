#!/bin/sh
#
# Sourced by rc/pe_run.sh (after rc/app_env.sh): builds, in RAM, what a PE mod's launch.sh finds around it, and
# takes it down again. POSIX sh, the console only. Nothing of the mod is edited - the mod's own launch.sh runs as
# it was written; what it expects is put where it looks (autobleem-main: the PE apps design, "Runtime env builder"):
#
#   a  /var/volatile/project_eris.cfg   the variables the mod sources (written from the template below)
#   b  /var/volatile/launchtmp          a link to the App's folder - the mods cd into it and keep their data there
#   c  $PE_TREE/project_eris/           PROJECT_ERIS_PATH: bin/ (the dialog stand-ins), lib/ (gl4es from the libs
#                                       pack, sdl_remap_arm.so = abpad's shim), etc/boot_menu/gamecontrollerdb.txt
#                                       (the trimmed copy rc/pe_gamecontrollerdb.txt, < 100 KB)
#   d  /media/project_eris              an empty folder on the stick the tree is bind-mounted onto while the mod
#                                       runs, so the mods' absolute /media/project_eris/... paths resolve; the
#                                       App's folder is bound under .../SUP/launchers/<launcher_filename> too
#   e  /data/power/disable              remembered, and put back at the end (the mods set it to 2 and back to 1
#                                       themselves, but a Reset kills them before the second write)
#
# Every place is a variable so a test can run all of it in a scratch folder (PE_TREE, PE_VOLATILE, PE_POWER_FLAG,
# PE_MOUNT_POINT); the defaults are the console's.
#
# Needs from the caller: AB_APP_DIR (the App's folder), AB_ROOT, AB_LOG_DIR; rc/app_env.sh sourced (the libs pack in
# /tmp/applib, the virtual pad). Sets PE_* variables and defines pe_prepare, pe_cleanup, pe_log.

: "${PE_TREE:=/tmp/pe}"
: "${PE_VOLATILE:=/var/volatile}"
: "${PE_POWER_FLAG:=/data/power/disable}"
: "${PE_MOUNT_POINT:=/media/project_eris}"
: "${PE_RC_DIR:=$AB_ROOT/Autobleem/rc}"
PE_ROOT=$PE_TREE/project_eris
PE_RUN_DIR=$PE_TREE/run
PE_LOG_DIR=$AB_LOG_DIR/pe
PE_DIALOG_LOG=$PE_LOG_DIR/dialogs.log
export PE_RUN_DIR PE_DIALOG_LOG

pe_log() {
    mkdir -p "$PE_LOG_DIR" 2>/dev/null
    echo "$(date '+%H:%M:%S' 2>/dev/null) $*" >> "$PE_LOG_DIR/pe_run.log" 2>/dev/null
}

# pe_ini_get FILE SECTION KEY: the value, "" when there is none (CR dropped; section and key are a-z0-9 names)
pe_ini_get() {
    sed -n -e "/^\[$2\]/,/^\[/{/^$3=/{s/^$3=//;p;q;}}" "$1" 2>/dev/null | tr -d '\r'
}

# pe_cfg_get FILE KEY: a launcher.cfg value (launcher_filename="openlara"), quotes and CR dropped
pe_cfg_get() {
    sed -n -e "s/^$2=//p" "$1" 2>/dev/null | head -n 1 | tr -d '\r"'
}

# pe_children PID: the pid and every process below it, from /proc (no job control, no pgrep)
pe_tree() {
    pe_all=" $1 "
    pe_more=1
    while [ "$pe_more" = 1 ]; do
        pe_more=0
        for pe_st in /proc/[0-9]*/stat; do
            [ -r "$pe_st" ] || continue
            read -r pe_line < "$pe_st" 2>/dev/null || continue
            pe_pid=${pe_st#/proc/}
            pe_pid=${pe_pid%/stat}
            pe_rest=${pe_line##*) } # after the command name (which may hold blanks): "S ppid pgrp ..."
            set -- $pe_rest
            case "$pe_all" in
                *" $2 "*)
                    case "$pe_all" in
                        *" $pe_pid "*) ;;
                        *) pe_all="$pe_all$pe_pid "; pe_more=1 ;;
                    esac
                    ;;
            esac
        done
    done
    echo $pe_all
}

# pe_signal SIG PID: the signal to the process and all below it
pe_signal() {
    for pe_p in $(pe_tree "$2"); do
        kill "-$1" "$pe_p" 2>/dev/null
    done
}

pe_mounted() {
    grep -q " $1 " /proc/mounts 2>/dev/null
}

# anything mounted at or below the RAM tree (the App's folder bound under its launchers folder)
pe_tree_mounted() {
    grep -q " $PE_TREE[/ ]" /proc/mounts 2>/dev/null
}

# the mounts of a run that did not clean up: below the tree, deepest first, then the placeholder
pe_unmount_stale() {
    for pe_m in $(grep -o " $PE_TREE/[^ ]* " /proc/mounts 2>/dev/null | sort -r); do
        umount "$pe_m" 2>/dev/null || umount -l "$pe_m" 2>/dev/null
    done
    if pe_mounted "$PE_MOUNT_POINT"; then
        umount "$PE_MOUNT_POINT" 2>/dev/null || umount -l "$PE_MOUNT_POINT" 2>/dev/null
    fi
    for pe_r in $(pe_ini_get "$PE_RC_DIR/pe_compat.ini" "$PE_FILENAME" remap | tr ',' ' '); do
        while pe_mounted "$AB_APP_DIR/$pe_r"; do
            umount "$AB_APP_DIR/$pe_r" 2>/dev/null || umount -l "$AB_APP_DIR/$pe_r" 2>/dev/null || break
        done
    done
}

pe_prepare() {
    PE_FILENAME=$(pe_cfg_get "$AB_APP_DIR/launcher.cfg" launcher_filename)
    [ -n "$PE_FILENAME" ] || PE_FILENAME=$(basename "$AB_APP_DIR" | sed 's/^pe-//')
    PE_LAUNCHTMP=$PE_VOLATILE/launchtmp
    PE_MOUNTED=
    PE_APP_BOUND=
    PE_BOUND_PATH=$PE_MOUNT_POINT

    # c: the tree. A run that was killed (Reset is a SIGKILL after three seconds) can leave its binds: they go
    # first, and the old tree is only deleted when nothing of the stick is mounted in it any more - rm -rf
    # through a bind would reach the App's own files
    pe_unmount_stale
    if pe_tree_mounted; then
        pe_log "$PE_TREE still has a mount in it that would not go - not starting"
        return 1
    fi
    rm -rf "$PE_TREE" 2>/dev/null
    mkdir -p "$PE_ROOT/bin" "$PE_ROOT/lib" "$PE_ROOT/etc/boot_menu" "$PE_ROOT/etc/project_eris/IMG" \
        "$PE_ROOT/etc/project_eris/SUP/launchers/$PE_FILENAME" "$PE_RUN_DIR" "$PE_LOG_DIR" || return 1
    for pe_tool in sdl_text_display sdl_input_text_display sdl_display; do
        cp -f "$PE_RC_DIR/pe/$pe_tool" "$PE_ROOT/bin/$pe_tool" && chmod 755 "$PE_ROOT/bin/$pe_tool"
    done
    # lib/: what Project Eris gave its mods in ${PROJECT_ERIS_PATH}/lib (some mods set LD_LIBRARY_PATH to this and
    # nothing else). gl4es (libGL, libGLU; MIT) ships with the launcher in rc/pe/lib - the libs pack is not
    # something every stick has - and the pack's copy in /tmp/applib is the fallback. Our SDL2 goes in as well:
    # a mod that points the loader at this folder alone would otherwise run on the firmware's old SDL2 (2.0.4),
    # which the pad shim and the console's Wayland patch were not made for.
    for pe_lib in libGL.so.1 libGLU.so.1; do
        if [ -e "$PE_RC_DIR/pe/lib/$pe_lib" ]; then
            ln -sf "$PE_RC_DIR/pe/lib/$pe_lib" "$PE_ROOT/lib/$pe_lib"
        elif [ -e "${PE_APPLIB:-/tmp/applib}/$pe_lib" ]; then
            ln -sf "${PE_APPLIB:-/tmp/applib}/$pe_lib" "$PE_ROOT/lib/$pe_lib"
        else
            pe_log "no $pe_lib (neither $PE_RC_DIR/pe/lib nor ${PE_APPLIB:-/tmp/applib}) - a mod that needs gl4es will not start"
        fi
    done
    if [ -e "${PE_SDL_DIR:-/tmp/lib}/libSDL2-2.0.so.0" ]; then
        ln -sf "${PE_SDL_DIR:-/tmp/lib}/libSDL2-2.0.so.0" "$PE_ROOT/lib/libSDL2-2.0.so.0"
    else
        pe_log "no ${PE_SDL_DIR:-/tmp/lib}/libSDL2-2.0.so.0 - the mods use the firmware's SDL2"
    fi
    # the mods preload sdl_remap_arm.so to make the console pad readable: abpad's shim does that job here
    if [ -f "$AB_PAD_DIR/libabpad.so" ]; then
        ln -sf "$AB_PAD_DIR/libabpad.so" "$PE_ROOT/lib/sdl_remap_arm.so"
    else
        pe_log "no abpad shim ($AB_PAD_DIR/libabpad.so) - the pad is not remapped"
    fi
    if [ -f "$PE_RC_DIR/pe_gamecontrollerdb.txt" ]; then
        cp -f "$PE_RC_DIR/pe_gamecontrollerdb.txt" "$PE_ROOT/etc/boot_menu/gamecontrollerdb.txt"
    else
        pe_log "no $PE_RC_DIR/pe_gamecontrollerdb.txt - the mods that read it get no pad table"
    fi
    # PE mods get the console's own pad layout as the virtual pad, with the App's pad.ini still on top
    if [ -n "$AB_PAD_DEFAULTS" ] && [ -f "$AB_PAD_DEFAULTS" ]; then
        { cat "$AB_PAD_DEFAULTS"; printf '\n# PE apps: the console pad as it is\nvirtual = psc\n'; } > "$PE_RUN_DIR/pad.pe.ini"
        AB_PAD_DEFAULTS=$PE_RUN_DIR/pad.pe.ini
        export AB_PAD_DEFAULTS
    fi

    # the mods' own pad-remap preloads (remap= in rc/pe_compat.ini, a file name in the App's folder, DraStic's
    # drastic_sdl_remap.so): abpad's shim is bound over each for the run - the file on the stick is not touched
    PE_REMAP_BOUND=
    for pe_r in $(pe_ini_get "$PE_RC_DIR/pe_compat.ini" "$PE_FILENAME" remap | tr ',' ' '); do
        case "$pe_r" in */* | .*) continue ;; esac
        if [ -f "$AB_APP_DIR/$pe_r" ] && [ -f "$AB_PAD_DIR/libabpad.so" ]; then
            if mount -o bind "$AB_PAD_DIR/libabpad.so" "$AB_APP_DIR/$pe_r" 2>/dev/null; then
                PE_REMAP_BOUND="$PE_REMAP_BOUND $pe_r"
            else
                pe_log "could not bind abpad over $pe_r - the mod keeps its own remap"
            fi
        fi
    done

    # d: the placeholder on the stick (empty)
    mkdir -p "$PE_MOUNT_POINT" 2>/dev/null
    if mount -o bind "$PE_ROOT" "$PE_MOUNT_POINT" 2>/dev/null; then
        PE_MOUNTED=1
        if mount -o bind "$AB_APP_DIR" "$PE_MOUNT_POINT/etc/project_eris/SUP/launchers/$PE_FILENAME" 2>/dev/null; then
            PE_APP_BOUND=1
        else
            pe_log "could not bind $AB_APP_DIR under the launchers folder - a mod using its absolute path will not find itself"
        fi
    else
        PE_BOUND_PATH=$PE_ROOT
        pe_log "could not bind $PE_ROOT onto $PE_MOUNT_POINT - the mods get PROJECT_ERIS_PATH=$PE_ROOT; absolute /media/project_eris paths will fail"
    fi

    # a: the variables (facts 1.2: Project Eris's own defaults, the paths moved where this tree is)
    PE_CFG=$PE_VOLATILE/project_eris.cfg
    mkdir -p "$PE_VOLATILE" 2>/dev/null
    cat > "$PE_CFG" <<CFG
RUNTIME_LOG="1"
REFRESH_LOGS="1"
FORCE_REDUMP="0"
LINK_EMMC_AND_USB="1"
LINK_ALPHABETICALISE="1"
LAUNCH_RA_FROM_STOCK_UI="0"
GENERATE_INTERNAL_GAME_PLAYLIST="1"
GENERATE_EXTERNAL_GAME_PLAYLIST="1"
ENABLE_NETWORKING="0"
ENABLE_BLUETOOTH="0"
DISPLAY_RA_LOADING_SCREEN="1"
BOOT_SPLASH="1"
BOOT_QUICK="0"
BOOT_DISABLE_HEALTH="0"
BOOT_TARGET_STOCK_BM="1"
UI_APP_LAUNCHERS="1"
MOUNTPOINT="/media"
PROJECT_ERIS_PATH="$PE_BOUND_PATH"
RETROARCH_PATH="\${PROJECT_ERIS_PATH}/opt/retroarch"
IMAGES_PATH="\${PROJECT_ERIS_PATH}/etc/project_eris/IMG"
THEMES_PATH="\${PROJECT_ERIS_PATH}/etc/project_eris/THEME"
SOUNDS_PATH="\${PROJECT_ERIS_PATH}/etc/project_eris/SND"
RUNTIME_LOG_PATH="$PE_LOG_DIR"
RUNTIME_EXE_PATH="\${PROJECT_ERIS_PATH}/bin"
RUNTIME_FLAG_PATH="\${PROJECT_ERIS_PATH}/flags"
DUMP_PATH="\${MOUNTPOINT}/dump"
SELECTED_THEME="modmyclassic"
OVERRIDE_THEME_MUSIC="0"
RANDOM_THEME_ONLOAD="0"
BOOT_MENU_MUSIC="1"
PROJECT_ERIS_ROOT_PATH="\${PROJECT_ERIS_PATH}/etc/project_eris"
EMULATIONSTATION_PATH="\${PROJECT_ERIS_PATH}/opt/emulationstation"
ES_CFG_PATH="\${PROJECT_ERIS_PATH}/opt/emulationstation/.emulationstation"
SUPLEMENTARY_PATH="\${PROJECT_ERIS_PATH}/etc/project_eris/SUP"
VOLATILE="$PE_VOLATILE"
GAADATA="/gaadata"
GAME_PATH="\${MOUNTPOINT}/games"
ROMS_PATH="\${MOUNTPOINT}/roms"
REGION="\$(cat "\${GAADATA}/geninfo/REGION" 2>/dev/null)"
USB_ONLY="0"
SET_GAADATA_WRITABLE="0"
SKIP_BOOTUP_CHECKS="0"
CFG
    # the mods that name the tree's tools by their short names find them on the PATH as they do under the original
    PATH="$PE_BOUND_PATH/bin:$PATH"
    export PATH

    # b: the App's folder where the mods look for it
    rm -f "$PE_LAUNCHTMP" 2>/dev/null
    ln -s "$AB_APP_DIR" "$PE_LAUNCHTMP" || return 1

    # e: the power flag
    PE_POWER_OLD=
    if [ -f "$PE_POWER_FLAG" ]; then
        PE_POWER_OLD=$(cat "$PE_POWER_FLAG" 2>/dev/null)
    fi
    return 0
}

# pe_trail: what the pad layer was given for this launch, into pe_run.log (the RAM tree is gone after the run, so the
# facts are written down while they are true). Format, one block per launch, lines "pad: <what>: <value>":
#   LD_PRELOAD, AB_PAD_DEFAULTS, AB_PAD_PROFILE, AB_APP_VIRTUAL_PAD, SDL_GAMECONTROLLERCONFIG_FILE, LD_LIBRARY_PATH
#   pad.pe.ini: the defaults the shim reads (virtual = psc last)
#   abpad.state / mappings: whether the daemon published, and the pads it resolved (name and GUID)
#   input: the kernel's input devices (name + handlers) - a pad with no js/event handler is invisible to any SDL
#   binary <name>: for each program in the App's folder, whether it names libSDL2 (the preload can reach it) or
#   carries its own SDL (it cannot: only SDL_GAMECONTROLLERCONFIG_FILE and the kernel's own devices help)
#   remap: the mod's remap files abpad was bound over
pe_trail() {
    pe_log "pad: LD_PRELOAD=$LD_PRELOAD"
    pe_log "pad: AB_PAD_DEFAULTS=$AB_PAD_DEFAULTS AB_PAD_PROFILE=$AB_PAD_PROFILE AB_APP_VIRTUAL_PAD=$AB_APP_VIRTUAL_PAD"
    pe_log "pad: SDL_GAMECONTROLLERCONFIG_FILE=$SDL_GAMECONTROLLERCONFIG_FILE LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
    if [ -n "$AB_PAD_DEFAULTS" ] && [ -f "$AB_PAD_DEFAULTS" ]; then
        grep -v '^[[:space:]]*\(#\|$\)' "$AB_PAD_DEFAULTS" | while read -r pe_l; do pe_log "pad: pad.pe.ini: $pe_l"; done
    fi
    if [ -f /tmp/abpad.state ]; then pe_log "pad: abpad.state: published"; else pe_log "pad: abpad.state: NOT there (no daemon)"; fi
    if [ -f /tmp/abpad.state.mappings ]; then
        grep -v '^#' /tmp/abpad.state.mappings | cut -d, -f1,2 | while read -r pe_l; do pe_log "pad: mappings: $pe_l"; done
    else
        pe_log "pad: mappings: none"
    fi
    grep -E '^(N: Name|H: Handlers)' /proc/bus/input/devices 2>/dev/null | cut -c1-100 | while read -r pe_l; do
        pe_log "pad: input: $pe_l"
    done
    for pe_b in "$AB_APP_DIR"/*; do
        [ -f "$pe_b" ] && [ -x "$pe_b" ] || continue
        [ "$(head -c 4 "$pe_b" 2>/dev/null | tail -c 3)" = ELF ] || continue
        case "$pe_b" in *.so | *.so.*) continue ;; esac
        if grep -q 'libSDL2-2.0.so.0' "$pe_b" 2>/dev/null; then
            pe_log "pad: binary $(basename "$pe_b"): names libSDL2 (the preload reaches it)"
        else
            pe_log "pad: binary $(basename "$pe_b"): no libSDL2 name (own SDL or none: the preload cannot reach it)"
        fi
    done
    pe_log "pad: remap files bound over by abpad:${PE_REMAP_BOUND:- none}"
}

# the abpad logs' last lines after the run (the shim writes a load line per process into abpad.log)
pe_trail_end() {
    for pe_f in "$AB_ABPAD_LOG_DIR/abpadd.log" "$AB_ABPAD_LOG_DIR/abpad.log"; do
        [ -f "$pe_f" ] || continue
        tail -n 25 "$pe_f" 2>/dev/null | while read -r pe_l; do pe_log "pad: $(basename "$pe_f"): $pe_l"; done
    done
}

# pe_cleanup: everything pe_prepare did, in the reverse order. Safe to call twice.
pe_cleanup() {
    [ -n "$PE_CLEANED" ] && return 0
    PE_CLEANED=1
    # the dialog stand-in
    if [ -f "$PE_RUN_DIR/sdl_display.pid" ]; then
        kill "$(cat "$PE_RUN_DIR/sdl_display.pid")" 2>/dev/null
    fi
    # e: the flag back to what it was (1 = the console may stand by again; a 2 left by a killed run is not "before")
    if [ -f "$PE_POWER_FLAG" ]; then
        case "$PE_POWER_OLD" in
            0 | 1) printf '%s' "$PE_POWER_OLD" > "$PE_POWER_FLAG" 2>/dev/null ;;
            *) printf 1 > "$PE_POWER_FLAG" 2>/dev/null ;;
        esac
    fi
    # b
    if [ -L "$PE_LAUNCHTMP" ]; then
        rm -f "$PE_LAUNCHTMP"
    fi
    rm -f "$PE_VOLATILE/project_eris.cfg" /tmp/launchfilecommand 2>/dev/null
    for pe_r in $PE_REMAP_BOUND; do
        umount "$AB_APP_DIR/$pe_r" 2>/dev/null || umount -l "$AB_APP_DIR/$pe_r" 2>/dev/null
    done
    # d: the App's folder first, then the tree; the tree is only deleted once nothing of the stick is mounted in it
    # (rm -rf through a bind mount would reach the App's own files)
    if [ -n "$PE_APP_BOUND" ]; then
        umount "$PE_MOUNT_POINT/etc/project_eris/SUP/launchers/$PE_FILENAME" 2>/dev/null ||
            umount -l "$PE_MOUNT_POINT/etc/project_eris/SUP/launchers/$PE_FILENAME" 2>/dev/null
    fi
    if [ -n "$PE_MOUNTED" ]; then
        umount "$PE_MOUNT_POINT" 2>/dev/null || umount -l "$PE_MOUNT_POINT" 2>/dev/null
    fi
    if pe_mounted "$PE_MOUNT_POINT" || pe_tree_mounted; then
        pe_log "$PE_MOUNT_POINT is still mounted - the RAM tree $PE_TREE is left alone"
    else
        rm -rf "$PE_TREE" 2>/dev/null
    fi
}
