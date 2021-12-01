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
    sed --in-place --regexp-extended 's/30 22 \* \* \* ("\$BIN\/new_journal_alert" relbib .*)/15 22 \* \* \* \1\n30 22 * * * "\$BIN\/new_journal_alert" bibstudies bible.ixtheo.de "IxTheo Team<no-reply@ub.uni-tuebingen.de>" "BibStudies Subscriptions" > "\$LOG_DIR\/new_journal_alert_bibstudies.log" 2>\&1\n45 22 * * * "\$BIN\/new_journal_alert" churchlaw churchlaw.ixtheo.de "IxTheo Team<no-reply@ub.uni-tuebingen.de>" "ChurchLaw Subscriptions" > "\$LOG_DIR\/new_journal_alert_churchlaw.log" 2>\&1\n/g' $root_crontab
fi
