#!/bin/bash
set -o errexit -o nounset -o pipefil

if [ -e /home/email_processor/.fetchmailrc ]; then
    echo 'It looks as if the email_processor account has already been set up!'
    exit 0
fi

if [[ $(whoami) != "root" ]]; then
    echo 'You must run '"$0"' as root!'
    exit 1
fi

if [ ! $(which fetchmail) ]; then
    echo 'You must first install fetchmail before running this script!'
    exit 2
fi

adduser email_processor email_processor
install --group=email_processor --owner=email_processor --mode=0600 /mnt/ZE020150/FID-Entwicklung/ub_tools/email_processor/.fetchmailrc /home/email_processor/
install --group=email_processor --owner=email_processor --mode=0600 /mnt/ZE020150/FID-Entwicklung/ub_tools/email_processor/.forward /home/email_processor/

# If on Ubuntu, make fetchmail a demon.  On CentOS that is the default via systemd.
DEFAULT_CONFIG=/etc/default/fetchmail
if [ -e /etc/debian_version ]; then # We're probably on Ubuntu.
    grep --quiet 'START_DAEMON=no' "$DEFAULT_CONFIG"
    if [ $? ]; then
        sed --in-place --expression='s/START_DAEMON=no/START_DAEMON=yes/' "$DEFAULT_CONFIG"
        fetchmail
    fi
fi
