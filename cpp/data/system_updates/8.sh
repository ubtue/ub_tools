#!/bin/bash
# We explicitly do NOT use -o nounset here, because $TUEFIND_FLAVOUR will not exist
# on systems where only ub_tools is installed, e.g. for zotaut.
set -o errexit -o pipefail

cd /usr/local/ub_tools/cpp

if [[ "$TUEFIND_FLAVOUR" == "ixtheo" ]]; then
    make import_rss_aggregator_conf
    ./import_rss_aggregator_conf relbib /usr/local/var/lib/tuelib/rss_aggregator_relbib.conf
fi

make convert_rss_db_tables
./convert_rss_db_tables
