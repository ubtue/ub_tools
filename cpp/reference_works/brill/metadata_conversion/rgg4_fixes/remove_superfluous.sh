#!/bin/bash

if [ $# != 3 ]; then
    echo "Usage: $0 multicandidates_file.txt superflous.txt cleaned_file.txt"
    exit 1
fi

infile="$1"
superfluous_regex="$(cat ${2} | \
    sed -re 's/([()/])/.?\\\1/g' | sed --null-data 's/\n$//' | tr '\n' '|')"
outfile="$3"

cat ${infile} | sed -re "/${superfluous_regex}/d" > ${outfile}
