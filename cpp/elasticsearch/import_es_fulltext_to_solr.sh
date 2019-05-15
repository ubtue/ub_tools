#!/bin/bash
set -e

ES_HOST="localhost"
ES_PORT="9200"
ES_INDEX="full_text_cache"
SOLR_HOST="localhost"
SOLR_PORT="8080"
SOLR_INDEX="biblio"

function generate_match_query() {
if [ $# == 1 ]; then
PPN=$1
    cat <<EOF
{ "match": { "id" :"$PPN"} }
EOF
else
  cat <<EOF
{ "match_all": {} }
EOF
fi
}

if [ $# != 1 ]; then
    echo "usage: $0 --all"
    echo "       $0 PPN"
    echo "       Extract fulltext from elasticsearch and import into solr"
    echo "       \"--all\" uses all entries in the elasticsearch full_text_cache"
    exit 1
fi

if [ "$1" = "--all" ]; then
    PPN=''
else
    PPN=$1
fi

# We still handle potential chunking 
# Extract the data from Elasticsearch, flatten it in case there are several chunks and tranform it to a valid Solr-Fulltext field and load it up to Solr
curl -s -X GET -H "Content-Type: application/json" http://$ES_HOST:$ES_PORT/$ES_INDEX/_search/ --data '{ "query":'"$(generate_match_query $PPN)"'} }' | \
jq  '[.hits.hits[]."_source"]  | . as $t | group_by(.id) | map({id:.[0].id, fulltext : { set : (map(.full_text) | join(" ")) } } )' | \
curl http://$SOLR_HOST:$SOLR_PORT/solr/$SOLR_INDEX/update?commit=true -H "Content-Type: application/json" --data @-



