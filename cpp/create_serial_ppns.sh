#!/bin/bash
set -o errexit -o nounset

# Creates a list of serials for which reasonable year information for sorting
# has to be generated

if [ $# -ne 2 ]; then
    echo "usage $0 marc_title_input serial_ppns_output"
    exit 1;
fi

marc_title_input="$1"
serial_ppns="$2"


# We look at field 008, positions 7-10 and use a non capturing group (?:) 
# and a negative lookahead (?!) to extract all records that do not
# contain a reasonable (i.e. for digit entry) at these positions
marc_grep "$marc_title_input" 'if "008"=="^.{7}(?:(?!\\d{4})).*" extract "001"' \
    | cut --delimiter=":" --fields=3 \
    > "$serial_ppns"
