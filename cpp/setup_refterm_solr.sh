#!/bin/bash
set -o errexit -o nounset

VUFIND_HOME=/usr/local/vufind2
STANDARD_VUFIND_SOLRMARC_HOME=$VUFIND_HOME/import/
UB_TOOLS_HOME=/usr/local/ub_tools/
CONFIG_FILE_DIR=$UB_TOOLS_HOME/cpp/data/refterm_solr_conf
RAMDISK_DIR=/mnt/zram
SOLR_START_SCRIPT=solr.sh
LOGDIR=$RAMDISK_DIR/log/ 
ZRAM_DISK_SIZE=2G
ZRAM_DISK_SIZE_CONTROL_FILE=/sys/block/zram0/disksize

export SOLR_BIN=/usr/local/vufind2/solr/vendor/bin/
export SOLRMARC_HOME=/mnt/zram/import
export SOLR_HOME=/mnt/zram/solr/vufind/
export SOLR_PORT=8081

trap ExitHandler SIGINT

usage() {
    cat << EOF
Sets up a a temporary Solr instance in a Ramdisk and imports the MARC21 data given as a parameter

USAGE: "${0##*/}" FILE_TO_IMPORT
EOF
}


function ExitHandler {
   shutdown_ramdisk
}


setup_ramdisk() {
    # Test whether zram is already active
    if ! modprobe --dry-run --first-time zram 2>/dev/null ; then
       echo "Zram module is already loaded"
       echo "We currently cannot cope with another active zram"
       exit 1
    fi

    #Setup RAMDISK
    modprobe zram
    sleep 1
    set_ramdisk_size
    mke2fs -q -m 0 -b 4096 -O sparse_super -L zram /dev/zram0
}


set_ramdisk_size() {
   echo $ZRAM_DISK_SIZE > $ZRAM_DISK_SIZE_CONTROL_FILE
   if [ $(cat $ZRAM_DISK_SIZE_CONTROL_FILE) == 0 ] ; then
       for  i in $(seq 1 5); do
          echo $ZRAM_DISK_SIZE > $ZRAM_DISK_SIZE_CONTROL_FILE
          sleep 5
          if [ $(cat $ZRAM_DISK_SIZE_CONTROL_FILE) != 0 ] ; then
              return 0
          fi
       done
   else
       return 0
   fi
   echo "Could not successfully set ramdisk size"
   shutdown_ramdisk
   exit 1
}


shutdown_ramdisk() {
# Centos dows not support wait option for rmmod, so...
sleep 1
rmmod zram
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

# Setup zram
setup_ramdisk
# Create mount directory if it does not exist
mkdir -p "$RAMDISK_DIR"
mount -o relatime,nosuid /dev/zram0 "$RAMDISK_DIR"
mkdir -p "$LOGDIR"

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
