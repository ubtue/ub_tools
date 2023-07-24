#!/bin/bash

if [[ $# != 3 ]]; then
    echo "Usage: $0 Gesamttiteldaten.mrc solr_host outfile"
    exit 1
fi

marc_input="$1"
solr_host="$2"
superiors_without_ssgn="$3"


comm -23 \
<(curl --silent 'http://'${solr_host}':8983/solr/biblio/select?fl=id&indent=true&q.op=OR&q=is_superior_work%3Atrue&rows=1000000&wt=csv' |  tail -n +2 | sed -re 's/,$//' | sort) \
<(marc_grep ${marc_input} 'if "0842"=="ssgn" extract "001"' no_label | sort) > ${superiors_without_ssgn}
