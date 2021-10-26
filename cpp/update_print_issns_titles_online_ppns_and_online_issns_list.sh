#!/bin/bash
set -o nounset -o errexit


readonly OUTPUT_FILE=print_issns_titles_online_ppns_and_online_issns.csv
extract_zeder_data /tmp/"$OUTPUT_FILE" ixtheo issn tit eppn essn
mv "$OUTPUT_FILE" /usr/local/var/lib/tuelib/
