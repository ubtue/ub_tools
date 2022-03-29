#!/bin/bash


if [ $# != 1 ]; then
   echo "Usage: $0 keibi_marc_file"
   exit 1
fi


KEIBI_MARC_FILE=$1
SOLR_BASE_URL='http://134.2.66.64:8983/solr/biblio/select?'
KEIBI_WORK_KEYS_STR_MV_URL=${SOLR_BASE_URL}'fl=work_keys_str_mv&q=id%3A'


keibi_ppns=$(marc_grep ${KEIBI_MARC_FILE} '"001"' | awk -F':' '{print $1}')

for ppn in ${keibi_ppns}; do
    echo -n ${ppn}":"
    work_keys_str_mv=$(curl --silent ${KEIBI_WORK_KEYS_STR_MV_URL}${ppn} | jq '.response.docs[].work_keys_str_mv')
    query_string=$(php assemble_frbr_string.php ${ppn} "${work_keys_str_mv}")
    query_string="${SOLR_BASE_URL}fl=id&${query_string}"
    results=$(curl --silent $query_string | jq -r '.response.docs[].id')
    if [ -z "${results}" ]; then
       echo
    else
       for result in ${results}; do
          echo -n " "$result
       done
       echo
    fi
done
