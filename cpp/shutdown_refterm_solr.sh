#!/bin/bash
export SOLR_BIN=/usr/local/vufind/solr/vendor/bin/
export SOLRMARC_HOME=/mnt/zram/import
export SOLR_HOME=/mnt/zram/solr/vufind/
export SOLR_PORT=8081

RAMDISK_PATH=/mnt/zram

Usage() {
    cat << EOF
    Shuts down the temporary Solr instance used to extract reference data"
USAGE: ${0##*/} 
EOF
}



if [[ "$EUID" -ne 0 ]]; then
    echo "We can only run as root" 2>&1
    exit 1
fi

if [[ "$#" -ne 0 ]]; then
    echo "Invalid parameter specified"
    usage
    exit 1
fi


if mount | grep "$RAMDISK_PATH"; then
    "$RAMDISK_PATH"/solr.sh stop
    umount "$RAMDISK_PATH"
    rmmod zram
else
   echo "$RAMDISK_PATH" "not mounted - nothing to do"
fi
