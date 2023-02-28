#!/bin/bash
set -o errexit -o nounset


if [[ $# != 3 ]]; then
    echo "Usage $0: orig_titles.txt. web_titles.txt output.txt"
    exit 1
fi

orig_titles="$1"
web_titles="$2"
output="$3"

function RemoveTempFiles {
    rm ${tmpfile1} 
}

trap RemoveTempFiles EXIT
tmpfile1=$(mktemp -t asisociate_rgg4XXXXX.txt)
./associate_rgg4_titles ${orig_titles} ${web_titles} ${tmpfile1}
cat <(cat ${tmpfile1} | grep -v '|||' | sed -rn '/\w+:$/q;p') | \
    `#Replace double expressions to save names etc` \
    sed -re 's/(\b(\w+)\s+\2\b(.*))\s[|].*/\1 | \2\3/' #| \
    `#Escape ->`
# Get Multicandidates
cat <(cat ${tmpfile1} | sed -rn '/\w+:$/,$ p' | grep -v '||||')
# Get unassociated
cat  <(cat ${tmpfile1} | grep '||||') | \
    `#Replace double expressions, but replace strange space character before` \
    sed -re 's/ / /g' | \
    sed -re 's/(((\b\w+\b\s\b[^.]+\b)[.]?)\s+\3(.*))\s[|]+.*/\1 | \2/'
