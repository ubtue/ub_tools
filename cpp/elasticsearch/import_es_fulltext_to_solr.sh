#!/bin/bash
set -e

ES_HOST="localhost"
ES_PORT="9200"
ES_INDEX="full_text_cache"
SOLR_HOST="localhost"
SOLR_PORT="8080"
SOLR_INDEX="biblio"


if [ $# != 1 ]; then
    echo "usage: $0 --all"
    echo "       $0 PPN"
    echo "       Extract fulltext from elasticsearch and import into solr"
    echo "       \"--all\" uses all entries in the elasticsearch full_text_cache"
    exit 1
fi


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

function get_scroll_id() {
   response=$@
   echo $response | jq -r ._scroll_id
}


function get_hit_count() {
   response=$@
   echo $response | jq -r '.hits.hits | length'
}


function upload_fulltext_to_solr() {
    response=$@
    echo $response | jq  '[.hits.hits[]."_source"]  | . as $t | group_by(.id) | map({id:.[0].id, fulltext : { set : (map(.full_text) | join(" ")) } } )' | \
        curl -s  http://$SOLR_HOST:$SOLR_PORT/solr/$SOLR_INDEX/update?commit=true -H "Content-Type: application/json" --data @-
}


function was_solr_successful() {
    response=$@
    status=$(echo $response | jq '.responseHeader.status')
    echo $status
}


function solr_response_handler() {
    response=$@
    solr_success=$(was_solr_successful "$solr_response")
    if [ "$solr_success" == "0" ]; then
         echo "Successfully imported to SOLR"
    else
         echo "Import to SOLR not successful"
         exit 1
    fi;
}


if [ "$1" = "--all" ]; then
    PPN=''
else
    PPN=$1
fi

# Handle potential chunking
# Extract the data from Elasticsearch, flatten it in case there are several chunks and transform it to a valid Solr-Fulltext field and load it up to Solr
response=$(curl -s -X GET -H "Content-Type: application/json" 'http://'$ES_HOST:$ES_PORT/$ES_INDEX'/_search/?scroll=1m' --data '{ "query":'"$(generate_match_query $PPN)"'} }')
scroll_id=$(get_scroll_id "$response")
hit_count=$(get_hit_count "$response")

#First iteration
solr_response=$(upload_fulltext_to_solr "$response")
solr_response_handler "$solr_response"

# Continue until there are no further results
while [ "$hit_count" != "0" ]; do
    response=$(curl -s -X GET -H "Content-Type: application/json" 'http://'$ES_HOST:$ES_PORT'/_search/scroll' --data '{ "scroll" : "1m", "scroll_id": "'$scroll_id'" }')
    scroll_id=$(get_scroll_id "$response")
    hit_count=$(get_hit_count "$response")
    solr_response=$(upload_fulltext_to_solr "$response")
done

echo "LEAVING"

