#!/bin/bash
set -o errexit -o nounset

if [ $# != 0 ]; then
    echo "Usage: $0"
    exit 1
fi

declare -r NACJD_INPUT="nacjd.json"
declare -r NACJD_OUTPUT="nacjd_ouput.xml"
declare -r ISSN_FILE="req_issn.txt"
declare -r MARC_FILE_K10PLUS="marc_from_k10plus.mrc"
declare -r WORKING_DIR="/usr/local/ub_tools/cpp/harvest/"
declare -r NACJD_TOOL="$WORKING_DIR/nacjd_data_publication"
declare -r ISSN_LOOKUP_K10_PLUS_TOOL="$WORKING_DIR/issn_lookup.py"


echo "Extracting ISSN"
cat "$NACJD_INPUT" | jq -r '.searchResults.response.docs[].ISSN' | sort | uniq > "$ISSN_FILE"

echo "Downloading MARC from K10Plus"
$ISSN_LOOKUP_K10_PLUS_TOOL "$ISSN_FILE" "$MARC_FILE_K10PLUS"

echo "Augmenting MARC using info from K10Plus"
$NACJD_TOOL "--verbose" $NACJD_INPUT $MARC_FILE_K10PLUS $NACJD_OUTPUT 