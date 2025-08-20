#!/bin/bash
set -e


host_and_port=$(inifile_lookup /usr/local/var/lib/tuelib/Elasticsearch.conf Elasticsearch host)


# Delete the indices:
for schema in *_schema.json; do
    index="${schema%_schema.json}"
    curl --fail --request DELETE --header 'Content-Type: application/json' "${host_and_port}/${index}"
    echo
done    
