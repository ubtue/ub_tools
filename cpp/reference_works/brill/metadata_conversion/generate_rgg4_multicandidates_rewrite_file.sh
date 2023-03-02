#!/bin/bash
set -o errexit -o nounset

if [[ $# != 3 ]]; then
    echo "Usage: $0 rgg4_multicandidates.txt rgg4_multicandidates_output.txt rgg4_manual_candidates.txt"
    exit 1
fi

input="$1"
output="$2"
manual_candidates="$3"

function RemoveTempFiles {
    rm ${tmpfile1}
}

trap RemoveTempFiles EXIT
tmpfile1=$(mktemp -t generate_rgg4_multicandidates_rewriteXXXXX.txt)

# Do a general |-separated rewriting
cat ${input} | sed -e '/^[^\t]/ d' | tr -d '\t'  | sed -re 's/(\b(\w+)\s+\2\b(.*))/\1 | \2\3/' \
    > ${tmpfile1}

# Write rewrite file
cat ${tmpfile1} | \
    grep ' [|] ' | \
    `#Escape ->` \
    sed -re 's/->/\\-\\>/g' | \
    sed -re 's/ [|] /->/' | \
    sed -re 's/[(]/\\(/g' | \
    sed -re 's/[)]/\\)/g' | \
    sed -re 's/[[]/\\[/g' | \
    sed -re 's/[]]/\\]/g' \
    > ${output}

# Write manual candidates file
cat ${tmpfile1} | \
    grep -v ' [|] ' | \
    `#Remove one word tokens => they don't need replacements` \
    grep -Ev '^\w+$' \
    > ${manual_candidates}
