#!/bin/bash

# bash script used to unify arrays in the JSON data files we get from CORE
# in order to use them as input for convert_json_to_mrc
readonly JSON_INPUT_FILE=$1
readonly JSON_OUTPUT_FILE=$2
if [[ ! "$#" == 2 ]]; then
    echo "Usage: $0 json_input_file_name json_output_file_name"
    exit 1
fi 
if [ -t 0 ] && [ -e "$JSON_OUTPUT_FILE" ]; then
  echo "File $JSON_OUTPUT_FILE already exists!"
  read -p "Would you like to overwrite the File $JSON_OUTPUT_FILE ? [y/n] " -n 1 -r
  echo
  if [[ ! $REPLY =~ ^[Nn]$ ]]; then
    exit 1
  fi
fi
python -m json.tool $JSON_INPUT_FILE > $JSON_OUTPUT_FILE
sed -i ':a;N;$!ba;s/}\n\s*],\n\s*"status": "OK",\n\s*"totalHits": 140891\n\s*},\n\s*{\n\s*"data": \[\n/},\n/g' $JSON_OUTPUT_FILE
