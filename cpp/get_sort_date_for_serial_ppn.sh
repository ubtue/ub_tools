#!/bin/bash
# Determine the sort date of a given serial PPN by finding inferior works
set -o errexit -o nounset

if [ $# -ne 2 ]; then
    echo "usage: $0 serial_ppn ppn_output_file"
    exit 1;
fi

serial_ppn="$1"
ppn_output_file="$2"

curl_base_string='http://localhost:8081/solr/biblio/select?fl=publishDateSort&wt=csv&rows=5&sort=publishDateSort+asc'

sort_year=$(curl --silent --get --data-urlencode "q=superior_ppn:${serial_ppn}"  ${curl_base_string} | tail  -n +2 | sed -e 's/,$//' | grep -v \"\" | head -n 1)

echo "${serial_ppn} : ${sort_year}" > ${ppn_output_file}
