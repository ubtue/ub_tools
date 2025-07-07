#!/bin/bash

if [ $# != 1 ]; then
    echo "Usage: $0 samples.jsonl"
    exit 1
fi

input_file="$1"

cat ${input_file} | jq -s 'del(.[] | select(.non_preferred_output[].content == "[]"))' | jq -c '.[]'
