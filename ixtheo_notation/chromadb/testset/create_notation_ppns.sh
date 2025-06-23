#!/bin/bash
if [[ ! -v SOLR_HOST_AND_PORT ]]; then
    echo "SOLR_HOST_AND_PORT is not set"
    exit 1
fi

for notation in $(./get_notations.sh); do
    echo "Obtaining \"${notation}\""
    curl --silent 'http://'${SOLR_HOST_AND_PORT}'/solr/biblio/select?fl=id&indent=true&q.op=OR&q=ixtheo_notation%3A'${notation}'&wt=json&rows=1000000' | jq -r '.response.docs[].id'  > ${notation}.txt
done
