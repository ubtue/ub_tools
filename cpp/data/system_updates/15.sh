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
    #replace quotation due to error in 12.sh
    sed --in-place --regexp-extended 's/("\$BIN\/generate_new_journal_alert_stats) (ixtheo|relbib) (days_in_last_month")/\1" "\2" "\3 "$EMAIL"/g' $root_crontab
fi
