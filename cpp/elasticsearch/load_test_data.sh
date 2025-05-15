#!/bin/bash
set -e


host_and_port=$(inifile_lookup /usr/local/var/lib/tuelib/Elasticsearch.conf Elasticsearch host)


curl --fail --header "Content-Type: application/json" --request POST "${host_and_port}/full_text_cache/_doc" \
     --data '{ "id": "a1", "full_text": "blah blah blah", "expiration": "2019-02-01" }'
curl --fail --header "Content-Type: application/json" --request POST "${host_and_port}/full_text_cache/_doc" \
     --data '{ "id": "a2", "full_text": "yo, what'"'"'s up?" }'
echo
