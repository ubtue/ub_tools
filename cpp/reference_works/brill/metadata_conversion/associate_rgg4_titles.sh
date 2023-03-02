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
    rm ${tmpfile1} ${tmpfile2} ${tmpfile3}
}

trap RemoveTempFiles EXIT
tmpfile1=$(mktemp -t associate_rgg4XXXXX.txt)
tmpfile2=$(mktemp -t associate_rgg4XXXXX.txt)
tmpfile3=$(mktemp -t associate_rgg4XXXXX.txt)
./associate_rgg4_titles ${orig_titles} ${web_titles} ${tmpfile1}
cat <(cat ${tmpfile1} | grep -v '|||' | sed -rn '/\w+:$/q;p') | \
    `#Replace double expressions to save names etc` \
    sed -re 's/(\b(\w+)\s+\2\b(.*))\s[|].*/\1 | \2\3/' | \
    `#Escape ->` \
    sed -re 's/->/\\-\\>/g' | \
    sed -re 's/ [|] /->/' \
    > ${tmpfile2}

# Get multicandidates
cat <(cat ${tmpfile1} | sed -rn '/\w+:$/,$ p' | grep -v '||||') > ${output%.txt}_multicandidates.txt
# Get unassociated
cat  <(cat ${tmpfile1} | grep '||||') | \
    `#Replace double expressions, but replace strange space character before` \
    sed -re 's/ / /g' | \
    sed -re 's/(((\b\w+\b\s\b[^.]+\b)[.]?)\s+\3(.*))\s[|]+.*/\1 | \2/' | \
    sed -re 's/ [|] /->/' | \
    tee ${tmpfile3} | \
    grep -v '||||' >> ${tmpfile2}

    cat ${tmpfile2} | \
    sed -re 's/[(]/\\(/g' | \
    sed -re 's/[)]/\\)/g' | \
    sed -re 's/[[]/\\[/g' | \
    sed -re 's/[]]/\\]/g' \
    > ${output}

cat ${tmpfile3} | grep '||||' > ${output%.txt}_unassociated.txt
