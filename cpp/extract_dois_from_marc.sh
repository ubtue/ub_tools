#!/bin/bash
# Extract DOIs from MARC title file into output file
set -o errexit -o nounset

if [ $# != 2 ]; then
    echo Usage: $0 marc_directory output.txt
    exit 1
fi


function ExtractDOIs {
    marc_file=$1
    doi_output=$2
    echo "Extracting from ${marc_file} to ${doi_output}"
    marc_grep ${marc_file}  'if "0242" == "doi" extract "024a"' no_label > ${doi_output} 
}


marc_directory=$1
doi_output=$2

latest_post_pipeline_file=$(ls -t ${marc_directory}/GesamtTiteldaten-post-pipeline-* | head -1)
ExtractDOIs ${latest_post_pipeline_file} ${doi_output}
