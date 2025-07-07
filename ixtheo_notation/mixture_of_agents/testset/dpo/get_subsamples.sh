#/bin/bash

if [ $# != 2 ]; then
    echo "Usage $0 input.json sample_num"
    exit 1
fi

input="$1"
sample_num="$2"

cat ${input} | jq -s '. | flatten(1)' | \
    jq --argjson SAMPLE_NUM ${sample_num} '.[:$SAMPLE_NUM]'

