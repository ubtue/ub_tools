#!/bin/bash

if [ $# != 3 ]; then
    echo "Usage: $0 es_host full_text_cache_deletion_outfile_name full_text_cache_html_deletion_outfile_name"
    exit 1
fi

es_host="$1"
full_text_cache_outfile="$2"
full_text_cache_html_outfile="$3"

PATH=${PATH}:.

generate_es_duplicate_deletion_list.sh <(extract_es_duplicates_deletion_candidates.sh ${es_host}) \
    > ${full_text_cache_outfile}
generate_es_html_duplicate_deletion_list.sh <(extract_es_html_duplicates_deletion_candidates.sh ${es_host}) \
    > ${full_text_cache_html_outfile}
