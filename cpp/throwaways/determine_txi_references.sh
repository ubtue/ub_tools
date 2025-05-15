#!/bin/bash
# From the special list of Tpi authority records extract the PPN and determine the number of references the number of references in the IxTheo data
set -o errexit -o nounset

function Usage {
    echo "Usage: $0 --target=(Tpi|Tfi|Tbi|Tui|Tgi|Tsi) Txi-Sätze_ixtheo.txt server"
    exit 1
}

if [[ $# != 3 ]]; then
    Usage
fi


target="$1"
shift
if [ "${target:0:9}" != "--target=" ]; then
    Usage
fi
target=${target:9}

input="$1"
server="$2"

read -r -d '' tpi_query_script <<'EOF' || true
   function GetTitleByExpression {
      echo $(urlencode "$(printf "author_id:%s OR author2_id:%s OR author_corporate:%s" $1 $1 $1)")
   }

   function GetTitleAboutExpression {
      echo $(printf "topic_id:%s" $1)
   }

   function GetHitNum {
      request="$1"
      echo $(curl --fail -s "${request}" | jq '.response.numFound')
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



read -r -d '' topic_query_script <<'EOF' || true
   function GetTopicExpression {
       echo $(printf "topic_id:%s" $1)
   }


   function GetHitNum {
      request="$1"
      echo $(curl --fail -s "${request}" | jq '.response.numFound')
   }

   server="$1"
   shift
   authority_ppn="$1"
   base_query="${server}:8983/solr/biblio/select?q="
   topic_count=$(GetHitNum "${base_query}$(GetTopicExpression ${authority_ppn})")
   echo ${authority_ppn},${topic_count}
EOF

case "$target" in
    Tpi) query_script=${tpi_query_script};;
    Tfi) query_script=${topic_query_script};;
    Tbi) query_script=${topic_query_script};;
    Tui) query_script=${topic_query_script};;
    Tgi) query_script=${topic_query_script};;
    Tsi) query_script=${topic_query_script};;
    *) echo "Invalid target \"${target}\""; exit 1;;
esac

cat ${input} | awk -F';' '{print $(NF-1)}' | sed -re 's/^\s*797\s*//' | \
    xargs --max-args 1 -I'{}' bash -c "${query_script}" _  ${server} '{}'
