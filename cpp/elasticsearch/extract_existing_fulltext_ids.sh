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
    error=$(CheckError $response)
    if [[ $(echo -n "$error" | wc --chars) -ne 0 ]]; then
        echo "An error occurred in Elasticsearch" >&2
        echo "$error" >&2
        exit 1
    fi
}


function CheckError() {
    response=$@
    echo $response | jq  'select(.error) | .'
}


if [ $# != 1 ]; then
    echo "usage: $0 output_file"
    echo "       Extract PPNs of all fulltexts from Elasticsearch and write them to file"
    exit 1
fi

id_output_file=$1

echo "Checking write access to output file..."
>> "$id_output_file" || (echo "Cannot write to $id_output_file" && exit 1)

echo "Querying Elasticsearch..."
response=$(curl --fail --silent --request GET --header "Content-Type: application/json" $ES_HOST_AND_PORT/$ES_INDEX'/_search/?scroll=1m' --data '{ "_source": ["id"], "size" : 1000,
 "query" : {
    "constant_score" : {
        "filter" : {
          "bool" : {
            "must_not" : {
              "exists" : {
                 "field" : "expiration"
              }
            }
          }
        }
     }
   }
}')

echo "Processing Elasticsearch response..."
GetDownloadStats "$response"
scroll_id=$(GetScrollId "$response")
hit_count=$(GetHitCount "$response")

#First iteration
id_arrays=$(ObtainIds "$response")

# Continue until there are no further results
while [ "$hit_count" != "0" ]; do
    echo "Obtaining batch with "$hit_count" items"
    response=$(curl --fail --silent --request GET --header "Content-Type: application/json" $ES_HOST_AND_PORT'/_search/scroll' --data '{ "scroll" : "1m", "scroll_id": "'$scroll_id'" }')
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
tmp_extension=$(< /dev/urandom tr --delete --complement _A-Z-a-z-0-9 | head --bytes 6)
id_output_file_tmp="$id_output_file"."$tmp_extension"
echo $id_arrays | jq -s add | sed -e 's/[][", ]//g' | sed  '/^$/d' | sort | uniq > "$id_output_file_tmp"
mv --force "$id_output_file_tmp" "$id_output_file"
echo "Finished"
