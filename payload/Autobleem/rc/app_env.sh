#!/bin/sh

# Sourced by an App's run.sh (Apps/<name>/run.sh) before it starts its program: the libraries the apps
# need beyond the console's firmware - SDL2_image/mixer/ttf, freetype, png, vorbis, ... (the site's
# libs pack, unpacked by the installer into Autobleem/lib/apps) - linked into /tmp/applib on first use,
# with the shorter soname links the loader looks for (the stick is FAT, it cannot hold a symlink), and
# put on the library path. Was RetroBoot's init_libs.sh / RB_LIBRARY_PATH.
#
# It also gives the App a home on the stick and the virtual gamepad - see the two sections below.

APPLIB=/tmp/applib
APPLIB_SRC=/media/Autobleem/lib/apps

if [ ! -d "$APPLIB" ] && [ -d "$APPLIB_SRC" ]; then
	mkdir -p "$APPLIB"
	for lib in "$APPLIB_SRC"/*.so*; do
		[ -f "$lib" ] || continue
		name=$(basename "$lib")
		ln -sf "$lib" "$APPLIB/$name"
		# libfoo.so.1.2.3 -> libfoo.so.1.2, libfoo.so.1, libfoo.so
		short=$name
		while extn=$(echo "$short" | sed -n '/\.[0-9][0-9]*$/s/.*\(\.[0-9][0-9]*\)$/\1/p'); [ -n "$extn" ]; do
			short=$(basename "$short" "$extn")
			[ -e "$APPLIB/$short" ] || ln -sf "$lib" "$APPLIB/$short"
		done
	done
fi

export LD_LIBRARY_PATH=$APPLIB

AB_ROOT=/media
AB_APP_DIR=$(cd "$(dirname "$0")" && pwd)
export AB_ROOT AB_APP_DIR

# ---------------------------------------------------------------------------------------------
# A home on the stick.
#
# An App left alone writes its settings and saves wherever the distribution tells it to - Chocolate
# Doom announces "Using /root/.local/share/chocolate-doom/ for configuration and saves" - which on a
# console means writing to the machine's own storage. Nothing of AutoBleem's does that, and an App
# shall not either: it is not ours to write to, it is not backed up with the stick, and a user who
# takes the stick to another console would leave their saves behind.
#
# So $HOME is a folder on the stick, and the XDG variables under it, which between them cover what
# any Linux program looks at. Everything an App saves is then in Home/ where it can be found, copied
# and deleted.
# ---------------------------------------------------------------------------------------------
HOME="$AB_ROOT/Home"
XDG_DATA_HOME="$HOME/.local/share"
XDG_CONFIG_HOME="$HOME/.config"
XDG_CACHE_HOME="$HOME/.cache"
XDG_STATE_HOME="$HOME/.local/state"
export HOME XDG_DATA_HOME XDG_CONFIG_HOME XDG_CACHE_HOME XDG_STATE_HOME
mkdir -p "$XDG_DATA_HOME" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_STATE_HOME" 2>/dev/null

# ---------------------------------------------------------------------------------------------
# The virtual gamepad (docs/virtual-gamepad-plan.md).
#
# abpadd reads the pads through SDL's GameController API with our own gamecontrollerdb.txt - the same
# code and the same file the launcher uses, so a pad resolves in an App exactly as it does in the
# launcher - and libabpad.so, preloaded, shows the App a pad it understands whichever SDL API it
# reads. Neither is required: a stick without them runs its Apps as it always did.
#
# The daemon watches this shell's pid, so it goes when the App goes - including when the App is
# exec'd over this shell, which keeps the same pid, and including when the App crashes.
# ---------------------------------------------------------------------------------------------
AB_PAD_DIR="$AB_ROOT/Autobleem/bin/abpad"
if [ -x "$AB_PAD_DIR/abpadd" ] && [ -f "$AB_PAD_DIR/libabpad.so" ]; then
	"$AB_PAD_DIR/abpadd" --watch-pid $$ > /media/System/Logs/abpadd.log 2>&1 &

	# the daemon lets a pad settle before publishing, so wait rather than have the App ask too early
	ab_waited=0
	while [ ! -f /tmp/abpad.state ] && [ $ab_waited -lt 60 ]; do
		ab_waited=$((ab_waited + 1))
		sleep 0.1
	done

	AB_PAD_LOG=/media/System/Logs/abpad.log
	export AB_PAD_LOG
	export LD_PRELOAD="$AB_PAD_DIR/libabpad.so"

	[ -f "$AB_PAD_DIR/pad.default.ini" ] && export AB_PAD_DEFAULTS="$AB_PAD_DIR/pad.default.ini"
	[ -f "$AB_APP_DIR/pad.ini" ] && export AB_PAD_PROFILE="$AB_APP_DIR/pad.ini"

	# For an App the preload cannot reach - one statically linked against SDL - the mapping the daemon
	# actually resolved. Deliberately not our gamecontrollerdb.txt: a file given this way overrides
	# SDL's built-in table, and for a pad SDL already knows the built-in entry is the right one while
	# ours may be a stale line for another of that pad's modes.
	[ -f /tmp/abpad.state.mappings ] && export SDL_GAMECONTROLLERCONFIG_FILE=/tmp/abpad.state.mappings
fi
