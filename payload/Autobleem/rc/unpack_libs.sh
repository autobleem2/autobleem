#!/bin/sh

# The shared libraries (our SDL2 family) from Autobleem/lib/libs.tar.gz into /tmp/lib - RAM. Run at the top of
# boot.sh (absplash needs them for the boot picture) and again by autobleem.sh, which finds them unpacked and
# leaves them alone: unpacking over the libraries a running absplash has mapped could crash it. A libs.tar.gz
# that changed (the stick updated) is unpacked again.
A=/media/Autobleem/lib/libs.tar.gz
[ -f "$A" ] || exit 1
if [ -f /tmp/lib/.unpacked ] && cmp -s "$A" /tmp/lib/libs.tar.gz; then
    exit 0
fi
mkdir -p /tmp/lib
cp -f "$A" /tmp/lib/libs.tar.gz
cd /tmp/lib || exit 1
rm -f .unpacked
gunzip -c libs.tar.gz | tar xf - && touch .unpacked   # busybox tar may lack -z (the new kernel payload's does)
