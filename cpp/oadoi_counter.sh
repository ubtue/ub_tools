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
while IFS='' read -r doi || [[ -n "$doi" ]]; do
    if [ ! -z "$doi" ]; then
        echo "Processing $doi"
        oa_color=$(curl 'https://api.oadoi.org/'$doi"?email=$EMAIL" --silent --output - \
                           | jq --monochrome-output '.results[0].oa_color')
        ((++total))
        if [[ $oa_color != "null" ]]; then
            ((++found))
        fi
    fi
done < $1

echo "Found $found of $total objects."
