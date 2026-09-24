#!/bin/bash
# PROOF (proof/plugin branch): run inside the autobleem-build image from the tree's root.
#   1. native Linux x86_64 dev build: dlopen the plugin, which logs through the executable's plog and
#      calls the executable's Env - unpacked, then UPX-packed
#   2. the console build (gcc-6, glibc 2.24): the plugin links; every symbol it leaves undefined is one the
#      executable exports or one of the libraries both load (libc, libstdc++, libm, libgcc_s)
set -u
export AB_GIT_HASH=proof AB_GIT_BRANCH=proof/plugin AB_GIT_VERSION=v2.0.0-alpha2 AB_GIT_DIRTY=true

echo "=== native: configure + build"
cmake -S . -B build_proof_lin -G Ninja -DAB_TARGET=dev -DAB_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release >/dev/null || exit 1
ninja -C build_proof_lin autobleem-gui ab_proof_plugin >/dev/null || exit 1
EXE=build_proof_lin/autobleem-gui
PLUGIN=$PWD/build_proof_lin/libab_proof_plugin.so
ls -la "$EXE" "$PLUGIN"
echo "exported dynamic symbols: $(readelf --dyn-syms -W "$EXE" | grep -c ' DEFAULT ')"

echo "=== native: unpacked"
AB_PLUGIN_PROOF_PING="$PLUGIN" "$EXE"
echo "native exit $?"

echo "=== native: UPX-packed"
cp "$EXE" /tmp/ab-proof-upx && upx -q --best --lzma /tmp/ab-proof-upx >/dev/null && ls -la /tmp/ab-proof-upx
AB_PLUGIN_PROOF_PING="$PLUGIN" /tmp/ab-proof-upx
echo "upx exit $?"

echo "=== psc: configure + build"
cmake -S . -B build_proof_psc -G Ninja -DCMAKE_BUILD_TYPE=Release -DAB_BUILD_TESTS=OFF \
    -DCMAKE_TOOLCHAIN_FILE=toolchains/psc/PSCtoolchainV8.cmake -DAB_PSC_TOOLCHAIN=/opt/psc >/dev/null || exit 1
ninja -C build_proof_psc autobleem-gui ab_proof_plugin >/dev/null || exit 1
PEXE=build_proof_psc/autobleem-gui
PPLUG=build_proof_psc/libab_proof_plugin.so
ls -la "$PEXE" "$PPLUG"
READELF=$(ls /opt/psc/bin/*-readelf 2>/dev/null | head -1)
[ -n "$READELF" ] || READELF=readelf
echo "psc exported dynamic symbols: $("$READELF" --dyn-syms -W "$PEXE" | grep -c ' DEFAULT ')"
"$READELF" --dyn-syms -W "$PEXE" | awk '$7 != "UND" && $5 != "LOCAL" {print $8}' | sed 's/@.*//' | sort -u > /tmp/exe_defined
"$READELF" --dyn-syms -W "$PPLUG" | awk '$7 == "UND" {print $8}' | sed 's/@.*//' | grep -v '^$' | sort -u > /tmp/plug_undef
LIBDEF=/tmp/lib_defined; : > $LIBDEF
for lib in libc.so.6 libstdc++.so.6 libm.so.6 libgcc_s.so.1; do
    f=$(find /opt/psc/sysroot /opt/psc/gcc-6 -name "$lib" 2>/dev/null | head -1)
    [ -n "$f" ] && "$READELF" --dyn-syms -W "$f" | awk '$7 != "UND" {print $8}' | sed 's/@.*//' >> $LIBDEF
done
sort -u -o $LIBDEF $LIBDEF
echo "plugin undefined: $(wc -l < /tmp/plug_undef); resolved by the executable: $(comm -12 /tmp/plug_undef /tmp/exe_defined | wc -l)"
comm -23 /tmp/plug_undef /tmp/exe_defined > /tmp/not_in_exe
comm -23 /tmp/not_in_exe $LIBDEF > /tmp/unresolved
echo "left to the system libraries: $(wc -l < /tmp/not_in_exe); resolved by none: $(wc -l < /tmp/unresolved)"
head -20 /tmp/unresolved
echo "psc: the plugin's NEEDED: $("$READELF" -d "$PPLUG" | grep NEEDED | tr -s ' ' | cut -d' ' -f6 | tr '\n' ' ')"
echo "psc: glibc/glibcxx needs:"
bash tools/check_psc_binary.sh "$PPLUG" /opt/psc || true
