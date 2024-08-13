#!/bin/bash
set -o errexit

if [[ $TUEFIND_FLAVOUR == "ixtheo" ]]; then
    root_crontab=/var/spool/cron/crontabs/root

    if [ ! -f ${root_crontab} ]; then
        echo "${root_crontab} does not seem to exist - Skipping"
        exit 0
    fi

    if [ ! -r ${root_crontab} ]; then
        echo "${root_crontab} not readable - Aborting"
        exit 1
    fi

    line_of_entry_before=$(fgrep --line-number "tuefind_generate_sitemap.sh\" \"relbib\"" ${root_crontab} | awk -F':' '{print $1}')
    new_cronjob_bibstudies="15 18 * * 6 \"\$VUFIND_HOME/util/tuefind_generate_sitemap.sh\" \"bibstudies\""
    new_cronjob_churchlaw="30 18 * * 6 \"\$VUFIND_HOME/util/tuefind_generate_sitemap.sh\" \"churchlaw\""

    sed --in-place "${line_of_entry_before}"'a '"$new_cronjob_churchlaw" ${root_crontab}
    sed --in-place "${line_of_entry_before}"'a '"$new_cronjob_bibstudies" ${root_crontab}
fi
