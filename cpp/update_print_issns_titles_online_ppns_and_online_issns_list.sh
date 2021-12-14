#!/bin/bash
set -o nounset -o errexit


readonly OUTPUT_FILE=journals.csv
extract_zeder_data /tmp/"$OUTPUT_FILE" ixtheo issn tit eppn essn
mv "$OUTPUT_FILE" /usr/local/var/lib/tuelib/
