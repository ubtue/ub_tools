#!/bin/bash
# Get information about Studia patavina records already present in IxTheo
set -o errexit -o nounset -o pipefail


function GetSolrResults {
    local year_to_query="$1"
    local query="http://${host_and_port}/solr/biblio/select?fl=id%2Cauthor%2Ctitle%2Cpages%2Cissue%2Cvolume&fq=publishDate%3A%5B${year_to_query}%20TO%20${year_to_query}%5D&indent=true&q.op=OR&q=superior_ppn%3A166751685&rows=1000&wt=json"
   curl --fail --silent "${query}" | jq -r '.response.docs[] | "\(.id)|\(.title)|\(.volume) \(.pages)"'
}

if [ $# != 2 ]; then
    echo "Usage: $0 host_and_port year_or_year_range"
    exit 1
fi

host_and_port="$1"
year_or_year_range_to_query="$2"
lower_bound=$(echo ${year_or_year_range_to_query} | cut --delimiter='-' --field 1)
upper_bound=$(echo ${year_or_year_range_to_query} | cut --delimiter='-' --only-delimited --field 2)

if [ -z ${upper_bound} ]; then
    GetSolrResults ${lower_bound}
else
    for year_to_query in $(eval echo "{${lower_bound}..${upper_bound}}"); do
        GetSolrResults ${year_to_query}
    done
fi






