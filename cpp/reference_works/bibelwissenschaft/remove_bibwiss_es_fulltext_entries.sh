#!/bin/bash
#Delete corresponding fulltext entries in the ES full text indices for all articles of WiBILex and WiReLex
set -o errexit -o nounset


IDS_PER_BUNCH=50
WIBILEX_PPN="896670716"
WIRELEX_PPN="896670740"
IXTHEO_SERVER="ptah"
FT_SERVER="localhost"

all=""

function QueryBunch {
ored_bunch=$(printf "%s\\n" $* | paste -sd '|')
    bunch_query=$(cat <<EOF
    {"query": {
        "regexp": {
          "id": {
            "value": "$ored_bunch"
          }
        }
      }
    }
EOF
   )

for index in "full_text_cache" "full_text_cache_urls" "full_text_cache_html"
do
   curl --fail --silent -XPOST "http://${FT_SERVER}:9200/${index}/_delete_by_query" -H "Content-Type: application/json" -d $(printf "%s" $bunch_query)
done
}


function QueryRefWork {
   superior_ppn="$1"
   all_refwork_ppns=$(curl --fail --silent \
    'http://'${IXTHEO_SERVER}':8983/solr/biblio/select?fl=id%2Ctitle&indent=true&q.op=OR&q=superior_ppn%3A'${superior_ppn}'&rows=10000&sort=title%20asc&start=0' \
    | jq .response.docs[].id | sed -r -e 's/"//g')

   i=0
   refwork_bunch=""
   for refwork_ppn in ${all_refwork_ppns}
   do
       if ((i != 0 && i % ${IDS_PER_BUNCH} == 0 )); then
           QueryBunch ${refwork_bunch}
           refwork_bunch=()
       fi
       refwork_bunch+=" "${refwork_ppn}
       ((i+=1))
   done

   #Handle remaining entries
   if [ -n "${refwork_bunch}" ]; then
           QueryBunch ${refwork_bunch}
   fi
}

QueryRefWork "${WIBILEX_PPN}"
QueryRefWork "${WIRELEX_PPN}"
