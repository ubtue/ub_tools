#!/bin/bash
set -o errexit
cd /usr/local/ub_tools/cpp
make convert_rss_db_tables
./convert_rss_db_tables
mkdir --mode=755 /var/www/custom_rss_feeds
