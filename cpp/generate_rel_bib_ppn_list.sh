#!/bin/bash
set -o nounset

if [[ $# -ne 2 ]]; then
    echo "usage: $0 output_filename"
    exit 1
fi

wget -qO- 'http://localhost:8080/solr/biblio/select?q=is_religious_studies%3Atrue&rows=999999&fl=id&wt=csv&csv.header=false' | cut --bytes=1-9 > "$1"
