#!/bin/bash

mount -o remount,rw /data

echo Installing AutoBleem payload

rm -rf /data/autobleem
mkdir -p /data/autobleem/rootfs
mkdir -p /data/autobleem/workdir
tar -xvzf /media/Apps/abflashkit/kernel/abrootfs.tgz -C /data/autobleem/rootfs

echo Done 