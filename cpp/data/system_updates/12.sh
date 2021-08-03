#!/bin/bash
set -o errexit


if [[ $TUEFIND_FLAVOUR == "ixtheo" ]]; then
    if [ -e /var/spool/cron/root ]; then # CentOS
        root_crontab=/var/spool/cron/root
    elif [ -e /var/spool/cron/crontabs/root ]; then # Ubuntu
        root_crontab=/var/spool/cron/crontabs/root
    else
        exit 0 # There is no crontab for root.
    fi

    sed --in-place '/purge_old_data.py/a 30 0 * * * "$BIN/generate_new_journal_alert_stats ixtheo days_in_last_month"\n30 0 * * * "$BIN/generate_new_journal_alert_stats relbib days_in_last_month"' $root_crontab
fi
