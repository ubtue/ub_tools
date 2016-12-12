#!/bin/bash
# Determine the sort date of a given serial PPN by finding inferior works
set -o errexit -o nounset

if [ $# -ne 2 ]; then
    echo "usage: $0 serial_ppn append_list"
    exit 1;
fi

serial_ppn="$1"
append_list="$2"

curl_base_string='http://localhost:8081/solr/biblio/select?fl=publishDateSort&wt=csv&rows=1&sort=publishDateSort+asc'

sort_year=$(curl --silent --get --data-urlencode "q=superior_ppn:${serial_ppn}"  ${curl_base_string} | tail  -n +2 | sed -e 's/,$//')

echo "${serial_ppn} : ${sort_year}" >> ${append_list}
