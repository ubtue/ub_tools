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


#marc_grep "$marc_title_input" 'leader[7]="s" if "001"==".*" extract "001"' \
# Look at leader byte 7 to see whether we are "serial" and then
# determine whether we are a serial in the strict sense, i.e. no
# Newspaper (=N) or Journal (=P)
marc_grep "$marc_title_input" 'leader[7]="s" if "008"=="(?i)^.{21}[^NP].*" extract "001"' \
    | cut --delimiter=":" --fields=3 \
    > "$serial_ppns"

