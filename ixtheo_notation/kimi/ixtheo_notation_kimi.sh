#!/bin/bash


if [ $# != 3 ]; then
    echo "Usage: $0 solr_server es_server ppn"
    exit 1;
fi

solr_server=$1
es_server=$2
ppn=$3

function GetRecord {
   echo $(/usr/local/bin/get_marc21_for_ppn_from_solr.sh --omit-local-data ${solr_server} $ppn | \
          marc_grep --input-format=marc-21 /dev/stdin 'if "001"==".*" extract *' traditional | grep -v '^[A-Z]')
}

function GetFulltextCategories {
     solr_server=$1
     ppn=$2
    ./get_admissible_fulltext_types.sh ${solr_server} ${ppn} | \
      jq 'def map_table: { "Fulltext" : "fulltext", "Table of Contents" : "toc" };
           map( . as $x | map_table[$x] // $x)' | jq -r '.[]'
}

function GetAdmissableFulltexts {
     for ft_type in $(GetFulltextCategories ${solr_server} $ppn); do
        local outfile=$(mktemp -u)
        ../download_fulltext_from_es_for_ppn.sh ${es_server} ${ppn} ${outfile} ${ft_type}
        cat ${outfile}
        rm ${outfile}
     done
}

ixtheo_notation_prompt=./ixtheo_notation_prompt.txt

#fulltexts_complete=$(for i in $(GetFulltextCategories ${solr_server} $ppn); do echo $i; done)

record_information=$(cat <(GetRecord) <(GetAdmissableFulltexts))
#record_information=$(cat <(GetRecord))
#record_information=$(cat  <(GetAdmissableFulltexts))

together_json=$(cat <<EOF
 {
    "model": "moonshotai/Kimi-K2-Thinking",
    "messages": [
      {
        "role": "system",
        "content": $(cat $ixtheo_notation_prompt | jq -Rs)
      },
      {
        "role": "user",
	"content": $(echo $record_information | jq -Rs)
      }
    ],
    "stream": false,
    "max_tokens": 50000,
    "temperature": 0
  }
EOF
)

curl -X POST "https://api.together.xyz/v1/chat/completions" \
 -H "Authorization: Bearer $TOGETHER_API_KEY" \
 -H "Content-Type: application/json" \
 -d @<(echo $together_json) |  \
jq '.choices[].message | {content} + {reasoning}' | \
awk -F '```json|```' '{print $2}' | \
sed -re 's/\\[nt]//g' | grep -v '^$' | \
sed -re 's/(.*)/"\1"/' | jq -ra fromjson
# cat <(echo $together_json)
