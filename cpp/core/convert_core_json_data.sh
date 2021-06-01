#!/bin/bash

# bash script used to unify arrays in the JSON data files we get from CORE
# in order to use them as input for convert_json_to_mrc
if [[ ! "$#" == 1 ]]; then
    echo "Usage: $0 json_file_name"
    exit 1
fi 
readonly JSON_FILE=$1
python -m json.tool $JSON_INPUT_FILE > $JSON_OUTPUT_FILE
sed --in-place ':a;N;$!ba;s/}\n\s*],\n\s*"status": "OK",\n\s*"totalHits": 140891\n\s*},\n\s*{\n\s*"data": \[\n/},\n/g' $JSON_FILE
