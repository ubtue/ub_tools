#!/bin/bash
set -o errexit -o nounset -o pipefail

function GetPIT {
   server="$1"
   echo $(curl --silent -X POST "${server}:9200/full_text_cache/_pit?keep_alive=30m" | jq .id)
}


function GetCredentialRequest {
    set +o nounset
    CREDENTIAL_REQUEST=""
    if [[ -v $API_KEY ]]; then CREDENTIAL_REQUEST="-H 'Authorization: ApiKey $API_KEY'"; fi
    echo "${CREDENTIAL_REQUEST}"
    set -o nounset
}


function GetSearchAfterID {
    local search_after_id=""
    if [[ $# != 0 ]]; then search_after_id="$1"; fi
    local search_after_part=""
    if [[ ${search_after_id} ]]; then search_after_part=',"search_after" : ["'${search_after_id}'"]'; fi
    echo "${search_after_part}"
}


function GetCodeForTextType {
    text_type="$1"
    declare -A text_type_to_int
    text_type_to_int['fulltext']=1
    text_type_to_int['summary']=8
    text_type_to_int['toc']=2

    if [ ! -v text_type_to_int[${text_type}] ]; then
        echo "Invalid text type \"${text_type}\""
        exit 1
    fi

    echo "\"${text_type_to_int[${text_type}]}\""
}

if [ $# != 3 ]; then
    echo "Usage $0: server outfile [fulltext|toc|summary]"
    exit 1
fi

SERVER="$1"
OUTFILE="$2"
TEXTTYPE="$3"

#Abort if text_type is invalid
GetCodeForTextType ${TEXTTYPE} 1>/dev/null

> $OUTFILE

last_search_id=""
curl_request=$(cat << EOM
curl --silent -XGET $(GetCredentialRequest) "http://${SERVER}:9200/_search" \
       -H "kbn-xsrf: reporting" -H "Content-Type: application/json" \
       -d  '{ "_source":["id","text_type","full_text"],"size":"9000","sort":[{"_shard_doc": "desc"}],
             "query":{"bool":{"must":[{"match":{"text_type": $(GetCodeForTextType ${TEXTTYPE})}}, \
                                      {"match" : {"id" : "1581948816"}}]}}, \
             "pit": { "id" : $(GetPIT ${SERVER})}
EOM
)


while true; do
    new_last_search_id=$(eval "${curl_request}$(GetSearchAfterID ${last_search_id})}'" \
       | jq '.hits.hits[]' | tee --append ${OUTFILE} \
       | jq ' .sort[]' | tail -n 1)

    echo "Last search ID: ${new_last_search_id}"
    if [[ -z ${new_last_search_id} ]]; then
        break;
    fi
    echo "Continuing..."
    last_search_id=${new_last_search_id}
done
