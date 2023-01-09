#!/bin/bash
set -o errexit

if [[ $TUEFIND_FLAVOUR == "ixtheo" ]]; then
    if [ -e /var/spool/cron/crontabs/root ]; then
        sed --in-place 's%"$BIN/rss_aggregator" %"$BIN/rss_aggregator" "--use-web-proxy" %g' /var/spool/cron/crontabs/root
    fi
fi
