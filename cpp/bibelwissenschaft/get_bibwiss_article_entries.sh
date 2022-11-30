#!/bin/bash
set -o errexit -o nounset


IDS_PER_BUNCH=50
WIBILEX_PPN="896670716"
WIRELEX_PPN="896670740"
IXTHEO_SERVER="ptah"
FT_SERVER="nu"

all=""

function QueryBunch {
ored_bunch=$(printf "%s\\n" $* | paste -sd '|')
    bunch_query=$(cat <<EOF
    {"_source": [ "id" ],
      "query": {
        "regexp": {
          "id": {
            "value": "$ored_bunch"
          }
        }
      }
    }
EOF
   )

curl --silent -XGET "http://${FT_SERVER}:9200/full_text_cache_urls/_search?size=${IDS_PER_BUNCH}" -H "kbn-xsrf: reporting" -H "Content-Type: application/json" -d $(printf "%s" $bunch_query)
}


function QueryRefWork {
   superior_ppn="$1"
   all_refwork_ppns=$(curl --silent \
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
