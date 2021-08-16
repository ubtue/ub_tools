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
    #replace quotation so that all parameters are quoted, parameter ixtheo|relbib (param1) are quoted through '" "' at param0 and param2
    sed --in-place 's/generate_new_journal_alert_stats /generate_new_journal_alert_stats" "/g' $root_crontab
    sed --in-place 's/ days_in_last_month"/" "days_in_last_month" "$EMAIL"/g' $root_crontab
fi
