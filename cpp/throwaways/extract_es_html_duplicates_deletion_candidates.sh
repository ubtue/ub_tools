#!/bin/bash

if [ $# != 1 ]; then
    echo "Usage: $0 target_host"
    exit 1
fi

target_host="$1"

read -r -d '' ppn_and_pages <<EOF
{ "size": "0",
  "query":{"match_all":{}},
  "aggs": {
     "ids":
       { "multi_terms" :
           {"terms":
               [ {"field": "id"},
                 {"field": "page" },
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

curl --fail -s -H "Content-Type: application/json" -X POST http://${target_host}:9200/full_text_cache_html/_search -d "${ppn_and_pages}" | \
jq -r .aggregations.ids.buckets[].key_as_string | \
xargs -I'{}' bash -c 'ppn_and_page=(${@//|/ }); echo ${ppn_and_page[0]} ${ppn_and_page[1]} ${ppn_and_page[2]}' _ '{}'  | \
xargs -I'{}' bash -c '
declare -a args=($2)
read -r -d '"'"''"'"' get_es_ids<<EOF
{ "size":"10000",
  "sort":[{"id":"asc"}],
  "_source":[ "id", "text_type", "page" ],
    "query" : {
       "bool" : { \
          "must" : [
            { "match" :  { "id" : "${args[0]}" } },
            { "match": {"page" : "${args[1]}" } },
            { "match": {"text_type" : "${args[2]}" } }
          ]
       }
    }
}
EOF
curl --fail -s -H "Content-Type: application/json" -X POST -d  "${get_es_ids}" \
http://$1:9200/full_text_cache_html/_search'  _ "${target_host}" '{}' | \
jq -r '.hits.hits[]| ._id, ._source.id, ._source.page, ._source.text_type'
