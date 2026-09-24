#!/bin/bash

# Update Database on the fly
# Extract system files to avoid crashing
mkdir -p /tmp/lib
cp /media/Autobleem/lib/libs.tar.gz /tmp/lib
cd /tmp/lib
gunzip -c libs.tar.gz | tar xvf -   # busybox tar may lack -z (the new kernel payload's does)

cd /media/Autobleem/bin/autobleem
./run.sh
sync


