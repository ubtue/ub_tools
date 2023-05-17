#!/bin/bash

if [ $# != 2 ]; then
    echo "Usage $0: ezw.csv marc_out"
    exit 1
fi

csv="$1"
outfile="$2"
stripped_csv=$(mktemp ${csv%.csv}_XXXX.csv)

function CleanUp {
   rm ${stripped_csv}
}

trap CleanUp EXIT

cat ${csv} | sed -r -e 's/,+$//' > ${stripped_csv}
convert_ezw_to_marc ${stripped_csv} ${outfile}

