#!/bin/bash

if [ $# != 3 ]; then
    echo "usage: $0 outputdir refterm query_string"
    exit 1;
fi

curl_base_string='http://localhost:8081/solr/biblio/select?fl=id&wt=csv'

outputdir=$1
refterm=$2
query_string=$3
outputfile=${outputdir}/${refterm}.ids

#Query solr using csv writer, strip the first line that contains the field names, strip the trailing and redirect to a file
curl --silent --get --data-urlencode "q=${query_string}" ${curl_base_string} | tail  -n +2 | sed -e 's/,$//' > "${outputfile}"
