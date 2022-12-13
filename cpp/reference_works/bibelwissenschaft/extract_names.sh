#!/bin/bash

if [[ $# != 2 ]]; then
    echo "Usage $0 in_marc_file outfile_prefix"
    exit 1
fi

MARC_FILE=$1
OUTFILE_PREFIX=$2

for bibwiss_type in "WiReLex" "WiBiLex"; do
    marc_grep $1 'if "TYP"=="'${bibwiss_type}'" extract "856u"' traditional | \
    awk '{print $2}' | \
    xargs -I '{}' /bin/bash -c $'
        interval=$((1 + $RANDOM % 3)); echo "Sleeping ${interval} seconds" >2; sleep ${interval};
        ./translate_url_multiple "$1" | \
        jq -r \'.[] | [ if ((.tags | length) != 0) then "Reference" else empty end, if ((.creators | length) != 0) then "Author" else empty end, .url, .title, .tags[].tag, (.creators[] | del(.creatorType) | flatten | reverse |join(","))] | to_entries | [.[].value] | @csv\'' _ '{}' \
    > ${OUTFILE_PREFIX}_${bibwiss_type}.csv
done

