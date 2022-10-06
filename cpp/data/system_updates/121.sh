#!/bin/bash
set -o errexit

# Make sure that the solr user is able to mount the ramdisk needed for speedup

if [[ $TUEFIND_FLAVOUR == "ixtheo" ]]; then
    cat > /etc/sudoers.d/99-alphabrowse_index_ramdisk <<EOF
# Allow the index script to create a RAMDISK
Cmnd_Alias RAMDISK_MOUNT = /bin/mount -t tmpfs -o size=10G tmpfs /tmp/ramdisk
Cmnd_Alias RAMDISK_UMOUNT = /bin/umount /tmp/ramdisk
solr ALL = NOPASSWD: RAMDISK_MOUNT,RAMDISK_UMOUNT
EOF
fi
