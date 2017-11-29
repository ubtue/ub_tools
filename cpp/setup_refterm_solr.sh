#!/bin/bash
set -o errexit -o nounset

export VUFIND_HOME="/usr/local/vufind"
export VUFIND_SOLRMARC_HOME="$VUFIND_HOME/import/"
export UB_TOOLS_HOME="/usr/local/ub_tools/"
export CONFIG_FILE_DIR="$UB_TOOLS_HOME/cpp/data/refterm_solr_conf"
export LOGDIR="/mnt/zram/solr/vufind/logs/"
ZRAM_DISK_SIZE=2147483648 # Has to be in bytes in oder to compare the set value.


export SOLR_BIN="$VUFIND_HOME"/solr/vendor/bin/
export SOLRMARC_HOME=/mnt/zram/import
export SOLR_HOME=/mnt/zram/solr/vufind/
export SOLR_PORT=8081
export IMPORT_PROPERTIES_FILE="$SOLRMARC_HOME"/import.properties

trap ExitHandler SIGINT

Usage() {
    cat << EOF
Sets up a a temporary Solr instance in a Ramdisk and imports the MARC21 data given as a parameter

USAGE: "${0##*/}" FILE_TO_IMPORT
EOF
}


function ExitHandler {
   shutdown_ramdisk
}
if [ $# -ne 1 ]; then
    >&2 echo -e "$0 : Invalid number of parameters\n"
    Usage
    exit 1
fi


# Call with a single error_msg argument.
ErrorExit() {
    >&2 echo "$0: $1"
    exit 1
}


SetupRamdisk() {
    # Nothing to do?
    test -e /mnt/zram && mountpoint --quiet /mnt/zram && return

    if ! modprobe zram num_devices=1; then
        ErrorExit 'Failed to load ZRAM module!'
    fi

    # Set the RAM disk size:
    if [[ $(cat /sys/block/zram0/disksize) != "$ZRAM_DISK_SIZE" ]]; then
        echo "$ZRAM_DISK_SIZE" > /sys/block/zram0/disksize
        if [[ $(cat /sys/block/zram0/disksize) != "$ZRAM_DISK_SIZE" ]]; then
            ErrorExit "Failed to set ZRAM disk size to $ZRAM_DISK_SIZE"'!'
        fi
    fi

    # Make sure the partition table is not garbled from a previous read
    if ! partprobe /dev/zram0; then
        ErrorExit 'Failed to reread partition table'
    fi

    # Create a file system in RAM...
    if ! mkfs.ext4 -q -m 0 -b 4096 -O sparse_super -L zram /dev/zram0; then
        ErrorExit 'Failed to create an Ext4 file system in RAM!'
    fi
    # ...and mount it.
    if ! mount -o relatime,nosuid /dev/zram0 /mnt/zram; then
        ErrorExit 'Failed to mount our RAM disk!'
    fi
}


GetSolrPingUrl() {
   echo $(cat $IMPORT_PROPERTIES_FILE | grep '^solr.hosturl' | sed -e 's/.*=\s*//' | \
          sed -e 's/update$/admin\/ping/')
}


IsSolrAvailable() {
   # Use the ping request handler like solrmarc
   # https://github.com/solrmarc/solrmarc/blob/master/src/org/solrmarc/solr/SolrCoreLoader.java
   local SOLR_PING_URL=$1
   for i in $(seq 1 5);
   do
      SOLR_UP=$(curl --get --silent "$SOLR_PING_URL" | grep '.*status.>OK<.*')
      if [[ ! -z $SOLR_UP ]]; then
          return 0 #true
      fi
      sleep 3
   done
   return 1 #false
}

FILE_TO_IMPORT="$1"

# Abort if we are not root or the parameters do not match
test "$EUID" -eq 0 || ErrorExit 'We can only run as root!'
if [[ $# != 1 ]]; then
    >&2 echo "$0: Invalid number of parameters"'!'
    Usage
    exit 1
fi

# Setup zram
SetupRamdisk
mkdir --parents "$LOGDIR"

# Copy the config directory to zram
rsync --archive --recursive "$CONFIG_FILE_DIR/" /mnt/zram
# Copy jars to zram
rsync --archive --include='*.jar' --exclude='*' "$VUFIND_SOLRMARC_HOME" /mnt/zram/import
rsync --archive "$VUFIND_SOLRMARC_HOME"/lib /mnt/zram/import
rsync --archive "$VUFIND_SOLRMARC_HOME"/bin /mnt/zram/import
rsync --archive "$VUFIND_SOLRMARC_HOME"/lib_local /mnt/zram/import
rsync --archive "$VUFIND_SOLRMARC_HOME"/index_java /mnt/zram/import

#Adjust access permissions for running as solr user
chown --recursive solr:solr "$SOLR_HOME"
chown solr:solr "$LOGDIR"

# Set up and start Solr
sudo --preserve-env --user solr /mnt/zram/solr.sh start
if [ $? -ne 0 ]; then
    >&2 echo "$0"': Failed to start Solr!'
    exit 1
fi

#Make sure Solr is up and running
if ! IsSolrAvailable $(GetSolrPingUrl); then
    >&2 echo "$0"': Failed to ping the Solr instance!'
    exit 1
else
    echo "Successfully pinged the Solr instance"
fi

# Import the MARC files
/mnt/zram/import-marc.sh -p "$IMPORT_PROPERTIES_FILE" "$FILE_TO_IMPORT" 2>&1
if [ $? -ne 0 ]; then
    >&2 echo "$0"': Failed to import MARC files!'
    exit 1
fi
