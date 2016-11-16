#!/bin/bash
# Use curl to query all matching ID's for a single refterm from the temporary Solr instance

if [ $# != 3 ]; then
    echo "usage: $0 outputdir refterm query_string"
    exit 1;
fi

curl_base_string='http://localhost:8081/solr/biblio/select?fl=id&wt=csv&rows=1000000'

outputdir="$1"
refterm="$2"
query_string="$3"
outputfile="${outputdir}/${refterm}.ids"

# Query Solr using csv writer to avoid overhard, strip the first line that contains the field names, strip possible trailing characters and redirect 
# the id list to a file named after the original refterm
curl --silent --get --data-urlencode "q=${query_string}" ${curl_base_string} | tail  -n +2 | sed -e 's/,$//' > "${outputfile}"
