#!/bin/bash
# Enable incron based triggering of pipeline scripts
set -o errexit -o nounset

if [[ $# != 0 ]]; then
    echo "Usage: $0"
    exit 1
fi

CRONTAB_BAK="/usr/local/var/tmp/crontab.bak"

crontab -l > ${CRONTAB_BAK} 2>/dev/null

if [[ -f /etc/redhat-release ]]; then
    systemctl start incrond
elif [[ -f /etc/debian_version ]]; then
    systemctl start incron
else
    echo "Could not detect Linux distribution"
    exit 1
fi




