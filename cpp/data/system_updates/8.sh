#!/bin/bash
set -o errexit -o nounset -o pipefail
cd /usr/local/ub_tools/cpp
make convert_rss_db_tables
./convert_rss_db_tables
