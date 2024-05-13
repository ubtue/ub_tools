#!/bin/bash
set -o errexit -o nounset

if [ $# != 0 ]; then
    echo "Usage: $0"
    exit 1
fi

#krimdok_nacjd_2220801_001.xml
declare -r BSZ_FILENAME="krimdok_nacjd_$(date +%y%m%d)_001"
declare -r DOWNLOAD_DIR=/tmp/NACJD/$(date +%Y%m%d_%H%M%S)
declare -r DOWNLOAD_FILE="$DOWNLOAD_DIR/$BSZ_FILENAME.json"
declare -r CONVERT_FILE="$DOWNLOAD_DIR/$BSZ_FILENAME.xml"
declare -r BSZ_DIR=/usr/local/ub_tools/bsz_daten
mkdir -p "$DOWNLOAD_DIR"

# Use empty MRC file as dummy input.
# On consecutive downloads we want to use the real KrimDok GesamtTiteldaten... file here.
declare -r GESAMTTITLE_DATEN = "$BSZ_DIR/GesamtTitelDaten-$(date +%y%m%d).mrc"
# touch "$EMPTY_MRC_FILE"

echo "Downloading data"
nacjd get_full "$GESAMTTITLE_DATEN" "$DOWNLOAD_FILE"

echo "Generating statistics (optional)"
nacjd get_statistics "$DOWNLOAD_FILE"

echo "Converting to MRC"
nacjd convert_json_to_marc "$DOWNLOAD_FILE" "$CONVERT_FILE"

#echo "Upload to BSZ"
#upload_to_bsz_ftp_server.py "$CONVERT_FILE" /2001/Default_Test/input/
