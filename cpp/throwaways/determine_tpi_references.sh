#!/bin/bash
# From the special list of Tpi authority records extract the PPN and determine the number of references the number of references in the IxTheo data
set -o errexit -o nounset

if [[ $# != 2 ]]; then
    echo "Usage: $0 Tpi-Sätze_ixtheo.txt server"
    exit 1
fi

input="$1"
server="$2"

read -r -d '' query_script <<'EOF' || true
   function GetTitleByExpression {
      echo $(urlencode "$(printf "author_id:%s OR author2_id:%s OR author_corporate:%s" $1 $1 $1)")
   }

   function GetTitleAboutExpression {
      echo $(printf "topic_id:%s" $1)
   }

   function GetHitNum {
      request="$1"
      echo $(curl -s "${request}" | jq '.response.numFound')
   }

   server="$1"
   shift
   author_id="$1"
   title_by_query=$(GetTitleByExpression ${author_id})
   title_about_query=$(GetTitleAboutExpression ${author_id})
   base_query="${server}:8983/solr/biblio/select?q="
   title_by_count=$(GetHitNum "${base_query}${title_by_query}")
   title_about_count=$(GetHitNum "${base_query}${title_about_query}")
   echo ${author_id},${title_by_count},${title_about_count},$((${title_by_count}+${title_about_count}))
EOF

cat ${input} | awk -F';' '{print $(NF-1)}' | sed -re 's/^\s*797\s*//' | \
    xargs --max-args 1 -I'{}' bash -c "${query_script}" _  ${server} '{}'
