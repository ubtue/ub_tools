#!/bin/bash
set -e


#host_and_port=$(inifile_lookup /usr/local/var/lib/tuelib/Elasticsearch.conf Elasticsearch host)
host_and_port='https://localhost:9200'



# Create the indices and types:
for schema in *_schema.json; do
    index="${schema%_schema.json}"
    curl -v -k -u 'elastic:5ygLJIa7nEpy_N67SFd+' --request PUT --header 'Content-Type: application/json' "${host_and_port}/${index}" --data @"$schema"
    echo
done
