#!/bin/bash

if [ $# != 2 ]; then
    echo "Usage: $0 partial_dir output_file"
    exit 1
fi


partial_dir=${1}
output_file=${2}
API_KEY="XXXXX"

> ${output_file}

for query_file in $(ls -d1v $(realpath ${partial_dir})/*.json); do
    curl -s -g --header "Content-Type: application/json" -H "apiKey:${API_KEY}" --request POST --data-binary  @${query_file} 'https://core.ac.uk/api-v2/articles/search'  >> ${output_file}
    sleep 15
done
