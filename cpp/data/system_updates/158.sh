#!/bin/bash

if [[ $TUEFIND_FLAVOUR == "krimdok" ]]; then
    root_crontab=/var/spool/cron/crontabs/root

    if [ ! -f ${root_crontab} ]; then
        echo "${root_crontab} does not seem to exist - Skipping"
        exit 0
    fi

    if [ ! -r ${root_crontab} ]; then
        echo "${root_crontab} not readable - Aborting"
        exit 1
    fi

    cron_job='10 22 * * * php "$VUFIND_HOME/public/index.php" scheduledsearch notify > "$LOG_DIR/scheduledsearch_notify.log" 2>&1'
    if crontab -l | grep -Fxq "$cron_job"; then
        echo "Cron job already exists - Skipping"
    else
        (crontab -l; echo "$cron_job") | crontab -
        echo "Cron job added successfully"
    fi
fi