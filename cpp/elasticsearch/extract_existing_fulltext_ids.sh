#!/bin/bash
set -o errexit -o nounset


ES_CONFIG_FILE="/usr/local/var/lib/tuelib/Elasticsearch.conf"
ES_HOST_AND_PORT=$(inifile_lookup $ES_CONFIG_FILE Elasticsearch host)
ES_INDEX=$(inifile_lookup $ES_CONFIG_FILE Elasticsearch index)

function GetScrollId() {
   response=$@
   echo $response | jq -r ._scroll_id
}


function GetHitCount() {
   response=$@
   echo $response | jq -r '.hits.hits | length'
}


function ObtainIds() {
    response=$@
    echo $response | jq  '[.hits.hits[]."_source"]  | . as $t | group_by(.id) | map(.[0].id)' 
}


function GetDownloadStats() {
    response=$@
    echo "------------------------------------" >&2
    echo -n "Received " >&2
    echo -n $(echo "$response" | wc -c) >&2
    echo " bytes" >&2
    echo "------------------------------------" >&2
}


if [ $# != 1 ]; then
    echo "usage: $0 output_file"
    echo "       Extract PPNs of all fulltexts from Elasticsearch and write them to file"
    exit 1
fi

id_output_file=$1

# Fail early if we cannot write the file
> $id_output_file

# Handle potential chunking
response=$(curl -s -X GET -H "Content-Type: application/json" $ES_HOST_AND_PORT/$ES_INDEX'/_search/?scroll=1m' --data '{ "_source": ["id"], "size" : 1000, "query":{ "match_all": {} } }')
GetDownloadStats "$response"
scroll_id=$(GetScrollId "$response")
hit_count=$(GetHitCount "$response")

#First iteration
id_arrays=$(ObtainIds "$response")

# Continue until there are no further results
while [ "$hit_count" != "0" ]; do
    echo "Obtaining batch with "$hit_count" items"
    response=$(curl -s -X GET -H "Content-Type: application/json" $ES_HOST_AND_PORT'/_search/scroll' --data '{ "scroll" : "1m", "scroll_id": "'$scroll_id'" }')
    GetDownloadStats "$response"
    scroll_id=$(GetScrollId "$response")
    hit_count=$(GetHitCount "$response")
    if [ "$hit_count" == "0" ]; then
        echo "Finished obtaining results set"
        break;
    fi
    id_arrays+=$(ObtainIds "$response")
done
echo "Flatten and deduplicate the scroll results and write them to file $id_output_file"
echo $id_arrays | jq -s add | sed -e 's/[][", ]//g' | sed  '/^$/d' | sort | uniq > "$id_output_file"
echo "Finished"
