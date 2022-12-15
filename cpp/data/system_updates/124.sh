# Insert is_publisher provided also to the HTML cache
#!/bin/bash
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
                 "is_publisher_provided": {
                     "type": "boolean"
                 }
              }
           }  
EOF
   else
       echo "No full_text_cache_html_present"
   fi
else
    echo "No running ES instance - skipping schema update"
fi

