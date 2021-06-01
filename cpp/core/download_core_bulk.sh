#!/bin/bash

if [ $# != 1 ]; then
    echo "Usage: $0 output_file (Must end in .json!)"
    exit 1
fi


readonly output_file=$1
if [[ ${output_file##*.} != "json" ]]; then
    echo "$output_file does not end in .json"'!'
    exit 1
fi
> "$output_file"


readonly query_file=query_file.$$


function CreateQueryFile {
    declare -r -i offset=$1
    declare -r -i pages_per_bulk=20
    echo "[" > $query_file
    for i in $(seq $((${pages_per_bulk} * ${offset} + 1)) $(((${pages_per_bulk} * ${offset}) + ${pages_per_bulk}))); do
        cat >> $query_file << EOF
    {
        "page": $i, 
        "pageSize": 100, 
        "query": "criminolog*"
    },
EOF
    done
    sed --in-place '$s/,//g' ${query_file}
    echo "]" >> $query_file
}


declare -r API_KEY=$(< /usr/local/var/lib/tuelib/CORE-API.key)
declare -r CORE_API_URL=https://core.ac.uk/api-v2/articles/search
declare -r -i CHUNK_COUNT=11


for i in $(seq 0 $(($CHUNK_COUNT-1))); do
    CreateQueryFile $i
    curl --silent --header "Content-Type: application/json" --header "apiKey:${API_KEY}" \
         --request POST --data-binary @$query_file $CORE_API_URL >> "$output_file"
    sleep 15
done
