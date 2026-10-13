#!/bin/sh
# One-time build (AUTOBLEEM-29): static armhf fsck.fat (dosfstools + the progress patch), fsck.exfat (exfatprogs)
# and e2fsck (e2fsprogs) for the PSC, with the build image's PSC toolchain. Run in the image with /w mounted:
#   /w/src/<the three tarballs>  /w/dosfstools-4.2-ab-progress.patch  ->  /w/out/
set -eu
W=/w
B=$W/build
O=$W/out
T=/opt/psc/bin/armv8-sony-linux-gnueabihf
export CC=$T-gcc AR=$T-ar RANLIB=$T-ranlib STRIP=$T-strip
HOST=arm-linux-gnueabihf
rm -rf "$B" "$O"
mkdir -p "$B" "$O/licenses"

echo "== dosfstools"
tar xzf $W/src/dosfstools-4.2.tar.gz -C "$B"
cd "$B/dosfstools-4.2"
patch -p1 < $W/dosfstools-4.2-ab-progress.patch
./configure --host=$HOST --without-udev CFLAGS=-O2 LDFLAGS=-static > "$B/dosfstools.configure.log" 2>&1
make -j4 -C src fsck.fat > "$B/dosfstools.make.log" 2>&1
cp src/fsck.fat "$O/fsck.fat"
cp COPYING "$O/licenses/dosfstools-COPYING"

echo "== exfatprogs"
tar xzf $W/src/exfatprogs-1.4.3.tar.gz -C "$B"
cd "$B/exfatprogs-1.4.3"
./configure --host=$HOST CFLAGS=-O2 LDFLAGS=-static > "$B/exfatprogs.configure.log" 2>&1
# only the library and fsck: mkfs/tune want libblkid, which the sysroot has as a shared library only
# (libtool drops a plain -static at the link: -all-static)
{ make -j4 -C lib && make -j4 -C fsck LDFLAGS=-all-static; } > "$B/exfatprogs.make.log" 2>&1
cp fsck/fsck.exfat "$O/fsck.exfat"
cp COPYING "$O/licenses/exfatprogs-COPYING"

echo "== e2fsprogs"
tar xJf $W/src/e2fsprogs-1.47.4.tar.xz -C "$B"
cd "$B/e2fsprogs-1.47.4"
./configure --host=$HOST CFLAGS=-O2 LDFLAGS=-static --disable-nls --disable-fuse2fs --disable-uuidd \
    --disable-defrag --disable-imager --disable-resizer --disable-debugfs --disable-fsck \
    --disable-e2initrd-helper --disable-elf-shlibs --enable-libuuid --enable-libblkid \
    --without-libarchive > "$B/e2fsprogs.configure.log" 2>&1
make -j4 libs > "$B/e2fsprogs.libs.log" 2>&1
make -j4 -C e2fsck e2fsck > "$B/e2fsprogs.e2fsck.log" 2>&1
cp e2fsck/e2fsck "$O/e2fsck"
cp NOTICE "$O/licenses/e2fsprogs-NOTICE"

$STRIP "$O/fsck.fat" "$O/fsck.exfat" "$O/e2fsck"
cd "$O"
for t in fsck.fat fsck.exfat e2fsck; do
    file $t | grep -q 'statically linked' || { echo "$t is not static" >&2; exit 1; }
done
file fsck.fat fsck.exfat e2fsck
ls -l fsck.fat fsck.exfat e2fsck
sha256sum fsck.fat fsck.exfat e2fsck
