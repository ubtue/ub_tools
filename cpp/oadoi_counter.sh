#!/bin/bash
# The API we're using is documented at: https://oadoi.org/api
set -o errexit -o nounset

if [ $# -ne 1 ]; then
    echo "Usage: $0 doi_list"
    exit 1
fi

# Who gets the bad news?
EMAIL="johannes.ruscheinski@uni-tuebingen.de"

declare -i total=0
declare -i found=0
declare -A origin=()
while IFS='' read -r doi || [[ -n "$doi" ]]; do
    if [ ! -z "$doi" ]; then
        echo "Processing $doi"
        contents=$(curl 'https://api.oadoi.org/'"$doi?email=$EMAIL" --silent --output -)
        oa_color=$(echo $contents | jq --monochrome-output '.results[0].oa_color')
        echo $oa_color
        ((++total))
        if [[ $oa_color != "null" ]]; then
            ((++found))
            evidence=$(echo $contents | jq --monochrome-output '.results[0].evidence')
            ((++origin["${evidence}"]))
        fi
    fi
done < $1

for key in "${!origin[@]}"; do
    echo "$key : ${origin[$key]}"
done

echo "Found $found of $total objects."
