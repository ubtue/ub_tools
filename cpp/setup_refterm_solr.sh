#!/bin/bash
set -o errexit -o nounset

VUFIND_HOME=/usr/local/vufind2
STANDARD_VUFIND_SOLRMARC_HOME=$VUFIND_HOME/import/
UB_TOOLS_HOME=/usr/local/ub_tools/
CONFIG_FILE_DIR=$UB_TOOLS_HOME/cpp/data/refterm_solr_conf
RAMDISK_DIR=/mnt/zram
SOLR_START_SCRIPT=solr.sh
LOGDIR=$RAMDISK_DIR/log/ 

export SOLR_BIN=/usr/local/vufind2/solr/vendor/bin/
export SOLRMARC_HOME=/mnt/zram/import
export SOLR_HOME=/mnt/zram/solr/vufind/
export SOLR_PORT=8081

usage() {
    cat << EOF
Sets up a a temporary Solr instance in a Ramdisk and imports the MARC21 data given as a parameter

USAGE: "${0##*/}" FILE_TO_IMPORT
EOF
}

#Abort if we are not root or the parameters do not match

if [[ "$EUID" -ne 0 ]]; then
    echo "We can only run as root" 2>&1
    exit 1
fi

if [[ "$#" -ne 1 ]]; then
    echo "Invalid number of parameters"
    usage
    exit 1
fi

FILE_TO_IMPORT="$1"

# Test whether zram is already active
if ! modprobe --dry-run --first-time zram 2>/dev/null ; then
   echo "Zram module is already loaded"
   echo "We currently cannot cope with another active zram"
   exit 1
fi   

#Setup RAMDISK
modprobe zram
sleep 1
echo 4G > /sys/block/zram0/disksize
mke2fs -q -m 0 -b 4096 -O sparse_super -L zram /dev/zram0

# Create mount directory if it does not exist
mkdir -p "$RAMDISK_DIR"
mkdir -p "$LOGDIR"

mount -o relatime,nosuid /dev/zram0 "$RAMDISK_DIR"

# Copy the config directory to zram
rsync --archive --recursive "$CONFIG_FILE_DIR/" "$RAMDISK_DIR"
# Copy jars to zram
rsync --archive --include='*.jar'  --exclude='*' "$STANDARD_VUFIND_SOLRMARC_HOME" "$RAMDISK_DIR/import"
rsync --archive "$STANDARD_VUFIND_SOLRMARC_HOME"/lib  "$RAMDISK_DIR"/import
rsync --archive "$STANDARD_VUFIND_SOLRMARC_HOME"/bin  "$RAMDISK_DIR"/import
rsync --archive "$STANDARD_VUFIND_SOLRMARC_HOME"/lib_local "$RAMDISK_DIR"/import
rsync --archive "$STANDARD_VUFIND_SOLRMARC_HOME"/index_java "$RAMDISK_DIR"/import

#Setup and start Solr
"$RAMDISK_DIR"/"$SOLR_START_SCRIPT" start

#Import the files
"$RAMDISK_DIR"/import-marc.sh -p "$RAMDISK_DIR"/import/import.properties "$1" 2>&1 
