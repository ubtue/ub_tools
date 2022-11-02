#!/bin/bash
# Associate the PPNs in the given marc file to other records given in 
# SOLR_BASE_URL using the VuFind FRBR-Functionality


if [ $# != 1 ]; then
   echo "Usage: $0 repo_marc_file"
   exit 1
fi


REPO_MARC_FILE=$1
SOLR_BASE_URL='http://ptah:8983/solr/biblio/select?'
REPO_WORK_KEYS_STR_MV_URL=${SOLR_BASE_URL}'fl=work_keys_str_mv&q=id%3A'
REPO_OA_LINKS=${SOLR_BASE_URL}'fl=urls_and_material_types&q=id%3A'


repo_ppns=$(marc_grep ${REPO_MARC_FILE} '"001"' | awk -F':' '{print $1}')

for ppn in ${repo_ppns}; do
    echo -n ${ppn}" "
    oa_link=
    work_keys_str_mv=$(curl --silent ${REPO_WORK_KEYS_STR_MV_URL}${ppn} | jq '.response.docs[].work_keys_str_mv')
    # Fixme: cannot determine OA status from URL-field
    oa_link=$(curl --silent ${REPO_OA_LINKS}${ppn} | jq -r '.response.docs[].urls_and_material_types[] | select(. | index("kostenfrei"))' 2>/dev/null | sed 's#:[^:]*$##')
    echo -n ${oa_link}
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
