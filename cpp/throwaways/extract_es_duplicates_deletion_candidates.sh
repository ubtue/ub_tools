#!/bin/bash

if [ $# != 1 ]; then
    echo "Usage: $0 target_host"
    exit 1
fi

target_host="$1"

read -r -d '' ppn_and_text_type <<EOF
{ "size": "0",
  "query":{"match_all":{}},
  "aggs": {
     "ids":
       { "multi_terms" :
           {"terms":
               [ {"field": "id"},
                 {"field": "text_type" }
               ],
            "size":10000,
            "min_doc_count":5
           }
       }
  }
}
EOF

# Create aggregations first how many arguments are there how many times, extract PPN and page number and determine the internal
# id

curl -s -H "Content-Type: application/json" -X POST http://${target_host}:9200/full_text_cache/_search -d "${ppn_and_text_type}" | \
jq -r .aggregations.ids.buckets[].key_as_string | \
xargs -I'{}' bash -c 'ppn_and_text_type=(${@//|/ }); echo ${ppn_and_text_type[0]} ${ppn_and_text_type[1]}' _ '{}'  | \
xargs -I'{}' bash -c '
declare -a args=($2)
read -r -d '"'"''"'"' get_es_ids<<EOF
{ "size":"10000",
  "sort":[{"id":"asc"}],
  "_source":[ "id", "text_type" ],
    "query" : {
       "bool" : { \
          "must" : [
            { "match" :  { "id" : "${args[0]}" } },
            { "match": {"text_type" : "${args[1]}" } }
          ]
       }
    }
}
EOF
curl -s -H "Content-Type: application/json" -X POST -d  "${get_es_ids}" \
http://$1:9200/full_text_cache/_search'  _ "${target_host}" '{}' | \
jq -r '.hits.hits[]| ._id, ._source.id, ._source.text_type'
