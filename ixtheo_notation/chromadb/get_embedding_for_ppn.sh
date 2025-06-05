#!/bin/bash
set -o errexit -o nounset -o pipefail

function GetEmbeddingConfig {
    model=$(inifile_lookup ${conf_file} "Embedding" "model")
    api_key=$(inifile_lookup ${conf_file} "Embedding" "api_key")
    embedding_endpoint=$(inifile_lookup ${conf_file} "Embedding" \
                         "embedding_endpoint")
}

function GetSolrConfig {
    solr_server=$(inifile_lookup ${conf_file} "Solr" "server")
    solr_port=$(inifile_lookup ${conf_file} "Solr" "port")
}

function GetRecordData {
    record_data=$(curl --silent "http://${solr_server}:${solr_port}/solr/biblio/select?fl=*&q.op=OR&q=id%3A${ppn}" | \
              jq '.response.docs[] | { id, title_full, author, topic_standardized, topic_non_standardized,
                 ixtheo_notation, era_facet, topic_facet, "summary" : .fullrecord | fromjson | .fields[]
                 | to_entries[] | select(.key=="520") | .value.subfields[].a }' | \
              jq 'tojson | {"model" : "'"${model}"'", "input": .}')

    echo "${record_data}"
}


function GetEmbedding {
    embedding=$(curl --silent --request POST --url ${embedding_endpoint}  --header "Authorization: Bearer ${api_key}" \
        --header 'accept: application/json' --header 'content-type: application/json' \
        --data "@"<(echo "${record_data}"))

    echo "${embedding}"
}

if [ $# < 1 ]; then
    echo "Usage: $0 ppns"
    exit 1
fi

conf_file=$(basename $0 | cut -d. -f1).ini
GetEmbeddingConfig
GetSolrConfig

for ppn in "$@"; do
   echo $ppn
   GetRecordData
   GetEmbedding
done

