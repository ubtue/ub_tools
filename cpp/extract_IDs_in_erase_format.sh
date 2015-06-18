#!/bin/bash
#
# This is a tool to extract IDs from MARC-21 data sets and append them in a format used
# by the BSZ to one of their erasure files.
set -o errexit -o nounset

if [ $# != 2 ]; then
    echo "usage: $0 marc_input_file append_list"
    exit 1
fi

now=$(date +%y%j%H%M%S)
marc_grep2 "$1" '"001"' no_label 2>/dev/null | \
while read ID; do
    echo "${now}A${ID}" >> "$2"
done

