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
mkdir -p "$DOWNLOAD_DIR"

# Use empty MRC file as dummy input.
# On consecutive downloads we want to use the real KrimDok GesamtTiteldaten... file here.
declare -r EMPTY_MRC_FILE="$DOWNLOAD_DIR/empty.mrc"
touch "$EMPTY_MRC_FILE"

echo "Downloading data"
nacjd get_full "$EMPTY_MRC_FILE" "$DOWNLOAD_FILE"

echo "Generating statistics (optional)"
nacjd get_statistics "$DOWNLOAD_FILE"

echo "Converting to MRC"
nacjd convert_JSON_to_MARC "$DOWNLOAD_FILE" "$CONVERT_FILE"

#echo "Upload to BSZ"
#upload_to_bsz_ftp_server.py "$CONVERT_FILE" /pub/UBTuebingen_Default_Test/
