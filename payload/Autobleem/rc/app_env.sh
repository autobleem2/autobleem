#!/bin/sh

# Sourced by an App's run.sh (Apps/<name>/run.sh) before it starts its program: the libraries the apps
# need beyond the console's firmware - SDL2_image/mixer/ttf, freetype, png, vorbis, ... (the site's
# libs pack, unpacked by the installer into Autobleem/lib/apps) - linked into /tmp/applib on first use,
# with the shorter soname links the loader looks for (the stick is FAT, it cannot hold a symlink), and
# put on the library path. Was RetroBoot's init_libs.sh / RB_LIBRARY_PATH.

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
