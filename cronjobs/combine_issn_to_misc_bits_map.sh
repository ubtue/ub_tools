#!/bin/bash

# This script takes map files from remote directory
# and combines them to a local file.
REMOTE_MAPS_DIR="/mnt/ZE020150/FID-Entwicklung/issn_to_misc_bits"
LOCAL_MAPS_FILE="/usr/local/var/lib/tuelib/issn_to_misc_bits.map"

cat $REMOTE_MAPS_DIR/*.map > $LOCAL_MAPS_FILE
