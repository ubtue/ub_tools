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
    sed --in-place 's/"$LOG_DIR\/rss_aggregator_relbib.log" 2>\&1/"$LOG_DIR\/rss_aggregator_relbib.log" 2>\&1\n20 23 * * * "$BIN\/rss_aggregator" "ixtheo" "$EMAIL" "\/usr\/local\/vufind\/public\/docs\/news.rss" > "$LOG_DIR\/rss_aggregator_ixtheo.log" 2>\&1/g' $root_crontab
    sed --in-place 's/"$LOG_DIR\/rss_subset_aggregator_relbib.log" 2>\&1/"$LOG_DIR\/rss_subset_aggregator_relbib.log" 2>\&1\n50 23 * * * "$BIN\/rss_subset_aggregator" "--mode=email" "$EMAIL" "ixtheo" > "$LOG_DIR\/rss_subset_aggregator_ixtheo.log" 2>\&1/g' $root_crontab
fi
