#!/bin/bash
set -o errexit -o nounset

if [[ $# != 0 ]]; then
    echo "Usage: $0"
    exit 1
fi

if [[ -f /etc/redhat-release ]]; then
    systemctl stop incrond
elif [[ -f /etc/debian_version ]]; then
    systemctl stop incron
else
    echo "Could not detect Linux distribution"
    exit 1
fi

CRONTAB_BAK="/usr/local/var/tmp/crontab.bak"
crontab ${CRONTAB_BAK}
