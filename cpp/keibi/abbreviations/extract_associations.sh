#!/bin/bash

if [ $# != 2 ]; then
    echo "Usage: $0 lookup_map_file association_dir"
    exit 1
fi

LOOKUP_MAP_FILE=${1}
ASSOCIATION_DIR=${2}
MINSIZE=100

for file in $(ls -1 --directory ${ASSOCIATION_DIR}/*); do
    filesize=$(stat -c%s "${file}")
    filename=$(basename ${file})
    if (( filesize < MINSIZE )); then
        echo ${filename} smaller than ${MINSIZE}: Skipping >&2
        continue
    fi
    # Filter out KEIBI Candidates
    ppn_candidates=$(cat ${file} |  jq -r '.records[] | select (.id | test("^[^K]")) | .id' |
                     paste -sd ,)
    abbreviation=${filename%.txt}
    echo $(cat ${LOOKUP_MAP_FILE} | grep ^${abbreviation}'|') : ${ppn_candidates}
done
