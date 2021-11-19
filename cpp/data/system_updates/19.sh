#!/bin/bash
set -o errexit

CIFS_KEEP_ALIVE="\$BIN/cifs_keepalive.sh"

if [ -e /var/spool/cron/root ]; then # CentOS
        root_crontab=/var/spool/cron/root
    elif [ -e /var/spool/cron/crontabs/root ]; then # Ubuntu
        root_crontab=/var/spool/cron/crontabs/root
    else
        exit 0 # There is no crontab for root.
fi

cp /usr/local/ub_tools/cpp/cifs_keepalive.sh /usr/local/bin

grep --quiet --fixed-strings ${CIFS_KEEP_ALIVE} ${root_crontab} || echo "0 * * * * \"${CIFS_KEEP_ALIVE}\"" >> ${root_crontab}
