#!/bin/bash
set -o errexit -o nounset -o pipefail

cd /usr/local/ub_tools/cpp

if [[ "$TUEFIND_FLAVOUR" == "ixtheo" ]]; then
    make import_rss_aggregator_conf
    ./import_rss_aggregator_conf relbib /usr/local/var/lib/tuelib/rss_aggregator_relbib.conf
fi

make convert_rss_db_tables
./convert_rss_db_tables
