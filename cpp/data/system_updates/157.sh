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

    sed --in-place 's|20 23 \* \* \* "\$BIN/rss_aggregator" --use-web-proxy "ixtheo" "\$EMAIL" "/usr/local/vufind/public/docs/news.rss" > "\$LOG_DIR/rss_aggregator_ixtheo.log" 2>\&1|25 23 * * * "$BIN/rss_aggregator" "ixtheo" "$EMAIL" "/usr/local/vufind/public/docs/news.rss" > "$LOG_DIR/rss_aggregator_ixtheo.log" 2>\&1|' $root_crontab
fi