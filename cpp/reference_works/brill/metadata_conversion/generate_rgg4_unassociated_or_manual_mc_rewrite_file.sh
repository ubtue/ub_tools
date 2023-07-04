#!/bin/bash
set -o errexit -o nounset

if [[ $# != 2 ]]; then
    echo "Usage: $0 rgg4_(unassociated|multicandidates_manual).txt rgg4_(unassociated|multicandidates_manual)_rewrite.txt"
    exit 1
fi

input="$1"
output="$2"

cat ${input} | sed -re 's/ [|]{4} /|/' | \
   `#Escape ->` \
    sed -re 's/->/\\-\\>/g' | \
    sed -re 's/ [|] /->/' | \
    sed -re 's/[(]/\\(/g' | \
    sed -re 's/[)]/\\)/g' | \
    sed -re 's/[[]/\\[/g' | \
    sed -re 's/[]]/\\]/g' | \
    awk -F'|' '{print $1 "->" $1}' > ${output}
