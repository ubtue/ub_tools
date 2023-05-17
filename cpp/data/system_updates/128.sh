#!/bin/bash
# Insert provenance also to the HTML cache
set -o errexit -o nounset -o pipefail

echo "Trying to extend full_text_cache_html"
if systemctl is-active --quiet elasticsearch; then
   echo "Detected running elasticsearch instance"
   if [ -n $(curl -s 'localhost:9200/_cat/indices?v' | \
       awk '{print $3}' | grep -E '^full_text_cache_html$') ]; then
       curl --silent -XPUT localhost:9200/full_text_cache_html/_mapping \
                  -H 'Content-Type: application/json' --data-binary @- <<EOF
           {
             "properties": {
                 "is_converted_pdf": {
                     "type": "boolean"
                 }
              }
           }  
EOF
       curl --silent -XPOST localhost:9200/full_text_cache_html/_update_by_query \
           -H 'Content-Type: application/json' --data-binary @- <<EOF
        {
           "query": {
               "match_all": {}
        },
        "script" : "ctx._source.is_converted_pdf = 'true';"
        }
EOF

        

   else
       echo " $0: No full_text_cache_html_present"
   fi
else
    echo "No running ES instance - skipping schema update"
fi

