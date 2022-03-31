#!/bin/bash
set -o errexit -o nounset

ZRAM_DISK_SIZE=8589934592

# Call with a single error_msg argument.
function ErrorExit() {
	    >&2 echo "$0: $1"
	        exit 1
}

function SetupRamdisk() {
    # Nothing to do?
    test -e /mnt/zram && mountpoint --quiet /mnt/zram && return

    #
    echo "Not already present...Setting up"

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
    echo "Successfully mounted /mnt/zram"
}


function ShutdownRamdisk() {
    test -e /mnt/zram && mountpoint --quiet /mnt/zram || return
    umount /mnt/zram
    rmmod zram
}


echo -n "Setting up RAMDISK..."
SetupRamdisk
sleep 3
echo "Done"


for archive in $(ls od-full_bsz-tit_[0-9][0-9][0-9].xml.gz)
do
	echo "Convert ${archive}"
	../clean_and_convert.sh ${archive}
done

ShutdownRamdisk
