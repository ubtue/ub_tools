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
    sed --in-place 's/30 20 \* \* \* cd "$BSZ_DATEN" \&\& "$BIN\/generate_beacon_file.py" "--filter-field=REL" "$EMAIL" "\/usr\/local\/ub_tools\/cpp\/data\/relbib-beacon.header" "$VUFIND_HOME\/public\/relbib_docs\/relbib-beacon.txt" > "$LOG_DIR\/generate_beacon_file_relbib.log" 2>\&1/15 20 * * * cd "$BSZ_DATEN" \&\& "$BIN\/generate_beacon_file.py" "--filter-field=REL" "$EMAIL" "\/usr\/local\/ub_tools\/cpp\/data\/relbib-beacon.header" "$VUFIND_HOME\/public\/relbib_docs\/relbib-beacon.txt" > "$LOG_DIR\/generate_beacon_file_relbib.log" 2>\&1\n30 20 * * * cd "$BSZ_DATEN" \&\& "$BIN\/generate_beacon_file.py" "--filter-field=BIB" "$EMAIL" "\/usr\/local\/ub_tools\/cpp\/data\/bibstudies-beacon.header" "$VUFIND_HOME\/public\/docs\/bibstudies-beacon.txt" > "$LOG_DIR\/generate_beacon_file_bibstudies.log" 2>\&1\n45 20 * * * cd "$BSZ_DATEN" \&\& "$BIN\/generate_beacon_file.py" "--filter-field=CAN" "$EMAIL" "\/usr\/local\/ub_tools\/cpp\/data\/canonlaw-beacon.header" "$VUFIND_HOME\/public\/docs\/canonlaw-beacon.txt" > "$LOG_DIR\/generate_beacon_file_canonlaw.log" 2>\&1/g' $root_crontab
fi

if [[ $TUEFIND_FLAVOUR == "krimdok" ]]; then
    if [ -e /var/spool/cron/root ]; then # CentOS
        root_crontab=/var/spool/cron/root
    elif [ -e /var/spool/cron/crontabs/root ]; then # Ubuntu
        root_crontab=/var/spool/cron/crontabs/root
    else
        exit 0 # There is no crontab for root.
    fi
    sed --in-place 's/"$LOG_DIR\/full_text_stats.log" 2>\&1/"$LOG_DIR\/full_text_stats.log" 2>\&1\n30 20 * * * cd "$BSZ_DATEN" \&\& "$BIN\/generate_beacon_file.py" "$EMAIL" "\/usr\/local\/ub_tools\/cpp\/data\/krimdok-beacon.header" "$VUFIND_HOME\/public\/docs\/krimdok-beacon.txt" > "$LOG_DIR\/generate_beacon_file_krimdok.log" 2>\&1/g' $root_crontab
fi
