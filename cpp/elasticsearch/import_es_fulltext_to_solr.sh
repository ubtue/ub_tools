#!/bin/bash
set -e

ES_HOST="localhost"
ES_PORT="9200"
ES_INDEX="full_text_cache"
SOLR_HOST="localhost"
SOLR_PORT="8080"
SOLR_INDEX="biblio"

if [ $# != 1 ]; then
    echo "usage: $0 PPN"
    exit 1
fi

PPN=$1

# We still handle potential chunking 
# Extract the data from Elasticsearch, flatten it in case there are several chunks and tranform it to a valid Solr-Fulltext field and load it up to Solr

curl -s -X GET -H "Content-Type: application/json" http://$ES_HOST:$ES_PORT/$ES_INDEX/_search/ --data '{ "query":{ "match": { "id" :"'$PPN'"} } }' | \
jq  '[.hits.hits[]."_source"]  | . as $t | group_by(.id) | map({id:.[0].id, fulltext : { set : (map(.full_text) | join(" ")) } } )' | \
curl http://$SOLR_HOST:$SOLR_PORT/solr/$SOLR_INDEX/update?commit=true -H "Content-Type: application/json" --data @-



