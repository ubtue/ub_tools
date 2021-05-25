#!/bin/bash
set -o errexit


if [[ $(hostname) == "ub16.uni-tuebingen.de" ]]; then
    if [ -e /var/spool/cron/root ]; then # CentOS
        root_crontab=/var/spool/cron/root
    else # Ubuntu
        root_crontab=/var/spool/cron/crontabs/root
    fi

    # Make sure our crontab ends with a newline:
    readonly $newline=$(echo -e \\n)
    if [[ $(tail --bytes=1 $root_crontab) != $newline ]]; then
        echo $newline >> $root_crontab
    fi
    
    echo '0 0 1 * * "$BIN/cologne.sh' >> $root_crontab
fi
