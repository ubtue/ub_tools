#!/bin/bash
# Return FT and TOC for free and TOC for 

if [ $# != 2 ]; then
    echo "Usage $0 server ppn"
    exit 1
fi

server="$1"
ppn="$2"


curl -s "http://$server:8983/solr/biblio/select?fl=fulltext_types%2Chas_publisher_fulltext&indent=true&q.op=OR&q=id%3A$ppn&useParams=" | \
jq  '.response |
     .numFound == 0 // .docs[] |
       if .has_publisher_fulltext == true then (.fulltext_types // []) | 
          select(. == "Table of Contents") 
       else 
          (.fulltext_types // []) | 
             map(select(. == "Table of Contents" or . == "Fulltext" or . == "List of References"))
       end
     '
